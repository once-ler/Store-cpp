#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <algorithm>
#include "json.hpp"
#include "postgres-connection.h"
#include "postgres-exceptions.h"
#include "store.storage/src/base_client.hpp"
#include "store.models/src/extensions.hpp"
#include "store.events/src/eventstore.hpp"
#include "store.common/src/group_by.hpp"

using namespace std;
using namespace db::postgres;

using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;

namespace Extensions = store::extensions;

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
      public:
        using BaseClient<T>::BaseClient;
        
        class PgEventStore : public EventStore {
        public:
          explicit PgEventStore(Client<T>* session_) : session(session_) {}

          int Save() override {
            // cout << this->pending.size() << endl;
            // cout << this->session->dbContext.server << endl;

            const auto& streams = groupBy(pending.begin(), pending.end(), [](IEvent& e) { return e.streamId; });

            for (const auto& o : streams) {
              // cout << o.second.size() << endl;

              auto streamId = o.first;
              auto stream = o.second;
              auto eventIds = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(generate_uuid()); });
              auto eventTypes = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(e.type); });
              auto bodies = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(e.data.dump()); });
              
              auto stmt = Extensions::string_format("select %s.mt_append_event(%s, %s, array[%s]::uuid[], array[%s]::varchar[], array[%s]::jsonb[])",
                dbSchema.c_str(),
                wrapString(streamId).c_str(),
                eventTypes.at(0).c_str(),
                join(eventIds.begin(), eventIds.end(), string(",")).c_str(),
                join(eventTypes.begin(), eventTypes.end(), string(",")).c_str(),
                join(bodies.begin(), bodies.end(), string(",")).c_str()
              );

              Connection cnx;
              try {
                cnx.connect(session->connectionInfo.c_str());
                // resp is [current_version, ...sequence num]
                // https://stackoverflow.com/questions/17947863/error-expected-primary-expression-before-templated-function-that-try-to-us
                const auto& resp = cnx.execute(stmt.c_str()).template asArray<int>(0);
              } catch (ConnectionException e) {
                std::cerr << e.what() << std::endl;
              } catch (ExecutionException e) {
                std::cerr << e.what() << std::endl;
              } catch (exception e) {
                std::cerr << e.what() << std::endl;
              }
            }

            auto eventIds = mapEvents<string>(pending, [](const IEvent& e) { return generate_uuid(); });
            auto eventTypes = mapEvents<string>(pending, [](const IEvent& e) { return e.type; });
            auto bodies = mapEvents<string>(pending, [](const IEvent& e) { return e.data.dump(); });

            return 0;
          }
        private:
          Client<T>* session;

          template<typename U, typename F>
          vector<U> mapEvents(vector<IEvent> events, F func) {
            vector<U> results;
            transform(events.begin(), events.end(), back_inserter(results), func);
            return results;
          }
        };

        Client(DBContext _dbContext) : BaseClient<T>(_dbContext) {
          connectionInfo = Extensions::string_format("application_name=%s host=%s port=%d dbname=%s connect_timeout=%d user=%s password=%s", 
            this->dbContext.applicationName.c_str(), 
            this->dbContext.server.c_str(), 
            this->dbContext.port, 
            this->dbContext.database.c_str(), 
            this->dbContext.connectTimeout, 
            this->dbContext.user.c_str(), 
            this->dbContext.password.c_str());
        }
        
        PgEventStore events{ this };

        template<typename U>
        U save(string version, U doc) {
          auto func = this->Save([this, &doc](string version) {
            Connection cnx;
            try {
              json j = doc;
              
              string createTable = Extensions::string_format(R"SQL(
                create table %s.%s (
                  id varchar(120) primary key,
                  name character varying(500),
                  ts timestamp, default current_timestamp,
                  type varchar(250),
                  related varchar(120),
                  current jsonb,
                  history jsonb
                )
              )SQL", version.c_str(), resolve_type_to_string<U>().c_str()); 

              string sql = Extensions::string_format(R"SQL(
                insert into %s.%s (id, name, ts, type, related, current)
                values ('%s', '%s', now(), '%s', '%s', %s')
                on conflict (id) do update set ts = now(), current = EXCLUDED.current, history = EXCLUDED.history
              )SQL", version.c_str(), resolve_type_to_string<U>().c_str(), j.value("id", "").c_str(), j.value("name", "").c_str(), j.value("type", "").c_str(), j.value("related", "").c_str(), j.dump().c_str());

              cnx.connect(connectionInfo.c_str());

              cnx.execute(sql.c_str());

            } catch (ConnectionException e) {
              std::cerr << e.what() << std::endl;
            } catch (ExecutionException e) {
              std::cerr << e.what() << std::endl;
            } catch (exception e) {
              std::cerr << e.what() << std::endl;
            }

            return doc;
          });

          return func(version);
        }

        template<typename U, typename... Params>
        int64_t insertOne(const string& version, const std::initializer_list<std::string>& fields, Params... params) {
          stringstream ss;

          ss << "insert into " << version << "." << resolve_type_to_string<U>().c_str() << " (";

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
          
          Connection cnx;
          try {
            cnx.connect(connectionInfo.c_str());
            cnx.execute(ss.str().c_str(), params...);
          } catch (ConnectionException e) {
            std::cerr << "Oops... Cannot connect...";
          } catch (ExecutionException e) {
            std::cerr << "Oops... " << e.what();
          } catch (exception e) {
            std::cerr << e.what();
          }

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
              auto sql = Extensions::string_format("select current from %s.%s order by current->>'%s' %s offset %d limit %d",
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
                json j = json::parse(strip_soh(row.template as<string>(0)));
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

      private:

      };     

    }
  }
}
