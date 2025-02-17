////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log storage allocator thread
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

#include "AllocatorThread.h"
#include "Basics/logging.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                             class AllocatorThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the allocator thread when idle
////////////////////////////////////////////////////////////////////////////////

const uint64_t AllocatorThread::Interval = 500 * 1000;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the allocator thread
////////////////////////////////////////////////////////////////////////////////

AllocatorThread::AllocatorThread (LogfileManager* logfileManager)
  : Thread("WalAllocator"),
    _logfileManager(logfileManager),
    _condition(),
    _recoveryLock(),
    _requestedSize(0),
    _stop(0),
    _inRecovery(true),
    _allocatorResultCondition(),
    _allocatorResult(TRI_ERROR_NO_ERROR) {

  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the allocator thread
////////////////////////////////////////////////////////////////////////////////

AllocatorThread::~AllocatorThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the collector result
////////////////////////////////////////////////////////////////////////////////

int AllocatorThread::waitForResult (uint64_t timeout) {    
  CONDITION_LOCKER(guard, _allocatorResultCondition);

  if (_allocatorResult == TRI_ERROR_NO_ERROR) { 
    if (guard.wait(timeout)) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }
 
  return _allocatorResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the allocator thread
////////////////////////////////////////////////////////////////////////////////

void AllocatorThread::stop () {
  if (_stop > 0) {
    return;
  }

  _stop = 1;
  _condition.signal();

  while (_stop != 2) {
    usleep(10000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the creation of a new logfile
////////////////////////////////////////////////////////////////////////////////

void AllocatorThread::signal (uint32_t markerSize) {
  CONDITION_LOCKER(guard, _condition);

  if (_requestedSize == 0 ||
      markerSize > _requestedSize) {
    // logfile must be as big as the requested marker
    _requestedSize = markerSize;
  }

  guard.signal();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new reserve logfile
////////////////////////////////////////////////////////////////////////////////

int AllocatorThread::createReserveLogfile (uint32_t size) {
  return _logfileManager->createReserveLogfile(size);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void AllocatorThread::run () {
  while (_stop == 0) {
    uint32_t requestedSize = 0;
        
    {
      CONDITION_LOCKER(guard, _condition);
      requestedSize = _requestedSize;
      _requestedSize = 0;
    }

    int res = TRI_ERROR_NO_ERROR;

    try {
      if (requestedSize == 0 &&
          ! inRecovery() &&
          ! _logfileManager->hasReserveLogfiles()) {
        // only create reserve files if we are not in the recovery mode
        res = createReserveLogfile(0);

        if (res == TRI_ERROR_NO_ERROR) {
          continue;
        }
        
        LOG_ERROR("unable to create new WAL reserve logfile for sized marker: %s", 
                  TRI_errno_string(res));
      }
      else if (requestedSize > 0 &&
              _logfileManager->logfileCreationAllowed(requestedSize)) {
        
        res = createReserveLogfile(requestedSize);
        
        if (res == TRI_ERROR_NO_ERROR) {
          continue;
        }
        
        LOG_ERROR("unable to create new WAL reserve logfile: %s",
                  TRI_errno_string(res));
      }
    }
    catch (triagens::basics::Exception const& ex) {
      res = ex.code();
      LOG_ERROR("got unexpected error in allocatorThread: %s", TRI_errno_string(res));
    }
    catch (...) {
      LOG_ERROR("got unspecific error in allocatorThread");
    }

    // reset allocator status
    {
      CONDITION_LOCKER(guard, _allocatorResultCondition);
      _allocatorResult = res;
    }

    {
      CONDITION_LOCKER(guard, _condition);

      guard.wait(Interval);
    }
  }

  _stop = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
