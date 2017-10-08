#pragma once

#include <iostream>
#include <memory>
#include "json.hpp"
// #include "include/connection.hpp"
// #include "include/utility.hpp"
#include "base_client.hpp"

#include "postgres-connection.h"
#include "postgres-exceptions.h"

using namespace std;
// using namespace postgrespp;
using namespace db::postgres;

using namespace store::interfaces;
using namespace store::storage;

using json = nlohmann::json;

namespace store {
  namespace storage {
    namespace pgsql {

      template<typename T>
      class Client : public BaseClient<T> {
      private:
        // shared_ptr<Connection> connection;
        Connection cnx;
      public:
        using BaseClient<T>::BaseClient;

        Client(DBContext _dbContext) : BaseClient<T>(_dbContext) {
          cnx.connect("host=127.0.0.1 port=5432 dbname=pccrms connect_timeout=10");
          
        }
        
        template<typename U>
        U save(string version, U doc) {
          auto f = this->Save([this, &doc](string version) {

            auto &resp = cnx.execute(R"SQL(
              select id, name, ts, current from master.droid limit 10;
            )SQL");

            for (auto &row: resp) {
              std::cout << "- " << row.as<string>(0) << " " << row.as<string>(1)
                << ", " << row.as<timestamp_t>(2) << ", " << row.as<string>(3) << endl;
            }

            cnx.close();

            return doc;
          });

          return f(version);
        }

      };

    }
  }
}
