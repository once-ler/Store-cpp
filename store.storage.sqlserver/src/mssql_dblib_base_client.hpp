#pragma once

#include <iostream>
#include <sstream>
#include "store.common/src/logger.hpp"
#include "store.storage.connection-pools/src/mssql_dblib.hpp"

using namespace store::common;

namespace MSSQLDbLibPool = store::storage::connection_pools::mssql::dblib;

namespace store::storage::mssql {
  class MsSqlDbLibBaseClient {
    
  public:
    MsSqlDbLibBaseClient(const string& server_, int port_, const string& database_, const string& user_, const string& password_, int poolSize = 10) :
      server(server_), port(port_), database(database_), user(user_), password(password_) {
      server_port = server + ":" + to_string(port);

      MSSQLDbLibPool::createPool(server, port, database, user, password, poolSize);

      pool = ioc::ServiceProvider->GetInstance<ConnectionPool<MSSQLDbLibConnection>>();
    }

    MsSqlDbLibBaseClient(const DBContext& dbContext, int poolSize = 10) {
      MSSQLDbLibPool::createPoolFromDBContext(dbContext, poolSize);

      pool = ioc::ServiceProvider->GetInstance<ConnectionPool<MSSQLDbLibConnection>>();
    }

    MsSqlDbLibBaseClient(const json& config_j, const string& environment, int poolSize = 10) {
      MSSQLDbLibPool::createPoolFromJson(config_j, environment, poolSize);

      pool = ioc::ServiceProvider->GetInstance<ConnectionPool<MSSQLDbLibConnection>>();
    }

    int quick(const istream& input, vector<string>& fieldNames, vector<vector<string>>& fieldValues) {
      std::shared_ptr<MSSQLDbLibConnection> conn = pool->borrow();

      ostringstream oss;
      oss << input.rdbuf();

      auto db = conn->sql_connection;

      db->sql(oss.str());

      int rc = db->execute();

      if (rc) {
        return rc;
      }

      fieldNames = std::move(db->fieldNames);
      fieldValues = std::move(db->fieldValues);

      pool->unborrow(conn);
    }

    // Default logger.
    shared_ptr<ILogger> logger = make_shared<ILogger>();

  protected:
    shared_ptr<ConnectionPool<MSSQLDbLibConnection>> pool;

  private:
    string server;
    int port;
    string database;
    string user;
    string password;
    string server_port;

  };
}
