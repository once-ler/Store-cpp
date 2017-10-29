#pragma once

#include <iostream>
#include <typeinfo>
#include "interfaces.hpp"
#include "models.hpp"
#include "event.hpp"

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
      class BaseEventStore : public EventStore {
      public:
        explicit BaseEventStore(shared_ptr<BaseClient<T>> session_) : session(session_) {}
        /*
        int Save() override {
          cout << this->pending.size() << endl;
          return 0;
        }
        */
      private:
        shared_ptr<BaseClient<T>> session;
      };

      template<typename U, typename... Params>
      int64_t insertOne(const string& version, const std::initializer_list<std::string>& fields, Params... params) {
        throw("Not implemented exception.");
        return -1;
      }
    };
  }
}
