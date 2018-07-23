#pragma once

#include "store.storage.sqlserver/src/mssql_base_client.hpp"
#include "store.storage/src/base_client.hpp"

using namespace store::common;
using namespace store::storage::mssql;

namespace store::storage::mssql {
  
  template<typename T>
  class MsSqlClient : public BaseClient<T>, public MsSqlBaseClient {
    public:
    const string version = "0.1.9";
    MsSqlClient(DBContext _dbContext) :
      BaseClient<T>(_dbContext),
      MsSqlBaseClient(_dbContext.server, _dbContext.port, _dbContext.database, _dbContext.user, _dbContext.password) {}
  
  };
}