#pragma once

#include <iostream>
#include "json.hpp"
#include "MSSQLConnection.h"

#include "store.models/src/extensions.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.models/src/models.hpp"

using namespace std;
using namespace com::eztier;
using namespace store::extensions;
using namespace store::models;
namespace ioc = store::ioc;

using json = nlohmann::json;

namespace store::storage::connection_pools::mssql {
  auto testPool = [](std::string instanceKey) {
    #ifdef DEBUG
    cout << "Testing MSSQL connection pool with key " << instanceKey << endl;
    #endif

    auto pool1 = ioc::ServiceProvider->GetInstanceWithKey<ConnectionPool<MSSQLConnection>>(instanceKey);

    std::shared_ptr<MSSQLConnection> conn = nullptr;
    
    try {
      conn = pool1->borrow();
      
      Query *q = conn->sql_connection->sql("select convert(varchar(23), current_timestamp, 126) right_now");
      q->execute();

      #ifdef DEBUG
      while (!q->eof()) {
        for (int i=0; i < q->fieldcount; i++) {
          auto fd = q->fields(i);
          cout << fd->to_str() << endl;
        }
        q->next();
      }
      #endif
    } catch (...) {
      throw "ConnectionPool<MSSQLConnection> test failed.";
    }

    if (conn)
      pool1->unborrow(conn);
  };

  auto createPoolImpl = [](const string& server, int port, const string& database, const string& user, const string& password, const string& poolKey, int poolSize = 10) {
    bool poolCreated = false;

    poolCreated = ioc::ServiceProvider->InstanceWithKeyExist<ConnectionPool<MSSQLConnection>>(poolKey);
    if (poolCreated)
      return;

    #ifdef DEBUG
    cout << "Creating MSSQL connections " << poolKey << "..." << endl;
    #endif

    std::shared_ptr<MSSQLConnectionFactory> connection_factory;
    std::shared_ptr<ConnectionPool<MSSQLConnection>> pool;
    
    try {
      connection_factory = make_shared<MSSQLConnectionFactory>(
        server,
        port,
        database,
        user,
        password
      );
      pool = make_shared<ConnectionPool<MSSQLConnection>>(poolSize, connection_factory);
      poolCreated = true;
    } catch (...) {
      throw "Cannot create MSSQL pool";
    }
    
    if (poolCreated) {
      ConnectionPoolStats stats=pool->get_stats();
      assert(stats.pool_size == poolSize);
      
      #ifdef DEBUG
      cout << "MSSQL connection pool count: " << stats.pool_size << endl;
      #endif

      // Register pool
      ioc::ServiceProvider->RegisterInstanceWithKey<ConnectionPool<MSSQLConnection>>(poolKey, pool);

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
