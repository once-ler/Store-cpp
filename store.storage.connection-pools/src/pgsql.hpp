#pragma once

#include <iostream>
#include "json.hpp"
#include "PostgreSQLConnection.h"

#include "store.models/src/extensions.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.models/src/models.hpp"

using namespace std;
using namespace com::eztier;
using namespace store::extensions;
using namespace store::models;
namespace ioc = store::ioc;

using json = nlohmann::json;

namespace store::storage::connection_pools::pgsql {
  auto testPool = [](std::string instanceKey) {
    #ifdef DEBUG
    cout << "Testing PostgreSQL connection pool with key " << instanceKey << endl;
    #endif

    auto pool1 = ioc::ServiceProvider->GetInstanceWithKey<ConnectionPool<PostgreSQLConnection>>(instanceKey);

    std::shared_ptr<PostgreSQLConnection> conn = nullptr;
    
    try {
      conn = pool1->borrow();
    
      auto &now = conn->sql_connection->execute("SELECT to_char(current_timestamp, 'YYYY-MM-DD HH:MI:SS.MS')");
    
      #ifdef DEBUG
      std::cout << now.as<std::string>(0) << std::endl;
      #endif
    } catch (...) {
      throw "ConnectionPool<PostgreSQLConnection> test failed.";
    }

    if (conn)
      pool1->unborrow(conn);
  };

  auto createPoolImpl = [](const string& server, int port, const string& database, const string& user, const string& password, const string& poolKey, int poolSize = 10) {
    bool poolCreated = false;

    poolCreated = ioc::ServiceProvider->InstanceWithKeyExist<ConnectionPool<PostgreSQLConnection>>(poolKey);
    if (poolCreated)
      return;

    #ifdef DEBUG
    cout << "Creating PostgreSQL connections..." << endl;
    #endif

    std::shared_ptr<PostgreSQLConnectionFactory> connection_factory;
    std::shared_ptr<ConnectionPool<PostgreSQLConnection>> pool;
    
    try {
      connection_factory = make_shared<PostgreSQLConnectionFactory>(
        server,
        port,
        database,
        user,
        password
      );
      pool = make_shared<ConnectionPool<PostgreSQLConnection>>(poolSize, connection_factory);
      poolCreated = true;
    } catch (...) {
      throw "Cannot create PostgreSQL pool";
    }
    
    if (poolCreated) {
      ConnectionPoolStats stats=pool->get_stats();
      assert(stats.pool_size == poolSize);
      
      #ifdef DEBUG
      cout << "PostgreSQL connection pool count: " << stats.pool_size << endl;
      #endif

      // Register pool
      ioc::ServiceProvider->RegisterInstanceWithKey<ConnectionPool<PostgreSQLConnection>>(poolKey, pool);

      testPool(poolKey);
    }
  };

  auto createPool = [](const string& server, int port, const string& database, const string& user, const string& password, const string& poolKey, int poolSize = 10) {
    createPoolImpl(server, port, database, user, password, poolKey, poolSize);
  };

  auto createPoolFromJson = [](const json& config_j, const string& environment, const string& poolKey, int poolSize = 10) {
    auto config_pt = make_shared<json>(config_j);

    int port = getPathValueFromJson<int>(config_pt, "postgres", environment, "port");

    string server = getPathValueFromJson<string>(config_pt, "postgres", environment, "server"),
      database = getPathValueFromJson<string>(config_pt, "postgres", environment, "database"),
      user = getPathValueFromJson<string>(config_pt, "postgres", environment, "user"),
      password = getPathValueFromJson<string>(config_pt, "postgres", environment, "password");
    
    createPoolImpl(server, port, database, user, password, poolKey, poolSize);
  };

  auto createPoolFromDBContext = [](const store::models::DBContext& dbContext, const string& poolKey, int poolSize = 10) {
    createPoolImpl(dbContext.server, dbContext.port, dbContext.database, dbContext.user, dbContext.password, poolKey, poolSize);
  };
}
