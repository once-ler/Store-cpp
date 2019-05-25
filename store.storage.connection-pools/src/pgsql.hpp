#pragma once

#include <iostream>
#include "json.hpp"
#include "PostgreSQLConnection.h"

#include "store.models/src/extensions.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace std;
using namespace com::eztier;
using namespace store::extensions;
namespace ioc = store::ioc;

using json = nlohmann::json;

namespace store::storage::connection_pools::pgsql {
  auto testPool = []() {
    cout << "Testing PostgreSQL connection pool" << endl;
    
    auto pool1 = ioc::ServiceProvider->GetInstance<ConnectionPool<PostgreSQLConnection>>();

    std::shared_ptr<PostgreSQLConnection> conn = pool1->borrow();
    
    auto &now = conn->sql_connection->execute("SELECT to_char(current_timestamp, 'YYYY-MM-DD HH:MI:SS.MS')");
    std::cout << now.as<std::string>(0) << std::endl;

    pool1->unborrow(conn);
  };

  auto createPool = [](const json& config_j, const string& environment, int poolSize = 10) {
    auto config_pt = make_shared<json>(config_j);

    int port = getPathValueFromJson<int>(config_pt, "postgres", environment, "port");

    string server = getPathValueFromJson<string>(config_pt, "postgres", environment, "server"),
      database = getPathValueFromJson<string>(config_pt, "postgres", environment, "database"),
      user = getPathValueFromJson<string>(config_pt, "postgres", environment, "user"),
      password = getPathValueFromJson<string>(config_pt, "postgres", environment, "password");

    cout << "Creating PostgreSQL connections..." << endl;
    std::shared_ptr<PostgreSQLConnectionFactory> connection_factory;
    std::shared_ptr<ConnectionPool<PostgreSQLConnection>> pool;
    bool poolCreated = false;

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
      cerr << "Cannot create PostgreSQL pool" << endl;
    }
    
    if (poolCreated) {
      ConnectionPoolStats stats=pool->get_stats();
      assert(stats.pool_size == poolSize);
      cout << "PostgreSQL connection pool count: " << stats.pool_size << endl;

      // Register pool
      ioc::ServiceProvider->RegisterInstance<ConnectionPool<PostgreSQLConnection>>(pool);

      testPool();
    }
    
  };
}
