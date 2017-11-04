#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <thread>

using namespace std;

namespace store {
  namespace common {

    template<typename F>
    class BackgroundWorker {
      vector<unique_ptr<thread>> threads;
      F func;
    public:
      explicit BackgroundWorker(F func_) : func(func_) {};
      
      template<typename ...U>
      void start(U... args) {
        auto params = { args... };
        
        for (const auto& e : params) {
          threads.emplace_back(new thread(func, e));
        }
      }
    
      /*
      Calling start() is only necessary if calling program does not join on thread.
      */
      void join() {
        auto len = threads.size();
        if (len > 0) {
          threads.at(len - 1)->join();
        }
      }
    };
    
  }
}
