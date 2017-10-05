#pragma once

#include <iostream>
#include <typeinfo>
#include "interfaces.hpp"
#include "models.hpp"

using namespace std;
using namespace store::interfaces;
using namespace store::models;

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
      void test() {
        
      }
    };
  }
}
