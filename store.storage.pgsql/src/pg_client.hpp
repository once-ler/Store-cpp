#pragma once

#include <iostream>
#include <memory>
#include <sstream>
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

      template<typename U>
      string serializeToJsonb(const U& o) noexcept {
        json j;
        try {
          j = o;
        } catch (...) {
          // No op.
        }
        return move(db::postgres::toJsonb(j.dump()));
      }

      template<typename T>
      class Client : public BaseClient<T> {
      private:
      public:
        using BaseClient<T>::BaseClient;

        Client(DBContext _dbContext) : BaseClient<T>(_dbContext) {
          connectionInfo = string_format("application_name=%s host=%s port=%d dbname=%s connect_timeout=%d user=%s password=%s", dbContext.applicationName.c_str(), dbContext.server.c_str(), dbContext.port, dbContext.database.c_str(), dbContext.connectTimeout, dbContext.user.c_str(), dbContext.password.c_str());
        }
        
        template<typename U>
        U save(string version, U doc) {
          auto func = this->Save([this, &doc](string version) {
            Connection cnx;
            try {
              json j = doc;
              
              string sql = string_format(R"SQL(
                insert into %s.%s (id, name,ts,current)
                values ('%s', '%s', now(), '%s')
                on conflict (id) do update set ts = now(), current = EXCLUDED.current, history = EXCLUDED.history
              )SQL", version.c_str(), resolve_type_to_string<U>().c_str(), j.value("id", "").c_str(), j.value("name", "").c_str(), j.dump().c_str());

              cout << sql << endl;

              cnx.connect(connectionInfo.c_str());

              cnx.execute(sql.c_str());

            } catch (ConnectionException e) {
              std::cerr << "Oops... Cannot connect...";
            } catch (ExecutionException e) {
              std::cerr << "Oops... " << e.what();
            } catch (exception e) {
              std::cerr << e.what();
            }

            return doc;
          });

          return func(version);
        }

        template<typename U, typename... Params>
        int64_t save(const string& version, const std::initializer_list<std::string>& fields, Params... params) {
          stringstream ss;

          ss << "insert into " << version << "." << resolve_type_to_string<U>() << "(";

          auto it = fields.begin();
          while (it != fields.end() - 1) {
            ss << *it << ",";
            ++it;
          }
          ss << *it;
          
          ss << ") values \n(";
          
          int i = 0;
          while (i < fields.size() - 1) {
            ss << "$" << i + 1 << ",";
            ++i;
          }
          ss << "$" << fields.size();

          ss << ");";
          cout << ss.str();
          return 0;
        }

        void save(const string& sql) {
          Connection cnx;
          try {
            cnx.connect(connectionInfo.c_str());
            cnx.execute(sql.c_str());
          } catch (ConnectionException e) {
            std::cerr << "Oops... Cannot connect...";
          } catch (ExecutionException e) {
            std::cerr << "Oops... " << e.what();
          } catch (exception e) {
            std::cerr << e.what();
          }
        }

        template<typename U>
        vector<U> list(string version = "master", int offset = 0, int limit = 10, string sortKey = "id", string sortDirection = "Asc") {
          
          auto func = this->List([this](string version, int offset, int limit, string sortKey, string sortDirection) {
            Connection cnx;
            vector<json> jsons;
            vector<U> pocos;

            try {
              auto sql = string_format("select current from %s.%s order by current->>'%s' %s offset %d limit %d",
                version.c_str(),
                resolve_type_to_string<U>().c_str(),
                sortKey.c_str(),
                sortDirection.c_str(),
                offset,
                limit
              );

              cout << "sql: " << sql << endl;

              cnx.connect(connectionInfo.c_str());
              auto& resp = cnx.execute(sql.c_str());

              for (auto &row : resp) {
                json j = json::parse(strip_soh(row.as<string>(0)));
                jsons.push_back(move(j));
              }

              for_each(jsons.begin(), jsons.end(), [&](const json& j) { U o = j; pocos.push_back(move(o)); });
            } catch (ConnectionException e) {
              std::cerr << "Oops... Cannot connect...";
            } catch (ExecutionException e) {
              std::cerr << "Oops... " << e.what();
            } catch (exception e) {
              std::cerr << e.what();
            }

            return pocos;
          });

          return func(version, offset, limit, sortKey, sortDirection);
        }

      protected:
        string connectionInfo;

      };

    }
  }
}
