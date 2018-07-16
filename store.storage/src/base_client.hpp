#pragma once

#include <iostream>
#include <typeinfo>

#if __has_include("interfaces.hpp")
#include "interfaces.hpp"
#include "models.hpp"
#include "event.hpp"
#include "eventstream.hpp"
#include "eventstore.hpp"
#include "background_worker.hpp"
#else
#include "store.models/src/interfaces.hpp"
#include "store.models/src/models.hpp"
#include "store.events/src/event.hpp"
#include "store.events/src/eventstream.hpp"
#include "store.events/src/eventstore.hpp"
#include "store.common/src/background_worker.hpp"
#endif

using namespace std;
using namespace store::interfaces;
using namespace store::models;
using namespace store::events;

namespace store {
  namespace storage {
    template<typename T>
    class BaseClient : public IStore<T> {
    public:  
      DBContext dbContext;
      
      BaseClient() = default;
      ~BaseClient() = default;
      BaseClient(DBContext _dbContext) : dbContext(_dbContext) {}      
    protected:
      /* Example of a EventStore. */
      class BaseEventStore : public EventStore {
      public:
        BaseEventStore() = default;
        ~BaseEventStore() = default;
        explicit BaseEventStore(BaseClient<T>* session_) : session(session_) {}
        
        int Save() override {
          throw("Not implemented exception.");
          return -1;
        }
      protected:
        BaseClient<T>* session;
      };
    };
  }
}
