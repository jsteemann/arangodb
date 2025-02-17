/*jshint esnext:true, -W051:true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {

var exports = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

if (global.ArangoError) {
  exports.ArangoError = global.ArangoError;
  delete global.ArangoError;
}
else {
  exports.ArangoError = function (error) {
    if (error !== undefined) {
      this.error = error.error;
      this.code = error.code;
      this.errorNum = error.errorNum;
      this.errorMessage = error.errorMessage;
    }

    this.message = this.toString();
  };

  exports.ArangoError.prototype = new Error();
}

exports.ArangoError.prototype._PRINT = function (context) {
  context.output += this.toString();
};

exports.ArangoError.prototype.toString = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage || this.message;

  return "[ArangoError " + errorNum + ": " + errorMessage + "]";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief SleepAndRequeue
////////////////////////////////////////////////////////////////////////////////

if (global.SleepAndRequeue) {
  exports.SleepAndRequeue = global.SleepAndRequeue;
  delete global.SleepAndRequeue;

  exports.SleepAndRequeue.prototype._PRINT = function (context) {
    context.output += this.toString();
  };

  exports.SleepAndRequeue.prototype.toString = function() {
    return "[SleepAndRequeue sleep: " + this.sleep + "]";
};

}
// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief threadNumber
////////////////////////////////////////////////////////////////////////////////

exports.threadNumber = 0;

if (global.THREAD_NUMBER) {
  exports.threadNumber = global.THREAD_NUMBER;
  delete global.THREAD_NUMBER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief developmentMode. this is only here for backwards compatibility
////////////////////////////////////////////////////////////////////////////////

exports.developmentMode = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfilePath
////////////////////////////////////////////////////////////////////////////////

if (global.LOGFILE_PATH) {
  exports.logfilePath = global.LOGFILE_PATH;
  delete global.LOGFILE_PATH;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet
////////////////////////////////////////////////////////////////////////////////

exports.quiet = false;

if (global.ARANGO_QUIET) {
  exports.quiet = global.ARANGO_QUIET;
  delete global.ARANGO_QUIET;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief valgrind
////////////////////////////////////////////////////////////////////////////////

exports.valgrind = false;

if (global.VALGRIND) {
  exports.valgrind = global.VALGRIND;
  delete global.VALGRIND;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief valgrind
////////////////////////////////////////////////////////////////////////////////

exports.coverage = false;

if (global.COVERAGE) {
  exports.coverage = global.COVERAGE;
  delete global.COVERAGE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief version
////////////////////////////////////////////////////////////////////////////////

exports.version = "unknown";

if (global.VERSION) {
  exports.version = global.VERSION;
  delete global.VERSION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief platform
////////////////////////////////////////////////////////////////////////////////

exports.platform = "unknown";

if (global.SYS_PLATFORM) {
  exports.platform = global.SYS_PLATFORM;
  delete global.SYS_PLATFORM;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesSentDistribution
////////////////////////////////////////////////////////////////////////////////

exports.bytesSentDistribution = [];

if (global.BYTES_SENT_DISTRIBUTION) {
  exports.bytesSentDistribution = global.BYTES_SENT_DISTRIBUTION;
  delete global.BYTES_SENT_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesReceivedDistribution
////////////////////////////////////////////////////////////////////////////////

exports.bytesReceivedDistribution = [];

if (global.BYTES_RECEIVED_DISTRIBUTION) {
  exports.bytesReceivedDistribution = global.BYTES_RECEIVED_DISTRIBUTION;
  delete global.BYTES_RECEIVED_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connectionTimeDistribution
////////////////////////////////////////////////////////////////////////////////

exports.connectionTimeDistribution = [];

if (global.CONNECTION_TIME_DISTRIBUTION) {
  exports.connectionTimeDistribution = global.CONNECTION_TIME_DISTRIBUTION;
  delete global.CONNECTION_TIME_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requestTimeDistribution
////////////////////////////////////////////////////////////////////////////////

exports.requestTimeDistribution = [];

if (global.REQUEST_TIME_DISTRIBUTION) {
  exports.requestTimeDistribution = global.REQUEST_TIME_DISTRIBUTION;
  delete global.REQUEST_TIME_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startupPath
////////////////////////////////////////////////////////////////////////////////

exports.startupPath = "";

if (global.STARTUP_PATH) {
  exports.startupPath = global.STARTUP_PATH;
  delete global.STARTUP_PATH;
}

if (exports.startupPath === "") {
  exports.startupPath = ".";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief configureEndpoint
////////////////////////////////////////////////////////////////////////////////

if (global.CONFIGURE_ENDPOINT) {
  exports.configureEndpoint = global.CONFIGURE_ENDPOINT;
  delete global.CONFIGURE_ENDPOINT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removeEndpoint
////////////////////////////////////////////////////////////////////////////////

if (global.REMOVE_ENDPOINT) {
  exports.removeEndpoint = global.REMOVE_ENDPOINT;
  delete global.REMOVE_ENDPOINT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief listEndpoints
////////////////////////////////////////////////////////////////////////////////

if (global.LIST_ENDPOINTS) {
  exports.listEndpoints = global.LIST_ENDPOINTS;
  delete global.LIST_ENDPOINTS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Decode
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_BASE64DECODE) {
  exports.base64Decode = global.SYS_BASE64DECODE;
  delete global.SYS_BASE64DECODE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Encode
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_BASE64ENCODE) {
  exports.base64Encode = global.SYS_BASE64ENCODE;
  delete global.SYS_BASE64ENCODE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugSegfault
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_SEGFAULT) {
  exports.debugSegfault = global.SYS_DEBUG_SEGFAULT;
  delete global.SYS_DEBUG_SEGFAULT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugSetFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_SET_FAILAT) {
  exports.debugSetFailAt = global.SYS_DEBUG_SET_FAILAT;
  delete global.SYS_DEBUG_SET_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugRemoveFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_REMOVE_FAILAT) {
  exports.debugRemoveFailAt = global.SYS_DEBUG_REMOVE_FAILAT;
  delete global.SYS_DEBUG_REMOVE_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugClearFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_CLEAR_FAILAT) {
  exports.debugClearFailAt = global.SYS_DEBUG_CLEAR_FAILAT;
  delete global.SYS_DEBUG_CLEAR_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugCanUseFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_CAN_USE_FAILAT) {
  exports.debugCanUseFailAt = global.SYS_DEBUG_CAN_USE_FAILAT;
  delete global.SYS_DEBUG_CAN_USE_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief download
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DOWNLOAD) {
  exports.download = global.SYS_DOWNLOAD;
  delete global.SYS_DOWNLOAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executeScript
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_EXECUTE) {
  exports.executeScript = global.SYS_EXECUTE;
  delete global.SYS_EXECUTE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getCurrentRequest
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GET_CURRENT_REQUEST) {
  exports.getCurrentRequest = global.SYS_GET_CURRENT_REQUEST;
  delete global.SYS_GET_CURRENT_REQUEST;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getCurrentResponse
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GET_CURRENT_RESPONSE) {
  exports.getCurrentResponse = global.SYS_GET_CURRENT_RESPONSE;
  delete global.SYS_GET_CURRENT_RESPONSE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extend
////////////////////////////////////////////////////////////////////////////////

exports.extend = function (target, source) {

  Object.getOwnPropertyNames(source)
  .forEach(function(propName) {
    Object.defineProperty(
      target,
      propName,
      Object.getOwnPropertyDescriptor(source, propName)
    );
  });

  return target;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief load
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_LOAD) {
  exports.load = global.SYS_LOAD;
  delete global.SYS_LOAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logLevel
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_LOG_LEVEL) {
  exports.logLevel = global.SYS_LOG_LEVEL;
  delete global.SYS_LOG_LEVEL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief md5
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_MD5) {
  exports.md5 = global.SYS_MD5;
  delete global.SYS_MD5;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomNumbers
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GEN_RANDOM_NUMBERS) {
  exports.genRandomNumbers = global.SYS_GEN_RANDOM_NUMBERS;
  delete global.SYS_GEN_RANDOM_NUMBERS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomAlphaNumbers
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GEN_RANDOM_ALPHA_NUMBERS) {
  exports.genRandomAlphaNumbers = global.SYS_GEN_RANDOM_ALPHA_NUMBERS;
  delete global.SYS_GEN_RANDOM_ALPHA_NUMBERS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomSalt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GEN_RANDOM_SALT) {
  exports.genRandomSalt = global.SYS_GEN_RANDOM_SALT;
  delete global.SYS_GEN_RANDOM_SALT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hmac
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_HMAC) {
  exports.hmac = global.SYS_HMAC;
  delete global.SYS_HMAC;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pbkdf2-hmac
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PBKDF2) {
  exports.pbkdf2 = global.SYS_PBKDF2;
  delete global.SYS_PBKDF2;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief createNonce
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_CREATE_NONCE) {
  exports.createNonce = global.SYS_CREATE_NONCE;
  delete global.SYS_CREATE_NONCE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checkAndMarkNonce
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_CHECK_AND_MARK_NONCE) {
  exports.checkAndMarkNonce = global.SYS_CHECK_AND_MARK_NONCE;
  delete global.SYS_CHECK_AND_MARK_NONCE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief output
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_OUTPUT) {
  exports.stdOutput = global.SYS_OUTPUT;
  exports.output = exports.stdOutput;
  delete global.SYS_OUTPUT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PARSE) {
  exports.parse = global.SYS_PARSE;
  delete global.SYS_PARSE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parseFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PARSE_FILE) {
  exports.parseFile = global.SYS_PARSE_FILE;
  delete global.SYS_PARSE_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PROCESS_STATISTICS) {
  exports.processStatistics = global.SYS_PROCESS_STATISTICS;
  delete global.SYS_PROCESS_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rand
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_RAND) {
  exports.rand = global.SYS_RAND;
  delete global.SYS_RAND;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha512
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA512) {
  exports.sha512 = global.SYS_SHA512;
  delete global.SYS_SHA512;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha384
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA384) {
  exports.sha384 = global.SYS_SHA384;
  delete global.SYS_SHA384;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA256) {
  exports.sha256 = global.SYS_SHA256;
  delete global.SYS_SHA256;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha224
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA224) {
  exports.sha224 = global.SYS_SHA224;
  delete global.SYS_SHA224;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha1
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA1) {
  exports.sha1 = global.SYS_SHA1;
  delete global.SYS_SHA1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief serverStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SERVER_STATISTICS) {
  exports.serverStatistics = global.SYS_SERVER_STATISTICS;
  delete global.SYS_SERVER_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SLEEP) {
  exports.sleep = global.SYS_SLEEP;
  delete global.SYS_SLEEP;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_TIME) {
  exports.time = global.SYS_TIME;
  delete global.SYS_TIME;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_WAIT) {
  exports.wait = global.SYS_WAIT;
  delete global.SYS_WAIT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief importCsvFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_IMPORT_CSV_FILE) {
  exports.importCsvFile = global.SYS_IMPORT_CSV_FILE;
  delete global.SYS_IMPORT_CSV_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief importJsonFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_IMPORT_JSON_FILE) {
  exports.importJsonFile = global.SYS_IMPORT_JSON_FILE;
  delete global.SYS_IMPORT_JSON_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processCsvFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PROCESS_CSV_FILE) {
  exports.processCsvFile = global.SYS_PROCESS_CSV_FILE;
  delete global.SYS_PROCESS_CSV_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processJsonFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PROCESS_JSON_FILE) {
  exports.processJsonFile = global.SYS_PROCESS_JSON_FILE;
  delete global.SYS_PROCESS_JSON_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clientStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_CLIENT_STATISTICS) {
  exports.clientStatistics = global.SYS_CLIENT_STATISTICS;
  delete global.SYS_CLIENT_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief httpStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_HTTP_STATISTICS) {
  exports.httpStatistics = global.SYS_HTTP_STATISTICS;
  delete global.SYS_HTTP_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executeExternal
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_EXECUTE_EXTERNAL) {
  exports.executeExternal = global.SYS_EXECUTE_EXTERNAL;
  delete global.SYS_EXECUTE_EXTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executeExternalAndWait - instantly waits for the exit, returns
/// joint result.
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_EXECUTE_EXTERNAL_AND_WAIT) {
  exports.executeExternalAndWait = global.SYS_EXECUTE_EXTERNAL_AND_WAIT;
  delete global.SYS_EXECUTE_EXTERNAL_AND_WAIT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief killExternal
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_KILL_EXTERNAL) {
  exports.killExternal = global.SYS_KILL_EXTERNAL;
  delete global.SYS_KILL_EXTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief statusExternal
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_STATUS_EXTERNAL) {
  exports.statusExternal = global.SYS_STATUS_EXTERNAL;
  delete global.SYS_STATUS_EXTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registerTask
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_REGISTER_TASK) {
  exports.registerTask = global.SYS_REGISTER_TASK;
  delete global.SYS_REGISTER_TASK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisterTask
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_UNREGISTER_TASK) {
  exports.unregisterTask = global.SYS_UNREGISTER_TASK;
  delete global.SYS_UNREGISTER_TASK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getTasks
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GET_TASK) {
  exports.getTask = global.SYS_GET_TASK;
  delete global.SYS_GET_TASK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief testPort
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_TEST_PORT) {
  exports.testPort = global.SYS_TEST_PORT;
  delete global.SYS_TEST_PORT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief isIP
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_IS_IP) {
  exports.isIP = global.SYS_IS_IP;
  delete global.SYS_IS_IP;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief unitTests
////////////////////////////////////////////////////////////////////////////////

exports.unitTests = function () {
  return global.SYS_UNIT_TESTS;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief setUnitTestsResult
////////////////////////////////////////////////////////////////////////////////

exports.setUnitTestsResult = function (value) {
  global.SYS_UNIT_TESTS_RESULT = value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                     Commandline argument handling
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief structured to flat commandline arguments
/// @param longOptsEqual whether long-options are in the type --opt=value 
///                      or --opt value
////////////////////////////////////////////////////////////////////////////////

exports.toArgv = function (structure, longOptsEqual) {
  if (typeof(longOptsEqual) === 'undefined') {
    longOptsEqual = false;
  }
  var vec = [];
  for (var key in structure) {
    if (structure.hasOwnProperty(key)) {
      if (key === 'commandSwitches') {
        var multivec = "";
        for (var i = 0; i < structure[key].length; i ++) {
          if (structure[key][i].length > 1) {
            vec.push(structure[key][i]);
          }
          else {
            multivec += structure[key][i];
          }
        }
        if (multivec.length > 0) {
          vec.push(multivec);
        }
      }
      else if (key === 'flatCommands') {
        vec = vec.concat(structure[key]);
      }
      else {
        if (longOptsEqual) {
          vec.push('--' + key + '=' + structure[key]);
        }
        else {
          vec.push('--' + key);
          if (structure[key] !== false) {
            if (structure[key] !== true) {
              vec.push(structure[key]);
            }
            else {
              vec.push("true");
            }
          }
          else {
            vec.push("false");
          }
        }
      }
    }
  }
  return vec;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief argv to structured
////////////////////////////////////////////////////////////////////////////////

exports.parseArgv = function (argv, startOffset) {
  function setOption(ret, option, value) {
    if (option.indexOf(':') > 0) {
      var n = option.indexOf(':');
      var topOption = option.slice(0, n);
      if (! ret.hasOwnProperty(topOption)) {
        ret[topOption] = {};
      }
      setOption(ret[topOption], option.slice(n + 1, option.length), value);
    }
    else if (argv[i + 1] === 'true') {
      ret[option] = true;
    }
    else if (argv[i + 1] === 'false') {
      ret[option] = false;
    }
    else if (! isNaN(argv[i + 1])) {
      ret[option] = parseInt(argv[i + 1]);
    }
    else {
      ret[option] = argv[i + 1];
    }
  }
  function setSwitch(ret, option) {
    if (! ret.hasOwnProperty('commandSwitches')) {
      ret.commandSwitches = [];
    }
    ret.commandSwitches.push(option);
  }

  function setSwitchVec(ret, option) {
    for (var i = 0; i < option.length; i++) {
      setSwitch(ret, option[i]);
    }
  }

  function setFlatCommand(ret, thisString) {
    if (! ret.hasOwnProperty('flatCommands')) {
      ret.flatCommands = [];
    }
    ret.flatCommands.push(thisString);
  }

  var inFlat = false;
  var ret = {};
  for (var i = startOffset; i < argv.length; i++) {
    var thisString = argv[i];
    if (! inFlat) {
      if ((thisString.length > 2) &&
          (thisString.slice(0,2) === '--')) {
        var option = thisString.slice(2, thisString.length);
        if ((argv.length > i) &&
            (argv[i + 1].slice(0,1) !== '-')) {
          setOption(ret, option, argv[i + 1]);
          i++;
        }
        else {
          setSwitch(ret, option);
        }
      }
      else if (thisString === '--') {
        inFlat = true;
      }
      else if ((thisString.length > 1) &&
              (thisString.slice(0, 1) === '-')) {
        setSwitchVec(ret, thisString.slice(1, thisString.length));
      }
      else {
        setFlatCommand(ret, thisString);
      }
    }
    else {
      setFlatCommand(ret, thisString);
    }
  }
  return ret;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                          PRINTING
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                         public printing variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief COLORS
////////////////////////////////////////////////////////////////////////////////

exports.COLORS = {};

if (global.COLORS) {
  exports.COLORS = global.COLORS;
  delete global.COLORS;
}
else {
  [ 'COLOR_RED', 'COLOR_BOLD_RED', 'COLOR_GREEN', 'COLOR_BOLD_GREEN',
    'COLOR_BLUE', 'COLOR_BOLD_BLUE', 'COLOR_YELLOW', 'COLOR_BOLD_YELLOW',
    'COLOR_WHITE', 'COLOR_BOLD_WHITE', 'COLOR_CYAN', 'COLOR_BOLD_CYAN',
    'COLOR_MAGENTA', 'COLOR_BOLD_MAGENTA', 'COLOR_BLACK', 'COLOR_BOLD_BLACK',
    'COLOR_BLINK', 'COLOR_BRIGHT', 'COLOR_RESET' ].forEach(function(color) {
      exports.COLORS[color] = '';
    });
}

exports.COLORS.COLOR_PUNCTUATION = exports.COLORS.COLOR_RESET;
exports.COLORS.COLOR_STRING = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_NUMBER = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_INDEX = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_TRUE = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_FALSE = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_NULL = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_UNDEFINED = exports.COLORS.COLOR_BRIGHT;

// -----------------------------------------------------------------------------
// --SECTION--                                        private printing variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief quote cache
////////////////////////////////////////////////////////////////////////////////

var characterQuoteCache = {
  '\b': '\\b', // ASCII 8, Backspace
  '\t': '\\t', // ASCII 9, Tab
  '\n': '\\n', // ASCII 10, Newline
  '\f': '\\f', // ASCII 12, Formfeed
  '\r': '\\r', // ASCII 13, Carriage Return
  '\"': '\\"',
  '\\': '\\\\'
};

////////////////////////////////////////////////////////////////////////////////
/// @brief colors
////////////////////////////////////////////////////////////////////////////////

var colors = exports.COLORS;

////////////////////////////////////////////////////////////////////////////////
/// @brief useColor
////////////////////////////////////////////////////////////////////////////////

var useColor = false;

if (global.COLOR_OUTPUT) {
  useColor = global.COLOR_OUTPUT;
  delete global.COLOR_OUTPUT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usePrettyPrint
////////////////////////////////////////////////////////////////////////////////

var usePrettyPrint = false;

if (global.PRETTY_PRINT) {
  usePrettyPrint = global.PRETTY_PRINT;
  delete global.PRETTY_PRINT;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        private printing functions
// -----------------------------------------------------------------------------

var printRecursive;

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a single character
////////////////////////////////////////////////////////////////////////////////

function quoteSingleJsonCharacter (c) {

  if (characterQuoteCache.hasOwnProperty(c)) {
    return characterQuoteCache[c];
  }

  var charCode = c.charCodeAt(0);
  var result;

  if (charCode < 16) {
    result = '\\u000';
  }
  else if (charCode < 256) {
    result = '\\u00';
  }
  else if (charCode < 4096) {
    result = '\\u0';
  }
  else {
    result = '\\u';
  }

  result += charCode.toString(16);
  characterQuoteCache[c] = result;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a string character
////////////////////////////////////////////////////////////////////////////////

var quotable = /[\\\"\x00-\x1f]/g;

function quoteJsonString (str) {

  return '"' + str.replace(quotable, quoteSingleJsonCharacter) + '"';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the ident for pretty printing
////////////////////////////////////////////////////////////////////////////////

function printIndent (context) {

  var j;
  var indent = "";

  if (context.prettyPrint) {
    indent += "\n";

    for (j = 0; j < context.level; ++j) {
      indent += "  ";
    }
  }

  context.output += indent;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

function printArray (object, context) {

  var useColor = context.useColor;

  if (object.length === 0) {
    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += "[ ]";

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }
  }
  else {
    var i;

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += "[";

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    var newLevel = context.level + 1;
    var sep = " ";

    context.level = newLevel;

    for (i = 0;  i < object.length;  i++) {
      if (useColor) {
        context.output += colors.COLOR_PUNCTUATION;
      }

      context.output += sep;

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }

      printIndent(context);

      var path = context.path;
      context.path += "[" + i + "]";

      printRecursive(object[i], context);

      if (context.emit && context.output.length >= context.emit) {
        exports.output(context.output);
        context.output = "";
      }

      context.path = path;
      sep = ", ";
    }

    context.level = newLevel - 1;
    context.output += " ";

    printIndent(context);

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += "]";

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an object
////////////////////////////////////////////////////////////////////////////////

function printObject (object, context) {

  var useColor = context.useColor;
  var sep = " ";

  if (useColor) {
    context.output += colors.COLOR_PUNCTUATION;
  }

  context.output += "{";

  if (useColor) {
    context.output += colors.COLOR_RESET;
  }

  var newLevel = context.level + 1;

  context.level = newLevel;

  var keys;
  try {
    keys = Object.keys(object);
  }
  catch (err) {
    // ES6 proxy objects don't support key enumeration
    keys = [ ];
  }
  var i, n = keys.length;

  for (i = 0; i < n; ++i) {
    var k = keys[i];
    var val = object[k];

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += sep;

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    printIndent(context);

    if (useColor) {
      context.output += colors.COLOR_INDEX;
    }

    context.output += quoteJsonString(k);

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    context.output += " : ";

    var path = context.path;
    context.path += "[" + k + "]";

    printRecursive(val, context);

    context.path = path;
    sep = ", ";

    if (context.emit && context.output.length >= context.emit) {
      exports.output(context.output);
      context.output = "";
    }
  }

  context.level = newLevel - 1;
  context.output += " ";

  printIndent(context);

  if (useColor) {
    context.output += colors.COLOR_PUNCTUATION;
  }

  context.output += "}";

  if (useColor) {
    context.output += colors.COLOR_RESET;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output without a new-line
////////////////////////////////////////////////////////////////////////////////

var funcRE = /function ([^\(]*)?\(\) \{ \[native code\] \}/;
var func2RE = /function ([^\(]*)?\((.*)\) \{/;

exports.printRecursive = printRecursive = function (value, context) {

  var useColor = context.useColor;
  var customInspect = context.customInspect;
  var useToString = context.useToString;
  var limitString = context.limitString;
  var showFunction = context.showFunction;

  if (typeof context.seen === "undefined") {
    context.seen = [];
    context.names = [];
  }

  var p = context.seen.indexOf(value);

  if (0 <= p) {
    context.output += context.names[p];
  }
  else {
    if (value && (value instanceof Object || (typeof value === 'object' && Object.getPrototypeOf(value) === null))) {
      context.seen.push(value);
      context.names.push(context.path);
      if (customInspect && typeof value._PRINT === "function") {
        value._PRINT(context);

        if (context.emit && context.output.length >= context.emit) {
          exports.output(context.output);
          context.output = "";
        }
      }
      else if (value instanceof Array) {
        printArray(value, context);
      }
      else if (
        value.toString === Object.prototype.toString
        || (typeof value === 'object' && Object.getPrototypeOf(value) === null)
      ) {
        var handled = false;
        try {
          if (value instanceof Set ||
              value instanceof Map ||
              value instanceof WeakSet ||
              value instanceof WeakMap ||
              typeof value[Symbol.iterator] === "function") {
            // ES6 iterators
            context.output += value.toString();
            handled = true;
          }
        }
        catch (err) {
          // ignore any errors thrown above, and simply fall back to normal printing
        }

        if (! handled) {
          // all other objects
          printObject(value, context);
        }

        if (context.emit && context.output.length >= context.emit) {
          exports.output(context.output);
          context.output = "";
        }
      }
      else if (typeof value === "function") {
        // it's possible that toString() throws, and this looks quite ugly
        try {
          var s = value.toString();

          if (0 < context.level && ! showFunction) {
            var a = s.split("\n");
            var f = a[0];

            var m = funcRE.exec(f);

            if (m !== null) {
              if (m[1] === undefined) {
                context.output += 'function { [native code] }';
              }
              else {
                context.output += 'function ' + m[1] + ' { [native code] }';
              }
            }
            else {
              m = func2RE.exec(f);

              if (m !== null) {
                if (m[1] === undefined) {
                  context.output += 'function ' + '(' + m[2] +') { ... }';
                }
                else {
                  context.output += 'function ' + m[1] + ' (' + m[2] +') { ... }';
                }
              }
              else {
                f = f.substr(8, f.length - 10).trim();
                context.output += '[Function "' + f + '" ...]';
              }
            }
          }
          else {
            context.output += s;
          }
        }
        catch (e1) {
          exports.stdOutput(String(e1));
          context.output += "[Function]";
        }
      }
      else if (useToString && typeof value.toString === "function") {
        try {
          context.output += value.toString();
        }
        catch (e2) {
          context.output += "[Object ";
          printObject(value, context);
          context.output += "]";
        }
      }
      else {
        context.output += "[Object ";
        printObject(value, context);
        context.output += "]";
      }
    }
    else if (value === undefined) {
      if (useColor) {
        context.output += colors.COLOR_UNDEFINED;
      }

      context.output += "undefined";

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (typeof(value) === "string") {
      if (useColor) {
        context.output += colors.COLOR_STRING;
      }

      if (limitString) {
        if (limitString < value.length) {
          value = value.substr(0, limitString) + "...";
        }
      }

      context.output += quoteJsonString(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (typeof(value) === "boolean") {
      if (useColor) {
        context.output += value ? colors.COLOR_TRUE : colors.COLOR_FALSE;
      }

      context.output += String(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (typeof(value) === "number") {
      if (useColor) {
        context.output += colors.COLOR_NUMBER;
      }

      context.output += String(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (value === null) {
      if (useColor) {
        context.output += colors.COLOR_NULL;
      }

      context.output += String(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    /* jshint notypeof: true */
    else if (typeof(value) === "symbol") {
    /* jshint notypeof: false */
      // handle ES6 symbols
      if (useColor) {
        context.output += colors.COLOR_NULL;
      }

      context.output += value.toString();

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else {
      context.output += String(value);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief buffers output instead of printing it
////////////////////////////////////////////////////////////////////////////////

function bufferOutput () {
  var i;

  for (i = 0;  i < arguments.length;  ++i) {
    var value = arguments[i];
    var text;

    if (value === null) {
      text = "null";
    }
    else if (value === undefined) {
      text = "undefined";
    }
    else if (typeof(value) === "object") {
      try {
        text = JSON.stringify(value);
      }
      catch (err) {
        text = String(value);
      }
    }
    else {
      text = String(value);
    }

    exports.outputBuffer += text;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all arguments
///
/// @FUN{exports.printShell(@FA{arg1}, @FA{arg2}, @FA{arg3}, ...)}
///
/// Only available in shell mode.
///
/// Prints the arguments. If an argument is an object having a function
/// @FN{_PRINT}, then this function is called. A final newline is printed.
////////////////////////////////////////////////////////////////////////////////

function printShell () {

  var output = exports.output;
  var i;

  for (i = 0;  i < arguments.length;  ++i) {
    if (0 < i) {
      output(" ");
    }

    if (typeof(arguments[i]) === "string") {
      output(arguments[i]);
    }
    else {
      var context = {
        customInspect: true,
        emit: 16384,
        level: 0,
        limitString: 80,
        names: [],
        output: "",
        path: "~",
        prettyPrint: usePrettyPrint,
        seen: [],
        showFunction: false,
        useColor: useColor,
        useToString: true
      };

      printRecursive(arguments[i], context);

      output(context.output);
    }
  }

  output("\n");
}

// -----------------------------------------------------------------------------
// --SECTION--                                         public printing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief flatten
////////////////////////////////////////////////////////////////////////////////

var hasOwnProperty = Function.prototype.call.bind(Object.prototype.hasOwnProperty);

exports.flatten = function (obj, seen) {

  if (!obj || (typeof obj !== 'object' && typeof obj !== 'function')) {
    return obj;
  }

  if (obj instanceof Date) {
    return obj.toJSON();
  }

  if (!seen) {
    seen = [];
  }

  var result = Object.create(null),
    src = obj,
    keys,
    i,
    key,
    val;

  if (typeof obj === 'function') {
    result.__exec = String(obj);
  }

  while (src) {
    if (
      seen.indexOf(src) !== -1
        || (obj.constructor && src === obj.constructor.prototype)
    ) {
      break;
    }
    seen.push(src);
    keys = Object.getOwnPropertyNames(src);
    for (i = 0; i < keys.length; i++) {
      key = keys[i];
      if (typeof src !== 'function' || (
        key !== 'arguments' && key !== 'caller' && key !== 'callee'
      )) {
        if (key.charAt(0) !== '_' && !hasOwnProperty(result, key)) {
          val = obj[key];
          if (seen.indexOf(val) !== -1 && (
            typeof val === 'object' || typeof val === 'function'
          )) {
            result[key] = '[Circular]';
          } else {
            result[key] = exports.flatten(val, seen);
          }
        }
      }
    }
    src = Object.getPrototypeOf(src);
  }

  if (obj.constructor && obj.constructor.name) {
    if (obj instanceof Error && obj.name === Error.name) {
      result.name = obj.constructor.name;
    } else if (!hasOwnProperty(result, 'constructor')) {
      result.constructor = {name: obj.constructor.name};
    }
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inspect
////////////////////////////////////////////////////////////////////////////////

exports.inspect = function (object, options) {

  var context = {
    customInspect: options && options.customInspect,
    emit: false,
    level: 0,
    limitString: false,
    names: [],
    output: "",
    prettyPrint: true,
    path: "~",
    seen: [],
    showFunction: true,
    useColor: false,
    useToString: false
  };

  if (options && options.hasOwnProperty("prettyPrint")) {
    context.prettyPrint = options.prettyPrint;
  }

  printRecursive(object, context);

  return context.output;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sprintf
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SPRINTF) {
  exports.sprintf = global.SYS_SPRINTF;
  delete global.SYS_SPRINTF;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

var sprintf = exports.sprintf;

exports.printf = function () {
  exports.output(sprintf.apply(sprintf, arguments));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

if (typeof exports.printBrowser === "function") {
  exports.printShell = printShell;
  exports.print = exports.printBrowser;
}
else {
  exports.print = printShell;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief printObject
////////////////////////////////////////////////////////////////////////////////

exports.printObject = printObject;

exports.isCaptureMode = function() {
  return exports.output === bufferOutput;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief startCaptureMode
////////////////////////////////////////////////////////////////////////////////

exports.startCaptureMode = function () {
  var old = exports.output;

  exports.outputBuffer = "";
  exports.output = bufferOutput;

  return old;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stopCaptureMode
////////////////////////////////////////////////////////////////////////////////

exports.stopCaptureMode = function (old) {
  var buffer = exports.outputBuffer;

  exports.outputBuffer = "";
  if (old !== undefined) {
    exports.output = old;
  }
  else {
    exports.output = exports.stdOutput;
  }
  
  return buffer;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief startPager
////////////////////////////////////////////////////////////////////////////////

exports.startPager = function () {};

if (global.SYS_START_PAGER) {
  exports.startPager = global.SYS_START_PAGER;
  delete global.SYS_START_PAGER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stopPager
////////////////////////////////////////////////////////////////////////////////

exports.stopPager = function () {};

if (global.SYS_STOP_PAGER) {
  exports.stopPager = global.SYS_STOP_PAGER;
  delete global.SYS_STOP_PAGER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startPrettyPrint
////////////////////////////////////////////////////////////////////////////////

exports.startPrettyPrint = function (silent) {
  if (! usePrettyPrint && !silent) {
    exports.print("using pretty printing");
  }

  usePrettyPrint = true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stopPrettyPrint
////////////////////////////////////////////////////////////////////////////////

exports.stopPrettyPrint = function (silent) {
  if (usePrettyPrint && !silent) {
    exports.print("disabled pretty printing");
  }

  usePrettyPrint = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief startColorPrint
////////////////////////////////////////////////////////////////////////////////

exports.startColorPrint = function (color, silent) {
  var schemes = {
    arangodb: {
      COLOR_PUNCTUATION: exports.COLORS.COLOR_RESET,
      COLOR_STRING: exports.COLORS.COLOR_BOLD_MAGENTA,
      COLOR_NUMBER: exports.COLORS.COLOR_BOLD_GREEN,
      COLOR_INDEX: exports.COLORS.COLOR_BOLD_CYAN,
      COLOR_TRUE: exports.COLORS.COLOR_BOLD_MAGENTA,
      COLOR_FALSE: exports.COLORS.COLOR_BOLD_MAGENTA,
      COLOR_NULL: exports.COLORS.COLOR_BOLD_YELLOW,
      COLOR_UNDEFINED: exports.COLORS.COLOR_BOLD_YELLOW
    }
  };

  if (!useColor && !silent) {
    exports.print("starting color printing");
  }

  if (color === undefined || color === null) {
    color = null;
  }
  else if (typeof color === "string") {
    color = color.toLowerCase();
    var c;

    if (schemes.hasOwnProperty(color)) {
      colors = schemes[color];

      for (c in exports.COLORS) {
        if (exports.COLORS.hasOwnProperty(c) && ! colors.hasOwnProperty(c)) {
          colors[c] = exports.COLORS[c];
        }
      }
    }
    else {
      colors = exports.COLORS;

      var setColor = function (key) {
        [ 'COLOR_STRING', 'COLOR_NUMBER', 'COLOR_INDEX', 'COLOR_TRUE',
          'COLOR_FALSE', 'COLOR_NULL', 'COLOR_UNDEFINED' ].forEach(function (what) {
          colors[what] = exports.COLORS[key];
        });
      };

      for (c in exports.COLORS) {
        if (exports.COLORS.hasOwnProperty(c) &&
            c.replace(/^COLOR_/, '').toLowerCase() === color) {
          setColor(c);
          break;
        }
      }
    }
  }

  useColor = true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stopColorPrint
////////////////////////////////////////////////////////////////////////////////

exports.stopColorPrint = function (silent) {
  if (useColor && !silent) {
    exports.print("disabled color printing");
  }

  useColor = false;
};

// -----------------------------------------------------------------------------
// --SECTION--                                          public utility functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief env
////////////////////////////////////////////////////////////////////////////////

if (typeof ENV !== 'undefined') {
  exports.env = new global.ENV();
  delete global.ENV;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief options
////////////////////////////////////////////////////////////////////////////////

if (typeof SYS_OPTIONS !== 'undefined') {
  exports.options = global.SYS_OPTIONS;
  delete global.SYS_OPTIONS;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         global printing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

global.print = function print () {
  var internal = require("internal");
  internal.print.apply(internal.print, arguments);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

global.printf = function printf () {
  var internal = require("internal");
  internal.printf.apply(internal.printf, arguments);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print_plain
////////////////////////////////////////////////////////////////////////////////

global.print_plain = function print_plain () {
  var output = require("internal").output;
  var printRecursive = require("internal").printRecursive;
  var i;

  for (i = 0;  i < arguments.length;  ++i) {
    if (0 < i) {
      output(" ");
    }

    if (typeof(arguments[i]) === "string") {
      output(arguments[i]);
    }
    else {
      var context = {
        names: [],
        seen: [],
        path: "~",
        level: 0,
        output: "",
        prettyPrint: false,
        useColor: false,
        customInspect: true
      };

      printRecursive(arguments[i], context);

      output(context.output);
    }
  }

  output("\n");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief start_pretty_print
////////////////////////////////////////////////////////////////////////////////

global.start_pretty_print = function start_pretty_print () {
  require("internal").startPrettyPrint();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop_pretty_print
////////////////////////////////////////////////////////////////////////////////

global.stop_pretty_print = function stop_pretty_print () {
  require("internal").stopPrettyPrint();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief start_color_print
////////////////////////////////////////////////////////////////////////////////

global.start_color_print = function start_color_print (color) {
  require("internal").startColorPrint(color, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop_color_print
////////////////////////////////////////////////////////////////////////////////

global.stop_color_print = function stop_color_print () {
  require("internal").stopColorPrint();
};

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
