/*jshint strict: false, unused: false */
/*global TRANSACTION */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDatabase
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                    ArangoDatabase
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

exports.ArangoDatabase = internal.ArangoDatabase;

var ArangoDatabase = exports.ArangoDatabase;

// must called after export
var ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;
var ArangoError = require("org/arangodb").ArangoError;
var ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._PRINT = function (context) {
  context.output += this.toString();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief strng representation of a database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype.toString = function(seen, path, names, level) {
  return "[ArangoDatabase \"" + this._name() + "\"]";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new statement
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createStatement = function (data) {
  return new ArangoStatement(this, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create and execute a new statement
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._query = function (query, bindVars, cursorOptions, options) {
  if (typeof query === 'object' && query !== null && arguments.length === 1) {
    return new ArangoStatement(this, query).execute();
  }

  var payload = {
    query: query,
    bindVars: bindVars || undefined,
    count: (cursorOptions && cursorOptions.count) || false,
    batchSize: (cursorOptions && cursorOptions.batchSize) || undefined,
    options: options || undefined,
    cache: (options && options.cache) || undefined
  };
  return new ArangoStatement(this, payload).execute();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      transactions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a transaction
/// @startDocuBlock executeTransaction
/// `db._executeTransaction(object)`
///
/// Executes a server-side transaction, as specified by *object*.
///
/// *object* must have the following attributes:
/// - *collections*: a sub-object that defines which collections will be
///   used in the transaction. *collections* can have these attributes:
///   - *read*: a single collection or a list of collections that will be
///     used in the transaction in read-only mode
///   - *write*: a single collection or a list of collections that will be
///     used in the transaction in write or read mode.
/// - *action*: a Javascript function or a string with Javascript code
///   containing all the instructions to be executed inside the transaction.
///   If the code runs through successfully, the transaction will be committed
///   at the end. If the code throws an exception, the transaction will be
///   rolled back and all database operations will be rolled back.
///
/// Additionally, *object* can have the following optional attributes:
/// - *waitForSync*: boolean flag indicating whether the transaction
///   is forced to be synchronous.
/// - *lockTimeout*: a numeric value that can be used to set a timeout for
///   waiting on collection locks. If not specified, a default value will be
///   used. Setting *lockTimeout* to *0* will make ArangoDB not time
///   out waiting for a lock.
/// - *params*: optional arguments passed to the function specified in
///   *action*.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._executeTransaction = function (data) {
  return TRANSACTION(data);
};

// -----------------------------------------------------------------------------
// --SECTION--                                              collection functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
/// @startDocuBlock collectionDatabaseDrop
/// `db._drop(collection)`
///
/// Drops a *collection* and all its indexes.
///
/// `db._drop(collection-identifier)`
///
/// Drops a collection identified by *collection-identifier* and all its
/// indexes. No error is thrown if there is no such collection.
///
/// `db._drop(collection-name)`
///
/// Drops a collection named *collection-name* and all its indexes. No error
/// is thrown if there is no such collection.
///
/// *Examples*
///
/// Drops a collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDrop}
/// ~ db._create("example");
///   col = db.example;
///   db._drop(col);
///   col;
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Drops a collection identified by name:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDropName}
/// ~ db._create("example");
///   col = db.example;
///   db._drop("example");
///   col;
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._drop = function(name) {
  var collection = name;

  if (! (name instanceof ArangoCollection)) {
    collection = internal.db._collection(name);
  }

  if (collection === null) {
    return;
  }

  return collection.drop();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
/// @startDocuBlock collectionDatabaseTruncate
/// `db._truncate(collection)`
///
/// Truncates a *collection*, removing all documents but keeping all its
/// indexes.
///
/// `db._truncate(collection-identifier)`
///
/// Truncates a collection identified by *collection-identified*. No error is
/// thrown if there is no such collection.
///
/// `db._truncate(collection-name)`
///
/// Truncates a collection named *collection-name*. No error is thrown if
/// there is no such collection.
///
/// @EXAMPLES
///
/// Truncates a collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseTruncate}
/// ~ db._create("example");
///   col = db.example;
///   col.save({ "Hello" : "World" });
///   col.count();
///   db._truncate(col);
///   col.count();
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Truncates a collection identified by name:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseTruncateName}
/// ~ db._create("example");
///   col = db.example;
///   col.save({ "Hello" : "World" });
///   col.count();
///   db._truncate("example");
///   col.count();
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._truncate = function(name) {
  var collection = name;

  if (! (name instanceof ArangoCollection)) {
    collection = internal.db._collection(name);
  }

  if (collection === null) {
    return;
  }

  collection.truncate();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   index functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief index id regex
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an index
/// @startDocuBlock IndexVerify
///
/// So you've created an index, and since its maintainance isn't for free,
/// you definitely want to know whether your Query can utilize it.
///
/// You can use explain to verify whether **skiplist** or **hash indices** are used
/// (if you ommit `colors: false` you will get nice colors in ArangoShell):
///
/// @EXAMPLE_ARANGOSH_OUTPUT{IndexVerify}
/// ~db._create("example");
/// var explain = require("org/arangodb/aql/explainer").explain;
/// db.example.ensureSkiplist("a", "b");
/// explain("FOR doc IN example FILTER doc.a < 23 RETURN doc", {colors:false});
/// ~db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.indexRegex = /^([a-zA-Z0-9\-_]+)\/([0-9]+)$/;

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an index
/// @startDocuBlock IndexHandle
/// `db._index(index-handle)`
///
/// Returns the index with *index-handle* or null if no such index exists.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{IndexHandle}
/// ~db._create("example");
/// db.example.ensureSkiplist("a", "b");
/// var indexInfo = db.example.getIndexes().map(function(x) { return x.id; });
/// indexInfo;
/// db._index(indexInfo[0])
/// db._index(indexInfo[1])
/// ~db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._index = function(id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var pa = ArangoDatabase.indexRegex.exec(id);
  var err;

  if (pa === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message;
    throw err;
  }

  var col = this._collection(pa[1]);

  if (col === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message;
    throw err;
  }

  var indexes = col.getIndexes();
  var i;

  for (i = 0;  i < indexes.length;  ++i) {
    var index = indexes[i];

    if (index.id === id) {
      return index;
    }
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
/// @startDocuBlock dropIndex
/// `db._dropIndex(index)`
///
/// Drops the *index*.  If the index does not exist, then *false* is
/// returned. If the index existed and was dropped, then *true* is
/// returned.
///
/// `db._dropIndex(index-handle)`
///
/// Drops the index with *index-handle*.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{dropIndex}
/// ~db._create("example");
/// db.example.ensureSkiplist("a", "b");
/// var indexInfo = db.example.getIndexes();
/// indexInfo;
/// db._dropIndex(indexInfo[0])
/// db._dropIndex(indexInfo[1].id)
/// indexInfo = db.example.getIndexes();
/// ~db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._dropIndex = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var pa = ArangoDatabase.indexRegex.exec(id);
  var err;

  if (pa === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message;
    throw err;
  }

  var col = this._collection(pa[1]);

  if (col === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message;
    throw err;
  }

  return col.dropIndex(id);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                endpoint functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of all endpoints
/// @startDocuBlock listEndpoints
/// `db._listEndpoints()`
///
/// Returns a list of all endpoints and their mapped databases.
///
/// Please note that managing endpoints can only be performed from out of the
/// *_system* database. When not in the default database, you must first switch
/// to it using the "db._useDatabase" method.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._listEndpoints = function () {
  return internal._listEndpoints();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
