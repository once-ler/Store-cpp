#pragma once

#include <iostream>
#include "json.hpp"
#include "MSSQLDbLibConnection.h"

#include "store.models/src/extensions.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.models/src/models.hpp"

using namespace std;
using namespace com::eztier;
using namespace store::extensions;
using namespace store::models;
namespace ioc = store::ioc;

using json = nlohmann::json;

namespace store::storage::connection_pools::mssql::dblib {
  auto testPool = [](std::string instanceKey) {
    #ifdef DEBUG
    cout << "Testing MSSQL dblib connection pool with key " << instanceKey << endl;
    #endif

    auto pool1 = ioc::ServiceProvider->GetInstanceWithKey<ConnectionPool<MSSQLDbLibConnection>>(instanceKey);

    std::shared_ptr<MSSQLDbLibConnection> conn = nullptr;
    
    try {
      conn = pool1->borrow();
      
      int rc = 0;
      vector<string> fieldNames;
      vector<vector<string>> fieldValues;
      
      auto db = conn->sql_connection;

      db->sql("select convert(varchar(23), current_timestamp, 126) right_now");
      rc = db->execute();

      if (rc) {
        throw "ConnectionPool<MSSQLDbLibConnection> test failed.";
      }

      #ifdef DEBUG
      fieldNames = std::move(db->fieldNames);
      fieldValues = std::move(db->fieldValues);

      int idx = 0, cnt = fieldNames.size() - 1;
      for (auto& fld : fieldNames) {
        cout << fld << (idx < cnt ? "\t" : "");
        idx++;
      }
      cout << endl;
      idx = 0;

      for (auto& row : fieldValues) {
        for (const auto& col: row) {
          cout << col << (idx < cnt ? "\t" : "");
        }
        cout << endl;
      }
      cout << endl;
      #endif
    } catch (...) {
      throw "ConnectionPool<MSSQLDbLibConnection> test failed.";
    }
    
    if (conn)
      pool1->unborrow(conn);
  };

  auto createPoolImpl = [](const string& server, int port, const string& database, const string& user, const string& password, const string& poolKey, int poolSize = 10) {
    bool poolCreated = false;

    poolCreated = ioc::ServiceProvider->InstanceWithKeyExist<ConnectionPool<MSSQLDbLibConnection>>(poolKey);
    if (poolCreated)
      return;

    #ifdef DEBUG
    cout << "Creating MSSQL dblib connections for " << poolKey << "..." << endl;
    #endif

    std::shared_ptr<MSSQLDbLibConnectionFactory> connection_factory;
    std::shared_ptr<ConnectionPool<MSSQLDbLibConnection>> pool;
    
    try {
      connection_factory = make_shared<MSSQLDbLibConnectionFactory>(
        server,
        port,
        database,
        user,
        password
      );
      pool = make_shared<ConnectionPool<MSSQLDbLibConnection>>(poolSize, connection_factory);
      poolCreated = true;
    } catch (...) {
      throw "Cannot create MSSQL dblib pool";
    }
    
    if (poolCreated) {
      ConnectionPoolStats stats=pool->get_stats();
      assert(stats.pool_size == poolSize);
      
      #ifdef DEBUG
      cout << "MSSQL dblib connection pool count: " << stats.pool_size << endl;
      #endif

      // Register pool
      ioc::ServiceProvider->RegisterInstanceWithKey<ConnectionPool<MSSQLDbLibConnection>>(poolKey, pool);

      testPool(poolKey);
    }
  };

  auto createPool = [](const string& server, int port, const string& database, const string& user, const string& password, const string& poolKey, int poolSize = 10) {
    createPoolImpl(server, port, database, user, password, poolKey, poolSize);
  };

  auto createPoolFromJson = [](const json& config_j, const string& environment, const string& poolKey, int poolSize = 10) {
    auto config_pt = make_shared<json>(config_j);

    int port = getPathValueFromJson<int>(config_pt, "mssql", environment, "port");

    string server = getPathValueFromJson<string>(config_pt, "mssql", environment, "server"),
      database = getPathValueFromJson<string>(config_pt, "mssql", environment, "database"),
      user = getPathValueFromJson<string>(config_pt, "mssql", environment, "user"),
      password = getPathValueFromJson<string>(config_pt, "mssql", environment, "password");

    createPoolImpl(server, port, database, user, password, poolKey, poolSize); 
  };

  auto createPoolFromDBContext = [](const store::models::DBContext& dbContext, const string& poolKey, int poolSize = 10) {
    createPoolImpl(dbContext.server, dbContext.port, dbContext.database, dbContext.user, dbContext.password, poolKey, poolSize);
  };
  
}
