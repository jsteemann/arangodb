////////////////////////////////////////////////////////////////////////////////
/// @brief base transaction wrapper
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_TRANSACTION_H
#define ARANGODB_UTILS_TRANSACTION_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/logging.h"
#include "Basics/random.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/DocumentHelper.h"
#include "Utils/TransactionContext.h"
#include "VocBase/collection.h"
#include "VocBase/Ditch.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/headers.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"
#include "VocBase/voc-types.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                 class Transaction
// -----------------------------------------------------------------------------

      class Transaction : public TransactionBase {

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction
////////////////////////////////////////////////////////////////////////////////

        private:

          Transaction () = delete;
          Transaction (Transaction const&) = delete;
          Transaction& operator= (Transaction const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

        public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction
////////////////////////////////////////////////////////////////////////////////

          Transaction (TransactionContext* transactionContext,
                       TRI_vocbase_t* vocbase,
                       TRI_voc_tid_t externalId)
            : _externalId(externalId),
              _setupState(TRI_ERROR_NO_ERROR),
              _nestingLevel(0),
              _errorData(),
              _hints(0),
              _timeout(0.0),
              _waitForSync(false),
              _isReal(true),
              _trx(nullptr),
              _vocbase(vocbase),
              _transactionContext(transactionContext) {

            TRI_ASSERT(_vocbase != nullptr);
            TRI_ASSERT(_transactionContext != nullptr);

            if (ServerState::instance()->isCoordinator()) {
              _isReal = false;
            }

            this->setupTransaction();
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

          virtual ~Transaction () {
            if (_trx == nullptr) {
              return;
            }

            if (isEmbeddedTransaction()) {
              _trx->_nestingLevel--;
            }
            else {
              if (getStatus() == TRI_TRANSACTION_RUNNING) {
                // auto abort a running transaction
                this->abort();
              }

              // free the data associated with the transaction
              freeTransaction();
            }

            delete _transactionContext;
          }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

        public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return database of transaction
////////////////////////////////////////////////////////////////////////////////

          inline TRI_vocbase_t* vocbase () const {
            return _vocbase;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief return internals of transaction
////////////////////////////////////////////////////////////////////////////////

          inline TRI_transaction_t* getInternals () const {
            return _trx;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a transaction hint
////////////////////////////////////////////////////////////////////////////////

          void inline addHint (TRI_transaction_hint_e hint,
                               bool passthrough) {
            _hints |= (TRI_transaction_hint_t) hint;

            if (passthrough && _trx != nullptr) {
              _trx->_hints |= ((TRI_transaction_hint_t) hint);
            }
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a transaction hint
////////////////////////////////////////////////////////////////////////////////

          void inline removeHint (TRI_transaction_hint_e hint,
                                  bool passthrough) {
            _hints &= ~ ((TRI_transaction_hint_t) hint);

            if (passthrough && _trx != nullptr) {
              _trx->_hints &= ~ ((TRI_transaction_hint_t) hint);
            }
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the registered error data
////////////////////////////////////////////////////////////////////////////////

          std::string const getErrorData () const {
            return _errorData;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the names of all collections used in the transaction
////////////////////////////////////////////////////////////////////////////////

          std::vector<std::string> collectionNames () {
            std::vector<std::string> result;

            for (size_t i = 0; i < _trx->_collections._length; ++i) {
              auto trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&_trx->_collections, i)); 

              if (trxCollection->_collection != nullptr) {
                result.emplace_back(trxCollection->_collection->_name);
              }
            }

            return result;  
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection name resolver
////////////////////////////////////////////////////////////////////////////////

          CollectionNameResolver const* resolver () const {
            CollectionNameResolver const* r = this->_transactionContext->getResolver();
            TRI_ASSERT(r != nullptr);
            return r;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embedded
////////////////////////////////////////////////////////////////////////////////

          inline bool isEmbeddedTransaction () const {
            return (_nestingLevel > 0);
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the status of the transaction
////////////////////////////////////////////////////////////////////////////////

          inline TRI_transaction_status_e getStatus () const {
            if (_trx != nullptr) {
              return _trx->_status;
            }

            return TRI_TRANSACTION_UNDEFINED;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////

          int begin () {
            if (_trx == nullptr) {
              return TRI_ERROR_TRANSACTION_INTERNAL;
            }

            if (_setupState != TRI_ERROR_NO_ERROR) {
              return _setupState;
            }

#ifdef TRI_ENABLE_MAINTAINER_MODE
            TRI_ASSERT(_numberTrxActive == _numberTrxInScope - 1);
            _numberTrxActive++;  // Every transaction gets here at most once
#endif

            if (! _isReal) {
              if (_nestingLevel == 0) {
                _trx->_status = TRI_TRANSACTION_RUNNING;
              }
              return TRI_ERROR_NO_ERROR;
            }


            int res = TRI_BeginTransaction(_trx, _hints, _nestingLevel);

            return res;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

          int commit () {
            if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
              // transaction not created or not running
              return TRI_ERROR_TRANSACTION_INTERNAL;
            }

            if (! _isReal) {
              if (_nestingLevel == 0) {
                _trx->_status = TRI_TRANSACTION_COMMITTED;
              }
#ifdef TRI_ENABLE_MAINTAINER_MODE
              TRI_ASSERT(_numberTrxActive == _numberTrxInScope);
              TRI_ASSERT(_numberTrxActive > 0);
              _numberTrxActive--;  // Every transaction gets here at most once
#endif
              return TRI_ERROR_NO_ERROR;
            }

            int res = TRI_CommitTransaction(_trx, _nestingLevel);

#ifdef TRI_ENABLE_MAINTAINER_MODE
            TRI_ASSERT(_numberTrxActive == _numberTrxInScope);
            TRI_ASSERT(_numberTrxActive > 0);
            _numberTrxActive--;  // Every transaction gets here at most once
#endif

            return res;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

          int abort () {
            if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
              // transaction not created or not running
              return TRI_ERROR_TRANSACTION_INTERNAL;
            }

            if (! _isReal) {
              if (_nestingLevel == 0) {
                _trx->_status = TRI_TRANSACTION_ABORTED;
              }

#ifdef TRI_ENABLE_MAINTAINER_MODE
              TRI_ASSERT(_numberTrxActive == _numberTrxInScope);
              TRI_ASSERT(_numberTrxActive > 0);
              _numberTrxActive--;  // Every transaction gets here at most once
#endif
              return TRI_ERROR_NO_ERROR;
            }

            int res = TRI_AbortTransaction(_trx, _nestingLevel);

#ifdef TRI_ENABLE_MAINTAINER_MODE
            TRI_ASSERT(_numberTrxActive == _numberTrxInScope);
            TRI_ASSERT(_numberTrxActive > 0);
            _numberTrxActive--;  // Every transaction gets here at most once
#endif

            return res;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction (commit or abort), based on the previous state
////////////////////////////////////////////////////////////////////////////////

          int finish (const int errorNum) {
            int res;

            if (errorNum == TRI_ERROR_NO_ERROR) {
              // there was no previous error, so we'll commit
              res = this->commit();
            }
            else {
              // there was a previous error, so we'll abort
              this->abort();

              // return original error number
              res = errorNum;
            }

            return res;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the transaction collection for a document collection
////////////////////////////////////////////////////////////////////////////////

         TRI_transaction_collection_t* trxCollection (TRI_voc_cid_t cid) {
           TRI_ASSERT(_trx != nullptr);
           TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

           return TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief order a ditch for a collection
////////////////////////////////////////////////////////////////////////////////

         triagens::arango::DocumentDitch* orderDitch (TRI_transaction_collection_t* trxCollection) {
           TRI_ASSERT(_trx != nullptr);
           TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
           TRI_ASSERT(trxCollection->_collection != nullptr);

           TRI_document_collection_t* document = trxCollection->_collection->_collection;
           TRI_ASSERT(document != nullptr);

           if (trxCollection->_ditch == nullptr) {
             trxCollection->_ditch = document->ditches()->createDocumentDitch(true, __FILE__, __LINE__);
           }
           else {
             // tell everyone else this ditch is still in use,
             // at least until the transaction is over
             trxCollection->_ditch->setUsedByTransaction();
           }

           return trxCollection->_ditch;
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit and an internal
/// offset into the primary index. this can be used for incremental access to
/// the documents without restarting the index scan at the begin
////////////////////////////////////////////////////////////////////////////////

        int readIncremental (TRI_transaction_collection_t* trxCollection,
                             std::vector<TRI_doc_mptr_copy_t>& docs,
                             triagens::basics::BucketPosition& internalSkip,
                             uint64_t batchSize,
                             int64_t skip,
                             uint64_t limit,
                             uint64_t& total) {

          TRI_document_collection_t* document = documentCollection(trxCollection);

          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          uint64_t count = 0;

          try {
            if (batchSize > 2048) {
              docs.reserve(2048);
            }
            else if (batchSize > 0) {
              docs.reserve(batchSize);
            }
            TRI_ASSERT(batchSize > 0);
              
            auto primaryIndex = document->primaryIndex();

            while (count < batchSize) {
              TRI_doc_mptr_t const* mptr = primaryIndex->lookupSequential(internalSkip, total);

              if (mptr == nullptr) {
                break;
              }
              if (skip > 0) {
                --skip;
              }
              else {
                docs.emplace_back(*mptr);

                if (++count >= limit) {
                  break;
                }
              }
            }
          }
          catch (...) {
            this->unlock(trxCollection, TRI_TRANSACTION_READ);
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          // READ-LOCK END

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit and an internal
/// offset into the primary index. this can be used for incremental access to
/// the documents without restarting the index scan at the begin
////////////////////////////////////////////////////////////////////////////////

        int readRandom (TRI_transaction_collection_t* trxCollection,
                        std::vector<TRI_doc_mptr_copy_t>& docs,
                        triagens::basics::BucketPosition& initialPosition,
                        triagens::basics::BucketPosition& position,
                        uint64_t batchSize,
                        uint64_t& step,
                        uint64_t& total) {
          TRI_document_collection_t* document = documentCollection(trxCollection);
          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          uint64_t numRead = 0;
          TRI_ASSERT(batchSize > 0);

          while (numRead < batchSize) { 
            auto mptr = document->primaryIndex()->lookupRandom(initialPosition, position, step, total);
            if (mptr == nullptr) {
              // Read all documents randomly
              break;
            }
            docs.emplace_back(*mptr);
            ++numRead;
          }
          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          // READ-LOCK END
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document
////////////////////////////////////////////////////////////////////////////////

        int remove (TRI_transaction_collection_t* trxCollection,
                    std::string const& key,
                    TRI_voc_rid_t rid,
                    TRI_doc_update_policy_e policy,
                    TRI_voc_rid_t expectedRevision,
                    TRI_voc_rid_t* actualRevision,
                    bool forceSync) {

          TRI_doc_update_policy_t updatePolicy(policy, expectedRevision, actualRevision);

          try {
            return TRI_RemoveShapedJsonDocumentCollection(trxCollection,
                                                          (TRI_voc_key_t) key.c_str(),
                                                          rid,
                                                          nullptr,
                                                          &updatePolicy,
                                                          ! isLocked(trxCollection, TRI_TRANSACTION_WRITE),
                                                          forceSync);
          }
          catch (triagens::basics::Exception const& ex) {
            return ex.code();
          }
          catch (...) {
            return TRI_ERROR_INTERNAL;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using JSON
////////////////////////////////////////////////////////////////////////////////

        int create (TRI_transaction_collection_t* trxCollection,
                    TRI_doc_mptr_copy_t* mptr,
                    TRI_json_t const* json,
                    void const* data,
                    bool forceSync) {

          TRI_voc_key_t key = nullptr;
          int res = DocumentHelper::getKey(json, &key);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto shaper = this->shaper(trxCollection);
          TRI_memory_zone_t* zone = shaper->memoryZone();
          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(shaper, json, true);

          if (shaped == nullptr) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          res = create(trxCollection,
                       key,
                       0,
                       mptr,
                       shaped,
                       data,
                       forceSync);

          TRI_FreeShapedJson(zone, shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using JSON
////////////////////////////////////////////////////////////////////////////////

        int update (TRI_transaction_collection_t* trxCollection,
                    std::string const& key,
                    TRI_voc_rid_t rid,
                    TRI_doc_mptr_copy_t* mptr,
                    TRI_json_t const* json,
                    TRI_doc_update_policy_e policy,
                    TRI_voc_rid_t expectedRevision,
                    TRI_voc_rid_t* actualRevision,
                    bool forceSync) {

          auto shaper = this->shaper(trxCollection);
          TRI_memory_zone_t* zone = shaper->memoryZone();
          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(shaper, json, true);

          if (shaped == nullptr) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          int res = update(trxCollection,
                           key,
                           rid,
                           mptr,
                           shaped,
                           policy,
                           expectedRevision,
                           actualRevision,
                           forceSync);

          TRI_FreeShapedJson(zone, shaped);
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a single document, identified by key
////////////////////////////////////////////////////////////////////////////////

        int readSingle (TRI_transaction_collection_t* trxCollection,
                        TRI_doc_mptr_copy_t* mptr,
                        std::string const& key) {

          TRI_ASSERT(mptr != nullptr);

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          try {
            return TRI_ReadShapedJsonDocumentCollection(trxCollection,
                                                        (TRI_voc_key_t) key.c_str(),
                                                        mptr,
                                                        ! isLocked(trxCollection, TRI_TRANSACTION_READ));
          }
          catch (triagens::basics::Exception const& ex) {
            return ex.code();
          }
          catch (...) {
            return TRI_ERROR_INTERNAL;
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

         TRI_document_collection_t* documentCollection (TRI_transaction_collection_t const* trxCollection) const {
           TRI_ASSERT(_trx != nullptr);
           TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
           TRI_ASSERT(trxCollection->_collection != nullptr);
           TRI_ASSERT(trxCollection->_collection->_collection != nullptr);

           return trxCollection->_collection->_collection;
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a collection's shaper
////////////////////////////////////////////////////////////////////////////////

         VocShaper* shaper (TRI_transaction_collection_t const* trxCollection) const {
           TRI_ASSERT(_trx != nullptr);
           TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
           TRI_ASSERT(trxCollection->_collection != nullptr);
           TRI_ASSERT(trxCollection->_collection->_collection != nullptr);

           return trxCollection->_collection->_collection->getShaper();  // PROTECTED by trx in trxCollection
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id, with the name supplied
////////////////////////////////////////////////////////////////////////////////

        int addCollection (TRI_voc_cid_t cid,
                           char const* name,
                           TRI_transaction_type_e type) {
          int res = this->addCollection(cid, type);

          if (res != TRI_ERROR_NO_ERROR) {
            _errorData = std::string(name);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id
////////////////////////////////////////////////////////////////////////////////

        int addCollection (TRI_voc_cid_t cid,
                           TRI_transaction_type_e type) {
          if (_trx == nullptr) {
            return registerError(TRI_ERROR_INTERNAL);
          }

          if (cid == 0) {
            // invalid cid
            return registerError(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }

          const TRI_transaction_status_e status = getStatus();

          if (status == TRI_TRANSACTION_COMMITTED ||
              status == TRI_TRANSACTION_ABORTED) {
            // transaction already finished?
            return registerError(TRI_ERROR_TRANSACTION_INTERNAL);
          }

          int res;

          if (this->isEmbeddedTransaction()) {
            res = this->addCollectionEmbedded(cid, type);
          }
          else {
            res = this->addCollectionToplevel(cid, type);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by name
////////////////////////////////////////////////////////////////////////////////

        int addCollection (std::string const& name,
                           TRI_transaction_type_e type) {
          if (! _isReal) {
            return addCollection(this->resolver()->getCollectionIdCluster(name), name.c_str(), type);
          }

          return addCollection(this->resolver()->getCollectionId(name), name.c_str(), type);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the lock acquisition timeout
////////////////////////////////////////////////////////////////////////////////

        void setTimeout (double timeout) {
          _timeout = timeout;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the waitForSync property
////////////////////////////////////////////////////////////////////////////////

        void setWaitForSync () {
          _waitForSync = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-lock a collection
////////////////////////////////////////////////////////////////////////////////

        int lock (TRI_transaction_collection_t* trxCollection,
                  TRI_transaction_type_e type) {

          if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }

          int res = TRI_LockCollectionTransaction(trxCollection, type, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        int unlock (TRI_transaction_collection_t* trxCollection,
                    TRI_transaction_type_e type) {

          if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }

          int res = TRI_UnlockCollectionTransaction(trxCollection, type, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        bool isLocked (TRI_transaction_collection_t* const trxCollection,
                       TRI_transaction_type_e type) {
          if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
            return false;
          }

          bool locked = TRI_IsLockedCollectionTransaction(trxCollection, type, _nestingLevel);

          return locked;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document
////////////////////////////////////////////////////////////////////////////////

        int readAny (TRI_transaction_collection_t* trxCollection,
                     TRI_doc_mptr_copy_t* mptr) {

          TRI_document_collection_t* document = documentCollection(trxCollection);

          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          auto idx = document->primaryIndex();
          triagens::basics::BucketPosition intPos;
          triagens::basics::BucketPosition pos;
          uint64_t step = 0;
          uint64_t total = 0;

          TRI_doc_mptr_t* found = idx->lookupRandom(intPos, pos, step, total);
          if (found != nullptr) {
            *mptr = *found;
          }
          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents
////////////////////////////////////////////////////////////////////////////////

        int readAll (TRI_transaction_collection_t* trxCollection,
                     std::vector<std::string>& ids,
                     bool lock) {

          TRI_document_collection_t* document = documentCollection(trxCollection);

          if (lock) {
            // READ-LOCK START
            int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }
          auto idx = document->primaryIndex();
          size_t used = idx->size();

          if (used > 0) {
            triagens::basics::BucketPosition step;
            uint64_t total = 0;

            while (true) {
              TRI_doc_mptr_t const* mptr = idx->lookupSequential(step, total);

              if (mptr == nullptr) {
                break;
              }
              ids.emplace_back(TRI_EXTRACT_MARKER_KEY(mptr));
            }
          }

          if (lock) {
            this->unlock(trxCollection, TRI_TRANSACTION_READ);
            // READ-LOCK END
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read master pointers in order of insertion/update
////////////////////////////////////////////////////////////////////////////////

        int readOrdered (TRI_transaction_collection_t* trxCollection,
                         std::vector<TRI_doc_mptr_copy_t>& documents,
                         int64_t offset,
                         int64_t count) {
          TRI_document_collection_t* document = documentCollection(trxCollection);

          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          TRI_doc_mptr_t* doc;

          if (offset >= 0) {
            // read from front
            doc = document->_headersPtr->front();  // PROTECTED by trx in trxCollection
            int64_t i = 0;

            while (doc != nullptr && i < offset) {
              doc = doc->_next;
              ++i;
            }

            i = 0;
            while (doc != nullptr && i < count) {
              documents.emplace_back(*doc);
              doc = doc->_next;
              ++i;
            }
          }
          else {
            // read from back
            doc = document->_headersPtr->back();  // PROTECTED by trx in trxCollection
            int64_t i = -1;

            while (doc != nullptr && i > offset) {
              doc = doc->_prev;
              --i;
            }

            i = 0;
            while (doc != nullptr && i < count) {
              documents.emplace_back(*doc);
              doc = doc->_prev;
              ++i;
            }
          }

          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          // READ-LOCK END

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit
////////////////////////////////////////////////////////////////////////////////

        int readSlice (TRI_transaction_collection_t* trxCollection,
                       std::vector<TRI_doc_mptr_copy_t>& docs,
                       int64_t skip,
                       uint64_t limit,
                       uint64_t& total) {

          TRI_document_collection_t* document = documentCollection(trxCollection);

          if (limit == 0) {
            // nothing to do
            return TRI_ERROR_NO_ERROR;
          }

          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          uint64_t count = 0;
          auto idx = document->primaryIndex();
          TRI_doc_mptr_t const* mptr = nullptr; 

          if (skip < 0) {
            triagens::basics::BucketPosition position;
            do {
              mptr = idx->lookupSequentialReverse(position);
              ++skip;
            }
            while (skip < 0 && mptr != nullptr);

            if (mptr == nullptr) {
              this->unlock(trxCollection, TRI_TRANSACTION_READ);
              // To few elements, skipped all
              return TRI_ERROR_NO_ERROR;
            }

            do {
              mptr = idx->lookupSequentialReverse(position);

              if (mptr == nullptr) {
                break;
              }
              ++count;
              docs.emplace_back(*mptr);
            }
            while (count < limit);

            this->unlock(trxCollection, TRI_TRANSACTION_READ);
            return TRI_ERROR_NO_ERROR;
          }
          triagens::basics::BucketPosition position;

          while (skip > 0) {
            mptr = idx->lookupSequential(position, total);
            --skip;
            if (mptr == nullptr) {
              // To few elements, skipped all
              this->unlock(trxCollection, TRI_TRANSACTION_READ);
              return TRI_ERROR_NO_ERROR;
            }
          }

          do {
            mptr = idx->lookupSequential(position, total);
            if (mptr == nullptr) {
              break;
            }
            ++count;
            docs.emplace_back(*mptr);
          }
          while (count < limit);

          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers
////////////////////////////////////////////////////////////////////////////////

        int readSlice (TRI_transaction_collection_t* trxCollection,
                       std::vector<TRI_doc_mptr_t const*>& docs) {
          TRI_document_collection_t* document = documentCollection(trxCollection);
          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          triagens::basics::BucketPosition position;
          uint64_t total = 0;
          auto idx = document->primaryIndex();
          docs.reserve(idx->size());

          while (true) {
            TRI_doc_mptr_t const* mptr = idx->lookupSequential(position, total);

            if (mptr == nullptr) {
              break;
            }

            docs.emplace_back(mptr);
          }

          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        inline int create (TRI_transaction_collection_t* trxCollection,
                           const TRI_voc_key_t key,
                           TRI_voc_rid_t rid,
                           TRI_doc_mptr_copy_t* mptr,
                           TRI_shaped_json_t const* shaped,
                           void const* data,
                           bool forceSync) {

          bool lock = ! isLocked(trxCollection, TRI_TRANSACTION_WRITE);

          try {
            return TRI_InsertShapedJsonDocumentCollection(trxCollection,
                                                          key,
                                                          rid,
                                                          nullptr,
                                                          mptr,
                                                          shaped,
                                                          static_cast<TRI_document_edge_t const*>(data),
                                                          lock,
                                                          forceSync,
                                                          false);
          }
          catch (triagens::basics::Exception const& ex) {
            return ex.code();
          }
          catch (...) {
            return TRI_ERROR_INTERNAL;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        inline int update (TRI_transaction_collection_t* const trxCollection,
                           std::string const& key,
                           TRI_voc_rid_t rid,
                           TRI_doc_mptr_copy_t* mptr,
                           TRI_shaped_json_t* const shaped,
                           TRI_doc_update_policy_e policy,
                           TRI_voc_rid_t expectedRevision,
                           TRI_voc_rid_t* actualRevision,
                           bool forceSync) {

          TRI_doc_update_policy_t updatePolicy(policy, expectedRevision, actualRevision);

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          try {
            return TRI_UpdateShapedJsonDocumentCollection(trxCollection,
                                                          (const TRI_voc_key_t) key.c_str(),
                                                          rid,
                                                          nullptr,
                                                          mptr,
                                                          shaped,
                                                          &updatePolicy,
                                                          ! isLocked(trxCollection, TRI_TRANSACTION_WRITE),
                                                          forceSync);
          }
          catch (triagens::basics::Exception const& ex) {
            return ex.code();
          }
          catch (...) {
            return TRI_ERROR_INTERNAL;
          }
        }


////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a collection
////////////////////////////////////////////////////////////////////////////////

        int removeAll (TRI_transaction_collection_t* const trxCollection,
                       bool forceSync) {

          std::vector<std::string> ids;

          if (orderDitch(trxCollection) == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }
          
          TRI_ASSERT(isLocked(trxCollection, TRI_TRANSACTION_WRITE));

          int res = readAll(trxCollection, ids, false);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          try {
            for (auto const& it : ids) {
              res = TRI_RemoveShapedJsonDocumentCollection(trxCollection,
                                                           (TRI_voc_key_t) it.c_str(),
                                                           0,
                                                           nullptr, // marker 
                                                           nullptr, // policy
                                                           false,
                                                           forceSync);

              if (res != TRI_ERROR_NO_ERROR) {
                // halt on first error
                break;
              }
            }
          }
          catch (triagens::basics::Exception const& ex) {
            res = ex.code();
          }
          catch (...) {
            res = TRI_ERROR_INTERNAL;
          }
          
          return res;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error for the transaction
////////////////////////////////////////////////////////////////////////////////

        int registerError (int errorNum) {
          TRI_ASSERT(errorNum != TRI_ERROR_NO_ERROR);

          if (_setupState == TRI_ERROR_NO_ERROR) {
            _setupState = errorNum;
          }

          TRI_ASSERT(_setupState != TRI_ERROR_NO_ERROR);

          return errorNum;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to an embedded transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionEmbedded (TRI_voc_cid_t cid,
                                   TRI_transaction_type_e type) {
          TRI_ASSERT(_trx != nullptr);

          int res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel, false);

          if (res != TRI_ERROR_NO_ERROR) {
            return registerError(res);
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a top-level transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionToplevel (TRI_voc_cid_t cid,
                                   TRI_transaction_type_e type) {
          TRI_ASSERT(_trx != nullptr);

          int res;

          if (getStatus() != TRI_TRANSACTION_CREATED) {
            // transaction already started?
            res = TRI_ERROR_TRANSACTION_INTERNAL;
          }
          else {
            res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel, false);
          }

          if (res != TRI_ERROR_NO_ERROR) {
            registerError(res);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the transaction
/// this will first check if the transaction is embedded in a parent
/// transaction. if not, it will create a transaction of its own
////////////////////////////////////////////////////////////////////////////////

        int setupTransaction () {
          // check in the context if we are running embedded
          _trx = this->_transactionContext->getParentTransaction();

          if (_trx != nullptr) {
            // yes, we are embedded
            _setupState = setupEmbedded();
          }
          else {
            // non-embedded
            _setupState = setupToplevel();
          }

          // this may well be TRI_ERROR_NO_ERROR...
          return _setupState;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set up an embedded transaction
////////////////////////////////////////////////////////////////////////////////

        int setupEmbedded () {
          TRI_ASSERT(_nestingLevel == 0);

          _nestingLevel = ++_trx->_nestingLevel;

          if (! this->_transactionContext->isEmbeddable()) {
            // we are embedded but this is disallowed...
            return TRI_ERROR_TRANSACTION_NESTED;
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set up a top-level transaction
////////////////////////////////////////////////////////////////////////////////

        int setupToplevel () {
          TRI_ASSERT(_nestingLevel == 0);

          // we are not embedded. now start our own transaction
          _trx = TRI_CreateTransaction(_vocbase,
                                       _externalId,
                                       _timeout,
                                       _waitForSync);

          if (_trx == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // register the transaction in the context
          return this->_transactionContext->registerTransaction(_trx);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief free transaction
////////////////////////////////////////////////////////////////////////////////

        int freeTransaction () {
          TRI_ASSERT(! isEmbeddedTransaction());

          if (_trx != nullptr) {
            this->_transactionContext->unregisterTransaction();

            TRI_FreeTransaction(_trx);
            _trx = nullptr;
          }

          return TRI_ERROR_NO_ERROR;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief external transaction id. used in replication only
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_tid_t _externalId;

////////////////////////////////////////////////////////////////////////////////
/// @brief error that occurred on transaction initialization (before begin())
////////////////////////////////////////////////////////////////////////////////

        int _setupState;

////////////////////////////////////////////////////////////////////////////////
/// @brief how deep the transaction is down in a nested transaction structure
////////////////////////////////////////////////////////////////////////////////

        int _nestingLevel;

////////////////////////////////////////////////////////////////////////////////
/// @brief additional error data
////////////////////////////////////////////////////////////////////////////////

        std::string _errorData;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction hints
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_hint_t _hints;

////////////////////////////////////////////////////////////////////////////////
/// @brief timeout for lock acquisition
////////////////////////////////////////////////////////////////////////////////

        double _timeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for sync property for transaction
////////////////////////////////////////////////////////////////////////////////

        bool _waitForSync;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not this is a "real" transaction
////////////////////////////////////////////////////////////////////////////////

        bool _isReal;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the C transaction struct
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_t* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* const _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction context
////////////////////////////////////////////////////////////////////////////////

        TransactionContext* _transactionContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief makeNolockHeaders
////////////////////////////////////////////////////////////////////////////////

      public:

        static thread_local std::unordered_set<std::string>* _makeNolockHeaders;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
