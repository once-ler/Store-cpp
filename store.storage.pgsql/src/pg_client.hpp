#pragma once

#include <iostream>
#include <memory>
#include "json.hpp"
// #include "include/connection.hpp"
// #include "include/utility.hpp"
#include "base_client.hpp"
#include "extensions.hpp"

#include "postgres-connection.h"
#include "postgres-exceptions.h"

using namespace std;
// using namespace postgrespp;
using namespace db::postgres;

using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;

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
          cnx.connect("host=127.0.0.1 port=5432 dbname=pccrms connect_timeout=10 user=editor password=editor");
          
        }
        
        template<typename U>
        U save(string version, U doc) {
          auto f = this->Save([this, &doc](string version) {

            string sql = string_format(R"SQL(
              insert into master.droid (id, name,ts,current)
              values ('%s', '%s', now(), '%s')
              on conflict (id) do update set ts = now(), current = EXCLUDED.current, history = EXCLUDED.history
            )SQL", "2", "c3po", R"({"id": "2", "name": "c3po", "ts": "2017-10-09T14:44:59.684Z"})");

            cout << sql << endl;

            cnx.execute(sql.c_str());

            auto &resp = cnx.execute(R"SQL(
              select id, name, ts, current from master.droid limit 10;
            )SQL");

            for (auto &row: resp) {
              
              std::cout << "- " << row.as<string>(0) << " " << row.as<string>(1)
                << ", " << row.as<timestamp_t>(2) << ", " << strip_controls(row.as<string>(3)) << endl;

              json j = json::parse(strip_controls(row.as<string>(3)));
              cout << j.dump(2) << endl;
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
