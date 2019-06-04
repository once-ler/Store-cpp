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
namespace ioc = store::ioc;

using json = nlohmann::json;

namespace store::storage::connection_pools::mssql::dblib {
  auto testPool = []() {
    cout << "Testing MSSQL dblib connection pool" << endl;
    
    auto pool1 = ioc::ServiceProvider->GetInstance<ConnectionPool<MSSQLDbLibConnection>>();

    std::shared_ptr<MSSQLDbLibConnection> conn = pool1->borrow();
    
    int rc = 0;
    vector<string> fieldNames;
    vector<vector<string>> fieldValues;
    
    auto db = conn->sql_connection;

    db->sql("select convert(varchar(23), current_timestamp, 126) right_now");
    rc = db->execute();

    if (rc) {
      cerr << "Error occurred." << endl;
      return;
    }

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

    pool1->unborrow(conn);
  };

  auto createPoolImpl = [](const string& server, int port, const string& database, const string& user, const string& password, int poolSize = 10) {
    bool poolCreated = false;

    poolCreated = ioc::ServiceProvider->InstanceExist<ConnectionPool<MSSQLDbLibConnection>>();
    if (poolCreated)
      return;

    cout << "Creating MSSQL dblib connections..." << endl;
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
      cerr << "Cannot create MSSQL dblib pool" << endl;
    }
    
    if (poolCreated) {
      ConnectionPoolStats stats=pool->get_stats();
      assert(stats.pool_size == poolSize);
      cout << "MSSQL dblib connection pool count: " << stats.pool_size << endl;

      // Register pool
      ioc::ServiceProvider->RegisterInstance<ConnectionPool<MSSQLDbLibConnection>>(pool);

      testPool();
    }
  };

  auto createPool = [](const string& server, int port, const string& database, const string& user, const string& password, int poolSize = 10) {
    createPoolImpl(server, port, database, user, password, poolSize);
  };

  auto createPoolFromJson = [](const json& config_j, const string& environment, int poolSize = 10) {
    auto config_pt = make_shared<json>(config_j);

    int port = getPathValueFromJson<int>(config_pt, "mssql", environment, "port");

    string server = getPathValueFromJson<string>(config_pt, "mssql", environment, "server"),
      database = getPathValueFromJson<string>(config_pt, "mssql", environment, "database"),
      user = getPathValueFromJson<string>(config_pt, "mssql", environment, "user"),
      password = getPathValueFromJson<string>(config_pt, "mssql", environment, "password");

    createPoolImpl(server, port, database, user, password, poolSize); 
  };

  auto createPoolFromDBContext = [](const store::models::DBContext& dbContext, int poolSize = 10) {
    createPoolImpl(dbContext.server, dbContext.port, dbContext.database, dbContext.user, dbContext.password, poolSize);
  };
  
}
