#pragma once

#include <iostream>
#include <sstream>
// #include <ctpublic.h>
// #include "tdspp.hh"
#include "store.common/src/runtime_get_tuple.hpp"
#include "store.common/src/logger.hpp"
#include "store.storage.connection-pools/src/mssql.hpp"

using namespace store::models;
using namespace store::common;
using namespace store::common::tuples;

namespace MSSQLPool = store::storage::connection_pools::mssql;

namespace store::storage::mssql {
  
  class MsSqlBaseClient {
    // friend TDSPP;
  public:
    const string version = "0.1.15";
    
    MsSqlBaseClient(const string& server_, int port_, const string& database_, const string& user_, const string& password_, const string& poolKey, int poolSize = 10) :
      server(server_), port(port_), database(database_), user(user_), password(password_) {
      server_port = server + ":" + to_string(port);

      MSSQLPool::createPool(server, port, database, user, password, poolKey, poolSize);

      pool = ioc::ServiceProvider->GetInstanceWithKey<ConnectionPool<MSSQLConnection>>(poolKey);
    }

    MsSqlBaseClient(const DBContext& dbContext, const string& poolKey, int poolSize = 10) {
      MSSQLPool::createPoolFromDBContext(dbContext, poolKey, poolSize);

      pool = ioc::ServiceProvider->GetInstanceWithKey<ConnectionPool<MSSQLConnection>>(poolKey);
    }

    MsSqlBaseClient(const json& config_j, const string& environment, const string& poolKey, int poolSize = 10) {
      MSSQLPool::createPoolFromJson(config_j, environment, poolKey, poolSize);

      pool = ioc::ServiceProvider->GetInstanceWithKey<ConnectionPool<MSSQLConnection>>(poolKey);
    }

    shared_ptr<Query> runQuery(const string& sqlStmt) {
      std::shared_ptr<MSSQLConnection> conn=pool->borrow();

      try {
        auto db = conn->sql_connection;
        Query* q = db->sql(sqlStmt);
        q->execute();
        pool->unborrow(conn);
        return make_shared<Query>(move(*q));
      } catch (TDSPP::Exception& e) {
        logger->error(e.message.c_str());
      }

      if (pool)
        pool->unborrow(conn);

      return nullptr;
    }

    pair<int, string> execute(const string& sqlStmt) {
      std::shared_ptr<MSSQLConnection> conn=pool->borrow();

      try {
        auto db = conn->sql_connection;
        db->execute(sqlStmt);
      } catch (TDSPP::Exception& e) {
        logger->error(e.message.c_str());
        pool->unborrow(conn);
        return make_pair(0, e.message);
      }
      pool->unborrow(conn);
      return make_pair(1, "Succeeded");
    }

    /*
      @usage:
      client.insertOne("epic", "participant_hist", {"firstname", "lastname", "processed"}, "foo", "bar", 0);
    */
    template<typename... Params>
    pair<int, string> insertOne(const string& schema, const string& table_name, const std::initializer_list<std::string>& fields, Params... params) {
      tuple<Params...> values(params...);
      stringstream ss;

      ss << "insert into " << schema << "." << table_name << " (";

      auto it = fields.begin();
      while (it != fields.end() - 1) {
        ss << *it << ",";
        ++it;
      }          
      ss << *it;          
      ss << ") values \n(";
      
      int i = 0;
      while (i < fields.size() - 1) {
        tuples::apply(values, i, TupleFormatFunc{&ss});
        ss << ",";
        ++i;
      }          
      tuples::apply(values, fields.size() - 1, TupleFormatFunc{&ss});
      ss << ");";

      std::shared_ptr<MSSQLConnection> conn=pool->borrow();

      try {
        auto db = conn->sql_connection;
        db->execute(ss.str());
      } catch (TDSPP::Exception& e) {
        logger->error(e.message.c_str());
        pool->unborrow(conn);
        return make_pair(0, e.message);
      }

      pool->unborrow(conn);
      return make_pair(1, "Succeeded");
    }

    // Default logger.
    shared_ptr<ILogger> logger = make_shared<ILogger>();

  protected:
    shared_ptr<ConnectionPool<MSSQLConnection>> pool;

  private:
    string server;
    int port;
    string database;
    string user;
    string password;
    string server_port;
  };

}
