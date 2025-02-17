////////////////////////////////////////////////////////////////////////////////
/// @brief replication initial data synchronizer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "InitialSyncer.h"

#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Basics/logging.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Indexes/Index.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/CollectionGuard.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::httpclient;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a binary search for the given key in the markers vector
////////////////////////////////////////////////////////////////////////////////

static bool BinarySearch (std::vector<TRI_df_marker_t const*> const& markers,
                          std::string const& key,
                          size_t& position) {
  TRI_ASSERT(! markers.empty());

  size_t l = 0;
  size_t r = markers.size() - 1;

  while (true) {
    // determine midpoint
    position = l + ((r - l) / 2);

    TRI_ASSERT(position < markers.size());
    char const* other = TRI_EXTRACT_MARKER_KEY(markers.at(position));

    int res = strcmp(key.c_str(), other);

    if (res == 0) {
      return true;
    }

    if (res < 0) {
      if (position == 0) {
        return false;
      }
      r = position - 1;
    }
    else {
      l = position + 1;
    }

    if (r < l) {
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a key range in the markers vector
////////////////////////////////////////////////////////////////////////////////

static bool FindRange (std::vector<TRI_df_marker_t const*> const& markers,
                       std::string const& lower,
                       std::string const& upper,
                       size_t& lowerPos,
                       size_t& upperPos) {
  bool found = false;

  if (! markers.empty()) {
    found = BinarySearch(markers, lower, lowerPos);

    if (found) {
      found = BinarySearch(markers, upper, upperPos);
    }
  }

  return found;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

size_t const InitialSyncer::MaxChunkSize = 10 * 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

InitialSyncer::InitialSyncer (TRI_vocbase_t* vocbase,
                              TRI_replication_applier_configuration_t const* configuration,
                              std::unordered_map<string, bool> const& restrictCollections,
                              string const& restrictType,
                              bool verbose) 
  : Syncer(vocbase, configuration),
    _progress("not started"),
    _restrictCollections(restrictCollections),
    _restrictType(restrictType),
    _processedCollections(),
    _batchId(0),
    _batchUpdateTime(0),
    _batchTtl(180),
    _includeSystem(false),
    _chunkSize(configuration->_chunkSize),
    _verbose(verbose),
    _hasFlushed(false) {

  if (_chunkSize == 0) {
    _chunkSize = (uint64_t) 2 * 1024 * 1024; // 2 mb
  }
  else if (_chunkSize < 128 * 1024) {
    _chunkSize = 128 * 1024;
  }

  _includeSystem = configuration->_includeSystem;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

InitialSyncer::~InitialSyncer () {
  if (_batchId > 0) {
    sendFinishBatch();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief run method, performs a full synchronization
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::run (string& errorMsg,
                        bool incremental) {
  if (_client == nullptr || 
      _connection == nullptr || 
      _endpoint == nullptr) {
    errorMsg = "invalid endpoint";

    return TRI_ERROR_INTERNAL;
  }

  int res = _vocbase->_replicationApplier->preventStart();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_DEFER(_vocbase->_replicationApplier->allowStart());

  setProgress("fetching master state");

  res = getMasterState(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (incremental) {
    res = sendFlush(errorMsg);
  
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  res = sendStartBatch(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  string url = BaseUrl + "/inventory?serverId=" + _localServerIdString;
  if (_includeSystem) {
    url += "&includeSystem=true";
  }

  // send request
  string const progress = "fetching master inventory from " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_GET,
                                                              url,
                                                              nullptr,
                                                              0));

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    sendFinishBatch();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();
  }
  else {
    std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str()));

    if (JsonHelper::isObject(json.get())) {
      res = handleInventoryResponse(json.get(), incremental, errorMsg);
    }
    else {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
        ": invalid JSON";
    }
  }

  sendFinishBatch();

  if (res != TRI_ERROR_NO_ERROR &&
      errorMsg.empty()) {
    errorMsg = TRI_errno_string(res);
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief send a WAL flush command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendFlush (std::string& errorMsg) {
  string const url = "/_admin/wal/flush";
  string const body = "{\"waitForSync\":true,\"waitForCollector\":true,\"waitForCollectorQueue\":true}";

  // send request
  string const progress = "send WAL flush command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_PUT,
                                                              url,
                                                              body.c_str(),
                                                              body.size()));

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    int res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();

    return res;
  }
    
  _hasFlushed = true;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "start batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendStartBatch (std::string& errorMsg) {
  _batchId = 0;

  std::string const url = BaseUrl + "/batch";
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  std::string const progress = "send batch start command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_POST,
                                                              url,
                                                              body.c_str(),
                                                              body.size()));

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();
  }
  else {
    std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str()));

    if (json == nullptr) {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
    else {
      std::string const id = JsonHelper::getStringValue(json.get(), "id", "");

      if (id.empty()) {
        res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }
      else {
        _batchId = StringUtils::uint64(id);
        _batchUpdateTime = TRI_microtime();
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an "extend batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendExtendBatch () {
  if (_batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  double now = TRI_microtime();

  if (now <= _batchUpdateTime + _batchTtl - 60) {
    // no need to extend the batch yet
    return TRI_ERROR_NO_ERROR;
  }

  string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId);
  string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  string const progress = "send batch start command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_PUT,
                                                              url,
                                                              body.c_str(),
                                                              body.size()));

  if (response == nullptr || ! response->isComplete()) {
    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  else {
    _batchUpdateTime = TRI_microtime();
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "finish batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendFinishBatch () {
  if (_batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId);

  // send request
  string const progress = "send batch finish command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_DELETE,
                                                              url,
                                                              nullptr,
                                                              0));

  if (response == nullptr || ! response->isComplete()) {
    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  else {
    _batchId = 0;
    _batchUpdateTime = 0;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::applyCollectionDump (TRI_transaction_collection_t* trxCollection,
                                        SimpleHttpResult* response,
                                        string& errorMsg) {

  const string invalidMsg = "received invalid JSON data for collection " +
                            StringUtils::itoa(trxCollection->_cid);

  StringBuffer& data = response->getBody();
  char* p = data.begin(); 
  char* end = p + data.length();

  // buffer must end with a NUL byte
  TRI_ASSERT(*end == '\0');

  while (p < end) {
    char* q = strchr(p, '\n');

    if (q == nullptr) {
      q = end;
    }
    
    if (q - p < 2) {
      // we are done
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(q <= end);
    *q = '\0';

    std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, p));
    
    p = q + 1;

    if (! JsonHelper::isObject(json.get())) {
      errorMsg = invalidMsg;

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_replication_operation_e type = REPLICATION_INVALID;
    char const* key       = nullptr;
    TRI_json_t const* doc = nullptr;
    TRI_voc_rid_t rid     = 0;

    auto objects = &(json.get()->_value._objects);
    size_t const n = TRI_LengthVector(objects);

    for (size_t i = 0; i < n; i += 2) {
      auto element = static_cast<TRI_json_t const*>(TRI_AtVector(objects, i));

      if (! JsonHelper::isString(element)) {
        errorMsg = invalidMsg;

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      char const* attributeName = element->_value._string.data;
      auto value = static_cast<TRI_json_t const*>(TRI_AtVector(objects, i + 1));

      if (TRI_EqualString(attributeName, "type")) {
        if (JsonHelper::isNumber(value)) {
          type = (TRI_replication_operation_e) (int) value->_value._number;
        }
      }

      else if (TRI_EqualString(attributeName, "key")) {
        if (JsonHelper::isString(value)) {
          key = value->_value._string.data;
        }
      }

      else if (TRI_EqualString(attributeName, "rev")) {
        if (JsonHelper::isString(value)) {
          rid = StringUtils::uint64(value->_value._string.data, value->_value._string.length - 1);
        }
      }

      else if (TRI_EqualString(attributeName, "data")) {
        if (JsonHelper::isObject(value)) {
          doc = value;
        }
      }
    }

    // key must not be 0, but doc can be 0!
    if (key == nullptr) {
      errorMsg = invalidMsg;

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    int res = applyCollectionDumpMarker(trxCollection, type, (const TRI_voc_key_t) key, rid, doc, errorMsg);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // reached the end      
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollectionDump (string const& cid,
                                         TRI_transaction_collection_t* trxCollection,
                                         string const& collectionName,
                                         TRI_voc_tick_t maxTick,
                                         string& errorMsg) {

  std::string appendix;

  if (_hasFlushed) {
    appendix = "&flush=false";
  }
  else {
    // only flush WAL once
    appendix = "&flush=true";
    _hasFlushed = true;
  }

  uint64_t chunkSize = _chunkSize;

  string const baseUrl = BaseUrl +
                         "/dump?collection=" + cid +
                         appendix;

  TRI_voc_tick_t fromTick = 0;
  int batch = 1;

  while (true) {
    sendExtendBatch();

    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    url += "&serverId=" + _localServerIdString;
    url += "&chunkSize=" + StringUtils::itoa(chunkSize);
  
    std::string const typeString = (trxCollection->_collection->_collection->_info._type == TRI_COL_TYPE_EDGE ? "edge" : "document");

    // send request
    string const progress = "fetching master collection dump for collection '" + collectionName +
                            "', type: " + typeString + ", id " + cid + ", batch " + StringUtils::itoa(batch);

    setProgress(progress.c_str());

    std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_GET,
                                                                url,
                                                                nullptr,
                                                                0));

    if (response == nullptr || ! response->isComplete()) {
      errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
                 ": " + _client->getErrorMessage();

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    TRI_ASSERT(response != nullptr);

    if (response->wasHttpError()) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                 ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
                 ": " + response->getHttpReturnMessage();

      return TRI_ERROR_REPLICATION_MASTER_ERROR;
    }

    int res = TRI_ERROR_NO_ERROR;  // Just to please the compiler
    bool checkMore = false;
    bool found;
    TRI_voc_tick_t tick;

    string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);
    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
        if (found) {
          tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          }
          else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (! found) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                 ": required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = applyCollectionDump(trxCollection, response.get(), errorMsg);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (! checkMore || fromTick == 0) {
      // done
      return res;
    }

    // increase chunk size for next fetch
    if (chunkSize < MaxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);
      if (chunkSize > MaxChunkSize) {
        chunkSize = MaxChunkSize;
      }
    }

    batch++;
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollectionSync (std::string const& cid,
                                         SingleCollectionWriteTransaction<UINT64_MAX>& trx,
                                         std::string const& collectionName,
                                         TRI_voc_tick_t maxTick,
                                         string& errorMsg) {

  string const baseUrl = BaseUrl + "/keys";
  string url = baseUrl + "?collection=" + cid + "&to=" + std::to_string(maxTick);
  
  std::string progress = "fetching collection keys from " + url;
  setProgress(progress);
   
  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_POST,
                                                              url,
                                                              nullptr,
                                                              0));

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  
  StringBuffer& data = response->getBody();

  // order collection keys
  std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.c_str()));

  if (! TRI_IsObjectJson(json.get())) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": response is no object";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_json_t const* idJson = TRI_LookupObjectJson(json.get(), "id");

  if (! TRI_IsStringJson(idJson)) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": response does not contain valid 'id' attribute";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  std::string const id(idJson->_value._string.data, idJson->_value._string.length - 1);
  
  auto shutdown = [&] () -> void { 
    url = baseUrl + "/" + id;
    string progress = "deleting remote collection keys object from " + url;
    setProgress(progress);

    // now delete the keys we ordered
    std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_DELETE,
                                                                url,
                                                                nullptr,
                                                                0));
  };

  TRI_DEFER(shutdown());
  

  TRI_json_t const* countJson = TRI_LookupObjectJson(json.get(), "count");

  if (! TRI_IsNumberJson(countJson)) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": response does not contain valid 'count' attribute";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (countJson->_value._number <= 0.0) {
    int res = trx.truncate(false);
 
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "unable to truncate collection '" + collectionName + "': " + TRI_errno_string(res);
 
      return res;
    }

    return TRI_ERROR_NO_ERROR;
  }


  // now we can fetch the complete chunk information from the master
  int res;

  try {
    res = handleSyncKeys(id, cid, trx, collectionName, maxTick, errorMsg);
  }
  catch (triagens::basics::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }
 
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleSyncKeys (std::string const& keysId,
                                   std::string const& cid,
                                   SingleCollectionWriteTransaction<UINT64_MAX>& trx,
                                   std::string const& collectionName,
                                   TRI_voc_tick_t maxTick,
                                   std::string& errorMsg) {

  TRI_doc_update_policy_t policy(TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr);
  auto shaper = trx.documentCollection()->getShaper();
          
  bool const isEdge = (trx.documentCollection()->_info._type == TRI_COL_TYPE_EDGE);
   
  string progress = "collecting local keys";
  setProgress(progress);

  // fetch all local keys from primary index
  std::vector<TRI_df_marker_t const*> markers;

  auto idx = trx.documentCollection()->primaryIndex();
  markers.reserve(idx->size());

  {
    triagens::basics::BucketPosition position;

    uint64_t total = 0;
    while (true) {
      auto ptr = idx->lookupSequential(position, total);

      if (ptr == nullptr) {
        // done
        break;
      }

      void const* marker = ptr->getDataPtr();
      auto df = static_cast<TRI_df_marker_t const*>(marker);
      markers.emplace_back(df);
    }

    string progress = "sorting " + std::to_string(markers.size()) + " local key(s)";
    setProgress(progress);
    
    // sort all our local keys
    std::sort(markers.begin(), markers.end(), [] (TRI_df_marker_t const* lhs, TRI_df_marker_t const* rhs) -> bool {
      int res = strcmp(TRI_EXTRACT_MARKER_KEY(lhs), TRI_EXTRACT_MARKER_KEY(rhs));

      return res < 0;
    });
  }

  std::vector<size_t> toFetch;

  TRI_voc_tick_t const chunkSize = 5000;
  string const baseUrl = BaseUrl + "/keys";
    
  string url = baseUrl + "/" + keysId + "?chunkSize=" + std::to_string(chunkSize);
  progress = "fetching remote keys chunks from " + url;
  setProgress(progress);
   
  std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_GET,
                                                              url,
                                                              nullptr,
                                                              0));

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  
  StringBuffer& data = response->getBody();

  // parse chunks
  std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.c_str()));

  if (! TRI_IsArrayJson(json.get())) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
                                  
  size_t const n = TRI_LengthArrayJson(json.get());
  
  // remove all keys that are below first remote key or beyond last remote key
  if (n > 0) {
    // first chunk
    auto chunk = static_cast<TRI_json_t const*>(TRI_AtVector(&(json.get()->_value._objects), 0));

    TRI_ASSERT(TRI_IsObjectJson(chunk));
    auto lowJson  = TRI_LookupObjectJson(chunk, "low");
    TRI_ASSERT(TRI_IsStringJson(lowJson));

    char const* lowKey = lowJson->_value._string.data;
    
    for (size_t i = 0; i < markers.size(); ++i) {
      auto key = TRI_EXTRACT_MARKER_KEY(markers[i]);

      if (strcmp(key, lowKey) >= 0) {
        break;
      }

      TRI_RemoveShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) key, 0, nullptr, &policy, false, false);
    }
    
    // last high
    chunk = static_cast<TRI_json_t const*>(TRI_AtVector(&(json.get()->_value._objects), n - 1));
    auto highJson = TRI_LookupObjectJson(chunk, "high");
    TRI_ASSERT(TRI_IsStringJson(highJson));

    char const* highKey = highJson->_value._string.data;

    for (size_t i = markers.size(); i >= 1; --i) {
      auto key = TRI_EXTRACT_MARKER_KEY(markers[i - 1]);
      
      if (strcmp(key, highKey) <= 0) {
        break;
      }

      TRI_RemoveShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) key, 0, nullptr, &policy, false, false);
    }
  }

  size_t nextStart = 0;

  // now process each chunk
  for (size_t i = 0; i < n; ++i) {
    size_t const currentChunkId = i;

    // read remote chunk
    auto chunk = static_cast<TRI_json_t const*>(TRI_AtVector(&(json.get()->_value._objects), i));

    if (! TRI_IsObjectJson(chunk)) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                 ": chunk is no object";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    auto lowJson  = TRI_LookupObjectJson(chunk, "low");
    auto highJson = TRI_LookupObjectJson(chunk, "high");
    auto hashJson = TRI_LookupObjectJson(chunk, "hash");
        
    //std::cout << "i: " << i << ", RANGE LOW: " << std::string(lowJson->_value._string.data) << ", HIGH: " << std::string(highJson->_value._string.data) << ", HASH: " << std::string(hashJson->_value._string.data) << "\n";
    if (! TRI_IsStringJson(lowJson) || ! TRI_IsStringJson(highJson) || ! TRI_IsStringJson(hashJson)) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                 ": chunks in response have an invalid format";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    size_t localFrom;
    size_t localTo;
    bool match = FindRange(markers, 
                           std::string(lowJson->_value._string.data, lowJson->_value._string.length - 1), 
                           std::string(highJson->_value._string.data, highJson->_value._string.length - 1),
                           localFrom,
                           localTo);

    if (match) {
      // now must hash the range
      uint64_t hash = 0x012345678;

      for (size_t i = localFrom; i <= localTo; ++i) {
        TRI_ASSERT(i < markers.size());
        auto marker = markers.at(i);
        char const* key = TRI_EXTRACT_MARKER_KEY(marker);

        hash ^= TRI_FnvHashString(key);
        hash ^= TRI_EXTRACT_MARKER_RID(marker);
      }
      
      if (std::to_string(hash) != std::string(hashJson->_value._string.data, hashJson->_value._string.length - 1)) {
        match = false;
      }
    }

    if (match) {
      // match
      nextStart = localTo + 1; 
    }
    else {
      // no match
      // must transfer keys for non-matching range
      std::string url = baseUrl + "/" + keysId + "?type=keys&chunk=" + std::to_string(i) + "&chunkSize=" + std::to_string(chunkSize);
      progress = "fetching keys from " + url;
      setProgress(progress);

      std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_PUT,
                                                                  url,
                                                                  nullptr,
                                                                  0));

      if (response == nullptr || ! response->isComplete()) {
        errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
                  ": " + _client->getErrorMessage();

        return TRI_ERROR_REPLICATION_NO_RESPONSE;
      }

      TRI_ASSERT(response != nullptr);

      if (response->wasHttpError()) {
        errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                  ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
                  ": " + response->getHttpReturnMessage();

        return TRI_ERROR_REPLICATION_MASTER_ERROR;
      }
      
      StringBuffer& rangeKeys = response->getBody();
  
      // parse keys
      std::unique_ptr<TRI_json_t> rangeKeysJson(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, rangeKeys.c_str()));

      if (! TRI_IsArrayJson(rangeKeysJson.get())) {
        errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                   ": response is no array";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }
                                  
      size_t const n = TRI_LengthArrayJson(rangeKeysJson.get());
      TRI_ASSERT(n > 0);
       
      // delete all keys at start of the range
      while (nextStart < markers.size()) {
        auto df = markers[nextStart];
        char const* localKey = TRI_EXTRACT_MARKER_KEY(df);
        int res = strcmp(localKey, lowJson->_value._string.data);

        if (res < 0) {
          // we have a local key that is not present remotely
          TRI_RemoveShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) localKey, 0, nullptr, &policy, false, false);
          ++nextStart;
        }
        else {
          break;
        }
      }
        
      toFetch.clear();

      for (size_t i = 0; i < n; ++i) {
        auto pair = static_cast<TRI_json_t const*>(TRI_AtVector(&(rangeKeysJson.get()->_value._objects), i));

        if (! TRI_IsArrayJson(pair) || TRI_LengthArrayJson(pair) != 2) {
          errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                     ": response key pair is no valid array";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }
      
        // key
        auto keyJson = static_cast<TRI_json_t const*>(TRI_AtVector(&pair->_value._objects, 0));

        if (! TRI_IsStringJson(keyJson)) {
          errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                     ": response key is no string";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }

        // rid
        if (markers.empty()) {
          // no local markers
          toFetch.emplace_back(i);
          continue;
        }
        
        while (nextStart < markers.size()) {
          auto df = markers[nextStart];
          char const* localKey = TRI_EXTRACT_MARKER_KEY(df);

          int res = strcmp(localKey, keyJson->_value._string.data);

          if (res < 0) {
            // we have a local key that is not present remotely
            TRI_RemoveShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) localKey, 0, nullptr, &policy, false, false);
            ++nextStart;
          }
          else if (res >= 0) {
            // key matches or key too high
            break;
          }
        }

        auto ridJson = static_cast<TRI_json_t const*>(TRI_AtVector(&chunk->_value._objects, 1));
        auto mptr = idx->lookupKey(keyJson->_value._string.data);

        if (mptr == nullptr) {
          // key not found locally
          toFetch.emplace_back(i);
        }
        else if (std::to_string(mptr->_rid) != std::string(ridJson->_value._string.data, ridJson->_value._string.length - 1)) {
          // key found, but rid differs
          toFetch.emplace_back(i);
          ++nextStart;
        }
        else {
          // a match - nothing to do!
          ++nextStart;
        }
      }
      
      // calculate next starting point
      if (! markers.empty()) {
        BinarySearch(markers, highJson->_value._string.data, nextStart);

        while (nextStart < markers.size()) {
          char const* key = TRI_EXTRACT_MARKER_KEY(markers.at(nextStart));
          int res = strcmp(key, highJson->_value._string.data);

          if (res <= 0) {
            ++nextStart;
          }
          else {
            break;
          }
        }
      }

      /*
      if (nextStart < markers.size()) {
        std::cout << "LOW: " << lowJson->_value._string.data << ", HIGH: " << highJson->_value._string.data << ", NEXT: " << TRI_EXTRACT_MARKER_KEY(markers.at(nextStart)) << "\n";
      }
      */

      if (! toFetch.empty()) {
        triagens::basics::Json keysJson(triagens::basics::Json::Array, toFetch.size());

        for (auto& it : toFetch) {
          keysJson.add(triagens::basics::Json(static_cast<double>(it)));
        }
      
        std::string url = baseUrl + "/" + keysId + "?type=docs&chunk=" + std::to_string(currentChunkId) + "&chunkSize=" + std::to_string(chunkSize);
        progress = "fetching documents from " + url;
        setProgress(progress);

        auto const keyJsonString = triagens::basics::JsonHelper::toString(keysJson.json());

        std::unique_ptr<SimpleHttpResult> response(_client->request(HttpRequest::HTTP_REQUEST_PUT,
                                                                    url,
                                                                    keyJsonString.c_str(),
                                                                    keyJsonString.size()));

        if (response == nullptr || ! response->isComplete()) {
          errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
                    ": " + _client->getErrorMessage();

          return TRI_ERROR_REPLICATION_NO_RESPONSE;
        }

        TRI_ASSERT(response != nullptr);

        if (response->wasHttpError()) {
          errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                    ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
                    ": " + response->getHttpReturnMessage();

          return TRI_ERROR_REPLICATION_MASTER_ERROR;
        }
      
        StringBuffer& documentsData = response->getBody();
      
        // parse keys
        std::unique_ptr<TRI_json_t> documentsJson(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, documentsData.c_str()));

        if (! TRI_IsArrayJson(documentsJson.get())) {
          errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                     ": response is no array";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }

        size_t const n = TRI_LengthArrayJson(documentsJson.get());

        for (size_t i = 0; i < n; ++i) {
          auto documentJson = static_cast<TRI_json_t const*>(TRI_AtVector(&(documentsJson.get()->_value._objects), i));

          if (! TRI_IsObjectJson(documentJson)) {
            errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                       ": document is no object";

            return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
          }

          auto const keyJson = TRI_LookupObjectJson(documentJson, "_key");

          if (! TRI_IsStringJson(keyJson)) {
            errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                       ": document key is invalid";

            return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
          }
          
          auto const revJson = TRI_LookupObjectJson(documentJson, "_rev");
          
          if (! TRI_IsStringJson(revJson)) {
            errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                       ": document revision is invalid";

            return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
          }

          std::string documentKey(keyJson->_value._string.data, keyJson->_value._string.length - 1);

          TRI_voc_rid_t rid = static_cast<TRI_voc_rid_t>(StringUtils::uint64(revJson->_value._string.data));

          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(shaper, documentJson, true);  // PROTECTED by trx 

          if (shaped == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          TRI_doc_mptr_copy_t result;

          int res = TRI_ERROR_NO_ERROR;
          auto mptr = idx->lookupKey(documentKey.c_str());

          if (mptr == nullptr || isEdge) {
            // in case of an edge collection we must always update
            TRI_document_edge_t* e = nullptr;
            TRI_document_edge_t edge;
            std::string from;
            std::string to;

            if (isEdge) {
              from = JsonHelper::getStringValue(documentJson, TRI_VOC_ATTRIBUTE_FROM, "");
              to   = JsonHelper::getStringValue(documentJson, TRI_VOC_ATTRIBUTE_TO, "");

              // parse _from
              if (! DocumentHelper::parseDocumentId(*trx.resolver(), from.c_str(), edge._fromCid, &edge._fromKey)) {
                res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
              }

              // parse _to
              if (! DocumentHelper::parseDocumentId(*trx.resolver(), to.c_str(), edge._toCid, &edge._toKey)) {
                res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
              }

              e = &edge;
            }

            // INSERT
            if (res == TRI_ERROR_NO_ERROR) {
              if (mptr != nullptr && isEdge) {
                // must remove existing edge first
                TRI_RemoveShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) documentKey.c_str(), 0, nullptr, &policy, false, false);
              }

              res = TRI_InsertShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) documentKey.c_str(), rid, nullptr, &result, shaped, e, false, false, true);
            }
          }
          else {
            // UPDATE
            res = TRI_UpdateShapedJsonDocumentCollection(trx.trxCollection(), (TRI_voc_key_t) documentKey.c_str(), rid, nullptr, &result, shaped, &policy, false, false);
          }
            
          TRI_FreeShapedJson(shaper->memoryZone(), shaped);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
        }
                                  
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the properties of a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::changeCollection (TRI_vocbase_col_t* col,
                                     TRI_json_t const* json) {

  bool waitForSync      = JsonHelper::getBooleanValue(json, "waitForSync", false);
  bool doCompact        = JsonHelper::getBooleanValue(json, "doCompact", true);
  int maximalSize       = JsonHelper::getNumericValue<int>(json, "maximalSize", TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE);
  uint32_t indexBuckets = JsonHelper::getNumericValue<uint32_t>(json, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);

  try {
    triagens::arango::CollectionGuard guard(_vocbase, col->_cid);

    TRI_col_info_t parameters;

    // only need to set these three properties as the others cannot be updated on the fly
    parameters._doCompact    = doCompact;
    parameters._maximalSize  = maximalSize;
    parameters._waitForSync  = waitForSync;
    parameters._indexBuckets = indexBuckets;

    bool doSync = _vocbase->_settings.forceSyncProperties;
    return TRI_UpdateCollectionInfo(_vocbase, guard.collection()->_collection, &parameters, doSync);
  }
  catch (triagens::basics::Exception const& ex) {
    return ex.code();
  }
  catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the information about a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollection (TRI_json_t const* parameters,
                                     TRI_json_t const* indexes,
                                     bool incremental,
                                     string& errorMsg,
                                     sync_phase_e phase) {

  sendExtendBatch();

  string const masterName = JsonHelper::getStringValue(parameters, "name", "");

  TRI_json_t const* masterId = JsonHelper::getObjectElement(parameters, "cid");

  if (! JsonHelper::isString(masterId)) {
    errorMsg = "collection id is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  TRI_json_t const* type = JsonHelper::getObjectElement(parameters, "type");

  if (! JsonHelper::isNumber(type)) {
    errorMsg = "collection type is missing in response";
    
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string const typeString = (type->_value._number == 3 ? "edge" : "document");

  TRI_voc_cid_t const cid = StringUtils::uint64(masterId->_value._string.data, masterId->_value._string.length - 1);
  string const collectionMsg = "collection '" + masterName + "', type " + typeString + ", id " + StringUtils::itoa(cid);


  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is invalid)
    _processedCollections.emplace(cid, masterName);

    return TRI_ERROR_NO_ERROR;
  }

  // drop collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP) {
    if (incremental) {
      return TRI_ERROR_NO_ERROR;
    }

    // first look up the collection by the cid
    TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

    if (col == nullptr && ! masterName.empty()) {
      // not found, try name next
      col = TRI_LookupCollectionByNameVocBase(_vocbase, masterName.c_str());
    }

    if (col != nullptr) {
      bool truncate = false;

      if (col->_name[0] == '_' && 
          TRI_EqualString(col->_name, TRI_COL_NAME_USERS)) {
        // better not throw away the _users collection. otherwise it is gone and this may be a problem if the
        // server crashes in-between.
        truncate = true;
      }

      if (truncate) {
        // system collection
        setProgress("truncating " + collectionMsg);
     
        SingleCollectionWriteTransaction<UINT64_MAX> trx(new StandaloneTransactionContext(), _vocbase, col->_cid);

        int res = trx.begin();

        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "unable to truncate " + collectionMsg + ": " + TRI_errno_string(res);
 
          return res;
        }

        res = trx.truncate(false);
 
        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "unable to truncate " + collectionMsg + ": " + TRI_errno_string(res);
 
          return res;
        }

        res = trx.commit();
        
        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "unable to truncate " + collectionMsg + ": " + TRI_errno_string(res);
 
          return res;
        }
      }
      else {
        // regular collection
        setProgress("dropping " + collectionMsg);
      
        int res = TRI_DropCollectionVocBase(_vocbase, col, true);

        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "unable to drop " + collectionMsg + ": " + TRI_errno_string(res);

          return res;
        }
      }
    }

    return TRI_ERROR_NO_ERROR;
  }

  // re-create collections locally
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_CREATE) {
    TRI_vocbase_col_t* col = nullptr;

    string const progress = "creating " + collectionMsg;
    setProgress(progress.c_str());
   
    if (incremental) { 
      col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

      if (col == nullptr && ! masterName.empty()) {
        // not found, try name next
        col = TRI_LookupCollectionByNameVocBase(_vocbase, masterName.c_str());
      }

      if (col != nullptr) {
        // collection is already present
        return changeCollection(col, parameters);
      }
    }

    int res = createCollection(parameters, &col);

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "unable to create " + collectionMsg + ": " + TRI_errno_string(res);

      return res;
    }

    return TRI_ERROR_NO_ERROR;
  }

  // sync collection data
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_DUMP) {
    string const progress = "dumping data for " + collectionMsg;
    setProgress(progress.c_str());
    
    TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

    if (col == nullptr && ! masterName.empty()) {
      // not found, try name next
      col = TRI_LookupCollectionByNameVocBase(_vocbase, masterName.c_str());
    }

    if (col == nullptr) {
      errorMsg = "cannot dump: " + collectionMsg + " not found";

      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    int res = TRI_ERROR_INTERNAL;

    {
      SingleCollectionWriteTransaction<UINT64_MAX> trx(new StandaloneTransactionContext(), _vocbase, col->_cid);

      res = trx.begin();

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));

        return res;
      }

      TRI_transaction_collection_t* trxCollection = trx.trxCollection();

      if (trxCollection == nullptr) {
        res = TRI_ERROR_INTERNAL;
        errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));
      }
      else {
        if (incremental && trx.documentCollection()->size() > 0) {
          res = handleCollectionSync(StringUtils::itoa(cid), trx, masterName, _masterInfo._lastLogTick, errorMsg);
        }
        else {
          res = handleCollectionDump(StringUtils::itoa(cid), trxCollection, masterName, _masterInfo._lastLogTick, errorMsg);
        } 
      }

      res = trx.finish(res);
    }

    if (res == TRI_ERROR_NO_ERROR) {
      // now create indexes
      size_t const n = TRI_LengthVector(&indexes->_value._objects);

      if (n > 0) {
        string const progress = "creating indexes for " + collectionMsg;
        setProgress(progress);

        READ_LOCKER(_vocbase->_inventoryLock);

        try {
          triagens::arango::CollectionGuard guard(_vocbase, col->_cid, false);
          TRI_vocbase_col_t* col = guard.collection();

          if (col == nullptr) {
            res = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
          }
          else {
            TRI_document_collection_t* document = col->_collection;
            TRI_ASSERT(document != nullptr);

            // create a fake transaction object to avoid assertions
            TransactionBase trx(true);
            TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

            for (size_t i = 0; i < n; ++i) {
              TRI_json_t const* idxDef = static_cast<TRI_json_t const*>(TRI_AtVector(&indexes->_value._objects, i));
              triagens::arango::Index* idx = nullptr;
 
              // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}
    
              res = TRI_FromJsonIndexDocumentCollection(document, idxDef, &idx);

              if (res != TRI_ERROR_NO_ERROR) {
                errorMsg = "could not create index: " + string(TRI_errno_string(res));
                break;
              }
              else {
                TRI_ASSERT(idx != nullptr);

                res = TRI_SaveIndex(document, idx, true);

                if (res != TRI_ERROR_NO_ERROR) {
                  errorMsg = "could not save index: " + string(TRI_errno_string(res));
                  break;
                }
              }
            }

            TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
          }
        }
        catch (triagens::basics::Exception const& ex) {
          res = ex.code();
        }
        catch (...) {
          res = TRI_ERROR_INTERNAL;
        }

      }
    }

    return res;
  }
  
  // we won't get here
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the inventory response of the master
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleInventoryResponse (TRI_json_t const* json,
                                            bool incremental,
                                            std::string& errorMsg) {
  TRI_json_t const* data = JsonHelper::getObjectElement(json, "collections");

  if (! JsonHelper::isArray(data)) {
    errorMsg = "collections section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  std::vector<std::pair<TRI_json_t const*, TRI_json_t const*>> collections;
  size_t const n = TRI_LengthVector(&data->_value._objects);
  
  for (size_t i = 0; i < n; ++i) {
    auto collection = static_cast<TRI_json_t const*>(TRI_AtVector(&data->_value._objects, i));

    if (! JsonHelper::isObject(collection)) {
      errorMsg = "collection declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
    
    TRI_json_t const* parameters = JsonHelper::getObjectElement(collection, "parameters");

    if (! JsonHelper::isObject(parameters)) {
      errorMsg = "collection parameters declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
    
    TRI_json_t const* indexes = JsonHelper::getObjectElement(collection, "indexes");

    if (! JsonHelper::isArray(indexes)) {
      errorMsg = "collection indexes declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    string const masterName = JsonHelper::getStringValue(parameters, "name", "");
  
    if (masterName.empty()) {
      errorMsg = "collection name is missing in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
  
    if (TRI_ExcludeCollectionReplication(masterName.c_str(), _includeSystem)) {
      continue;
    }
  
    if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {
      // we don't care about deleted collections
      continue;
    }
  
    if (! _restrictType.empty()) {
      auto const it = _restrictCollections.find(masterName);

      bool found = (it != _restrictCollections.end());

      if (_restrictType == "include" && ! found) {
        // collection should not be included
        continue;
      }
      else if (_restrictType == "exclude" && found) {
        // collection should be excluded
        continue;
      }
    }

    collections.emplace_back(parameters, indexes);
  }

  int res;

  // STEP 1: validate collection declarations from master
  // ----------------------------------------------------------------------------------

  // iterate over all collections from the master...
  res = iterateCollections(collections, incremental, errorMsg, PHASE_VALIDATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
    
  // STEP 2: drop collections locally if they are also present on the master (clean up)
  //  ----------------------------------------------------------------------------------

  res = iterateCollections(collections, incremental, errorMsg, PHASE_DROP);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // STEP 3: re-create empty collections locally
  // ----------------------------------------------------------------------------------

  res = iterateCollections(collections, incremental, errorMsg, PHASE_CREATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  // STEP 4: sync collection data from master and create initial indexes
  // ----------------------------------------------------------------------------------

  return iterateCollections(collections, incremental, errorMsg, PHASE_DUMP);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all collections from an array and apply an action
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::iterateCollections (std::vector<std::pair<TRI_json_t const*, TRI_json_t const*>> const& collections,
                                       bool incremental,
                                       std::string& errorMsg,
                                       sync_phase_e phase) {
  std::string phaseMsg("starting phase " + translatePhase(phase) + " with " + std::to_string(collections.size()) + " collections");
  setProgress(phaseMsg); 

  for (auto const& collection : collections) {
    TRI_json_t const* parameters = collection.first;
    TRI_json_t const* indexes    = collection.second;

    TRI_ASSERT(parameters != nullptr);
    TRI_ASSERT(indexes != nullptr);

    int res = handleCollection(parameters, indexes, incremental, errorMsg, phase);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // all ok
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
