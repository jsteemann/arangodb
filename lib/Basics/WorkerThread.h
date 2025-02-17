////////////////////////////////////////////////////////////////////////////////
/// @brief thread pool worker thread
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
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_WORKER_THREAD_H
#define ARANGODB_BASICS_WORKER_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/Thread.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                      WorkerThread
// -----------------------------------------------------------------------------

    class WorkerThread : public triagens::basics::Thread {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        WorkerThread (WorkerThread const&) = delete;
        WorkerThread operator= (WorkerThread const&) = delete;

        WorkerThread (ThreadPool* pool)
          : Thread(pool->name()),
            _pool(pool),
            _status(0) {
        }

        ~WorkerThread () {
        }


////////////////////////////////////////////////////////////////////////////////
/// @brief stops the worker thread
////////////////////////////////////////////////////////////////////////////////

        void waitForDone () {
          int expected = 0;
          _status.compare_exchange_strong(expected, 1, std::memory_order_relaxed);

          while (_status != 2) {
            usleep(5000);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

      protected:

        void run () {
          while (_status == 0) {
            std::function<void()> task;

            if (! _pool->dequeue(task)) {
              break;
            }
            
            task();
          }

          _status = 2;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        ThreadPool* _pool;

        std::atomic<int> _status;
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
