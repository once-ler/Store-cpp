#pragma once

#include <iostream>
#include <typeinfo>
#include "store.models/src/interfaces.hpp"
#include "store.models/src/models.hpp"
#include "store.events/src/event.hpp"
#include "store.events/src/eventstream.hpp"
#include "store.events/src/eventstore.hpp"
#include "store.common/src/background_worker.hpp"

using namespace std;
using namespace store::interfaces;
using namespace store::models;
using namespace store::events;

using json = nlohmann::json;

namespace store {
  namespace storage {
    template<typename T>
    class BaseClient : public IStore<T> {
    public:  
      DBContext dbContext;
      
      BaseClient() = default;
      ~BaseClient() = default;
      BaseClient(DBContext _dbContext) : dbContext(_dbContext) {}

      virtual vector<shared_ptr<json>> runQueryJson(const string& sqlStmt) {}
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
