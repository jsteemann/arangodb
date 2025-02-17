////////////////////////////////////////////////////////////////////////////////
/// @brief database server functionality
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
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_SERVER_H
#define ARANGODB_VOC_BASE_SERVER_H 1

#include "Basics/Common.h"
#include "Basics/associative.h"
#include "Basics/locks.h"
#include "Basics/Mutex.h"
#include "Basics/threads.h"
#include "Basics/DataProtector.h"
#include "Basics/vector.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase-defaults.h"

struct TRI_vocbase_t;

namespace triagens {
  namespace aql {
    class QueryRegistry;
  }
  namespace basics {
    class ThreadPool;
  }
  namespace rest {
    class ApplicationEndpointServer;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief server structure
////////////////////////////////////////////////////////////////////////////////

struct DatabasesLists {
  std::unordered_map<std::string, TRI_vocbase_t*> _databases;
  std::unordered_map<std::string, TRI_vocbase_t*> _coordinatorDatabases;
  std::unordered_set<TRI_vocbase_t*> _droppedDatabases;
};

struct TRI_server_t {
  TRI_server_t ();
  ~TRI_server_t ();

  std::atomic<DatabasesLists*>       _databasesLists;
  // TODO: Make this again a template once everybody has gcc >= 4.9.2
  // triagens::basics::DataProtector<64>  
  triagens::basics::DataProtector    _databasesProtector;
  triagens::basics::Mutex            _databasesMutex;

  TRI_thread_t                       _databaseManager;

  TRI_vocbase_defaults_t             _defaults;
  triagens::rest::ApplicationEndpointServer*  _applicationEndpointServer; 
  triagens::basics::ThreadPool*      _indexPool;                 
  triagens::aql::QueryRegistry*      _queryRegistry;

  char*                              _basePath;
  char*                              _databasePath;
  char*                              _lockFilename;
  char*                              _serverIdFilename;
  char*                              _appPath;

  bool                               _disableReplicationAppliers;
  bool                               _iterateMarkersOnOpen;
  bool                               _hasCreatedSystemDatabase;
  bool                               _initialized;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief page size
////////////////////////////////////////////////////////////////////////////////

extern size_t PageSize;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a server instance with configuration
////////////////////////////////////////////////////////////////////////////////

int TRI_InitServer (TRI_server_t*,
                    triagens::rest::ApplicationEndpointServer*,
                    triagens::basics::ThreadPool*,
                    char const*,
                    char const*,
                    TRI_vocbase_defaults_t const*,
                    bool,
                    bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize globals
////////////////////////////////////////////////////////////////////////////////

void TRI_InitServerGlobals (void);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global server id
////////////////////////////////////////////////////////////////////////////////

TRI_server_id_t TRI_GetIdServer (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief start the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StartServer (TRI_server_t*,
                     bool checkVersion,
                     bool performUpgrade);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes all databases
////////////////////////////////////////////////////////////////////////////////

int TRI_InitDatabasesServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StopServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication appliers
////////////////////////////////////////////////////////////////////////////////

void TRI_StopReplicationAppliersServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateCoordinatorDatabaseServer (TRI_server_t*,
                                         TRI_voc_tick_t,
                                         char const*,
                                         TRI_vocbase_defaults_t const*,
                                         TRI_vocbase_t**);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateDatabaseServer (TRI_server_t*,
                              TRI_voc_tick_t,
                              char const*,
                              TRI_vocbase_defaults_t const*,
                              TRI_vocbase_t**,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the ids of all local coordinator databases
/// the caller is responsible for freeing the result
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_voc_tick_t> TRI_GetIdsCoordinatorDatabaseServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdCoordinatorDatabaseServer (TRI_server_t*,
                                           TRI_voc_tick_t,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropDatabaseServer (TRI_server_t*,
                            char const*,
                            bool,
                            bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdDatabaseServer (TRI_server_t*,
                                TRI_voc_tick_t,
                                bool,
                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a coordinator database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseByIdCoordinatorDatabaseServer (TRI_server_t*,
                                                     TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief use a coordinator database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseCoordinatorDatabaseServer (TRI_server_t*,
                                                 char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief use a database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseDatabaseServer (TRI_server_t*,
                                      char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a database by its name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_LookupDatabaseByNameServer (TRI_server_t*,
                                               char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief use a database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseDatabaseByIdServer (TRI_server_t*,
                                          TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief release a previously used database
/// this will decrease the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseDatabaseServer (TRI_server_t*,
                                TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all databases a user can see
////////////////////////////////////////////////////////////////////////////////

int TRI_GetUserDatabasesServer (TRI_server_t*,
                                char const*,
                                std::vector<std::string>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all database names
////////////////////////////////////////////////////////////////////////////////

int TRI_GetDatabaseNamesServer (TRI_server_t*,
                                std::vector<std::string>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the defaults into the target
////////////////////////////////////////////////////////////////////////////////

void TRI_GetDatabaseDefaultsServer (TRI_server_t*,
                                    TRI_vocbase_defaults_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    tick functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NewTickServer (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, with lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTickServer (TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current tick counter
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_CurrentTickServer (void);

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_MSync (int,
                char const*,
                char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

int TRI_ChangeOperationModeServer (TRI_vocbase_operationmode_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_operationmode_e TRI_GetOperationModeServer ();

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
