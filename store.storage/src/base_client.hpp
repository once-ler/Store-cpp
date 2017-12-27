#pragma once

#include <iostream>
#include <typeinfo>
#include "interfaces.hpp"
#include "models.hpp"
#include "event.hpp"
#include "eventstream.hpp"
#include "eventstore.hpp"
#include "background_worker.hpp"

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
