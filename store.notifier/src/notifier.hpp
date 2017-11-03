#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <memory>

using namespace std;

namespace store {
  namespace notifier {
    struct IHandler {
      virtual void operator()(const string& channel) {
        throw ("Not implemented exception.");
      }
    };

    template<typename ...U>
    class IFactory {
      vector<unique_ptr<thread>> threads;
    public:
      explicit IFactory(const U&... channels) {
        vector<U> params = {channels};

        for (cont auto& e : channels) {
          threads.emplace_back(new thread(IHandler(), e));
        }
      }

      /*
        Calling start() is only necessary if calling program does not join on thread.
      */
      void start() {
        auto len = threads.size();
        if (len > 0) {
          threads.at(len - 1)->join();
        }
      }
    };

  }
}
