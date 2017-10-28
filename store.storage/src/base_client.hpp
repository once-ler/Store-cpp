#pragma once

#include <iostream>
#include <typeinfo>
#include "interfaces.hpp"
#include "models.hpp"
#include "eventstore.hpp"

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
      EventStore Events;

      BaseClient() = default;
      ~BaseClient() = default;
      BaseClient(DBContext _dbContext) : dbContext(_dbContext) {}
      
    protected:
    };
  }
}
