////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase context
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VocbaseContext.h"

#include "Basics/MutexLocker.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Rest/ConnectionInfo.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sid lock
////////////////////////////////////////////////////////////////////////////////

static triagens::basics::Mutex SidLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief sid cache
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
// turn off warnings about too long type name for debug symbols blabla in MSVC only...
#pragma warning(disable : 4503)
#endif

typedef std::unordered_map<std::string, std::pair<std::string, double>> DatabaseSessionsType;

static std::unordered_map<std::string, DatabaseSessionsType> SidCache;

// -----------------------------------------------------------------------------
// --SECTION--                                               static initializers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief time-to-live for aardvark server sessions
////////////////////////////////////////////////////////////////////////////////

double VocbaseContext::ServerSessionTtl = 60.0 * 60.0 * 2; // 2 hours session timeout

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a sid
////////////////////////////////////////////////////////////////////////////////

void VocbaseContext::createSid (std::string const& database,
                                std::string const& sid, 
                                std::string const& username) {
  MUTEX_LOCKER(SidLock);

  // find entries for database first
  auto it = SidCache.find(database);

  if (it == SidCache.end()) {
    it = SidCache.emplace(database, DatabaseSessionsType()).first;
  }

  // now insert a database-specific sid
  double const now = TRI_microtime() * 1000.0;
  (*it).second.emplace(sid, std::make_pair(username, now));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears all sid entries for a database
////////////////////////////////////////////////////////////////////////////////

void VocbaseContext::clearSid (std::string const& database) {
  MUTEX_LOCKER(SidLock);
  
  SidCache.erase(database); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears a sid
////////////////////////////////////////////////////////////////////////////////

void VocbaseContext::clearSid (std::string const& database,
                               std::string const& sid) {
  MUTEX_LOCKER(SidLock);
  
  auto it = SidCache.find(database);

  if (it == SidCache.end()) {
    // database not found. no need to go on
    return;
  }

  (*it).second.erase(sid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the last access time
////////////////////////////////////////////////////////////////////////////////

double VocbaseContext::accessSid (std::string const& database,
                                  std::string const& sid) {
  MUTEX_LOCKER(SidLock);

  auto it = SidCache.find(database);

  if (it == SidCache.end()) {
    // database not found. no need to go on
    return 0.0;
  }

  auto const& sids = (*it).second;
  auto it2 = sids.find(sid);

  if (it2 == sids.end()) {
    return 0.0;
  }

  return (*it2).second.second;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class VocbaseContext
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

VocbaseContext::VocbaseContext (HttpRequest* request,
                                TRI_server_t* server,
                                TRI_vocbase_t* vocbase) :
  RequestContext(request),
  _server(server),
  _vocbase(vocbase) {

  TRI_ASSERT(_server != nullptr);
  TRI_ASSERT(_vocbase != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

VocbaseContext::~VocbaseContext () {
  TRI_ReleaseVocBase(_vocbase);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to use special cluster authentication
////////////////////////////////////////////////////////////////////////////////

bool VocbaseContext::useClusterAuthentication () const {
  auto role = ServerState::instance()->getRole();

  if (ServerState::instance()->isDBServer(role)) {
    return true;
  }

  if (ServerState::instance()->isCoordinator(role)) {
    std::string s(_request->requestPath());

    if (s == "/_api/shard-comm" || s == "/_admin/shutdown") {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return authentication realm
////////////////////////////////////////////////////////////////////////////////

const char* VocbaseContext::getRealm () const {
  if (_vocbase == nullptr) {
    return nullptr;
  }

  return _vocbase->_name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponseCode VocbaseContext::authenticate () {
  TRI_ASSERT(_vocbase != nullptr);

  if (! _vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return HttpResponse::OK;
  }

#ifdef TRI_HAVE_LINUX_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DOMAIN_UNIX &&
      ! _vocbase->_settings.requireAuthenticationUnixSockets) {
    // no authentication required for unix socket domain connections
    return HttpResponse::OK;
  }
#endif

  char const* path = _request->requestPath();

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.

    if (path != nullptr) {
      // check if path starts with /_
      if (*path != '/') {
        return HttpResponse::OK;
      }
      if (*path != '\0' && *(path + 1) != '_') {
        return HttpResponse::OK;
      }
    }
  }

  if (TRI_IsPrefixString(path, "/_open/") || TRI_IsPrefixString(path, "/_admin/aardvark/") || TRI_EqualString(path, "/")) {
    return HttpResponse::OK;
  }

  // .............................................................................
  // authentication required
  // .............................................................................

  bool found;
  char cn[4096];

  cn[0] = '\0';
  strncat(cn, "arango_sid_", 11);
  strncat(cn + 11, _vocbase->_name, sizeof(cn) - 12);

  // extract the sid
  char const* sid = _request->cookieValue(cn, found);

  if (found) {
    MUTEX_LOCKER(SidLock);

    auto it = SidCache.find(_vocbase->_name);
    
    if (it != SidCache.end()) {
      auto& sids = (*it).second;
      auto it2 = sids.find(sid);

      if (it2 != sids.end()) {
        _request->setUser((*it2).second.first);
        double const now = TRI_microtime() * 1000.0;
        // fetch last access date of session
        double const lastAccess = (*it2).second.second;

        // check if session has expired
        if (lastAccess + (ServerSessionTtl * 1000.0) < now) {
          // session has expired
          sids.erase(sid);
          return HttpResponse::UNAUTHORIZED;
        }

        (*it2).second.second = now;
        return HttpResponse::OK;
      }
    }

    // no cookie found. fall-through to regular HTTP authentication
  }

  char const* auth = _request->header("authorization", found);

  if (! found || ! TRI_CaseEqualString2(auth, "basic ", 6)) {
    return HttpResponse::UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  if (useClusterAuthentication()) {
    string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != string(auth)) {
      return HttpResponse::UNAUTHORIZED;
    }

    string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE("invalid authentication data found, cannot extract username/password");

      return HttpResponse::BAD;
    }

    string const username = up.substr(0, n);
    _request->setUser(username);

    return HttpResponse::OK;
  }

  // look up the info in the cache first
  bool mustChange;
  char* cached = TRI_CheckCacheAuthInfo(_vocbase, auth, &mustChange);
  string username;

  // found a cached entry, access must be granted
  if (cached != 0) {
    username = string(cached);
    TRI_Free(TRI_CORE_MEM_ZONE, cached);
  }

  // no entry found in cache, decode the basic auth info and look it up
  else {
    string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE("invalid authentication data found, cannot extract username/password");
      return HttpResponse::BAD;
    }

    username = up.substr(0, n);

    LOG_TRACE("checking authentication for user '%s'", username.c_str());
    bool res = TRI_CheckAuthenticationAuthInfo(
                 _vocbase, auth, username.c_str(), up.substr(n + 1).c_str(), &mustChange);

    if (! res) {
      return HttpResponse::UNAUTHORIZED;
    }
  }

  // TODO: create a user object for the VocbaseContext
  _request->setUser(username);

  if (mustChange) {
    if ((_request->requestType() == HttpRequest::HTTP_REQUEST_PUT
         || _request->requestType() == HttpRequest::HTTP_REQUEST_PATCH)
        && TRI_EqualString2(_request->requestPath(), "/_api/user/", 11)) {
      return HttpResponse::OK;
    }

    return HttpResponse::FORBIDDEN;
  }

  return HttpResponse::OK;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
