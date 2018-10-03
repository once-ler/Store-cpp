#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <algorithm>
#include <regex>
#include "json.hpp"
#include "postgres-connection.h"
#include "postgres-exceptions.h"
#include "store.storage/src/base_client.hpp"
#include "store.models/src/extensions.hpp"
#include "store.events/src/eventstore.hpp"
#include "store.common/src/group_by.hpp"
#include "store.common/src/logger.hpp"

using namespace std;
using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;
using namespace store::common;

// Don't use db::postgres b/c it defines time_t which will collide with std::time_t if used in other libs.
namespace Postgres = db::postgres;
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

          pair<int, string> Save() override {
            const auto& streams = groupBy(pending.begin(), pending.end(), [](IEvent& e) { return e.streamId; });

            for (const auto& o : streams) {
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

              Postgres::Connection cnx;
              try {
                cnx.connect(session->connectionInfo.c_str());
                // resp is [current_version, ...sequence num]
                // https://stackoverflow.com/questions/17947863/error-expected-primary-expression-before-templated-function-that-try-to-us
                const auto& resp = cnx.execute(stmt.c_str()).template asArray<int>(0);
              } catch (Postgres::ConnectionException e) {
                session->logger->error(e.what());
                return make_pair(0, e.what());
              } catch (Postgres::ExecutionException e) {
                session->logger->error(e.what());
                return make_pair(0, e.what());
              } catch (exception e) {
                session->logger->error(e.what());
                return make_pair(0, e.what());
              }
            }

            // Clear pending collection.
            this->Reset();

            return make_pair(1, "Succeeded");
          }

          vector<IEvent> Search(string type, int64_t fromSeqId, int limit = 10) override {
            Postgres::Connection cnx;
            vector<IEvent> events;

            try {
              cnx.connect(session->connectionInfo.c_str());

              auto sql = Extensions::string_format("select seq_id, id, stream_id, type, version, data, timestamp from %s.mt_events where seq_id >= %d limit %d",
                dbSchema.c_str(),
                fromSeqId,
                limit
              );

              auto& resp = cnx.execute(sql.c_str());

              for (auto &row : resp) {
                json data = json::parse(strip_soh(row.as<string>(5)));

                IEvent ev{
                  row.as<int64_t>(0),
                  row.as<Primitive::uuid>(1),
                  row.as<Primitive::uuid>(2),
                  row.as<string>(3),
                  row.as<int64_t>(4),
                  data,
                  row.as<int64_t>(6)
                };

                events.push_back(move(ev));
              }
            } catch (Postgres::ConnectionException e) {
              session->logger->error(e.what());
            } catch (Postgres::ExecutionException e) {
              session->logger->error(e.what());
            } catch (exception e) {
              session->logger->error(e.what());
            }

            return events;
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
        
        // Pass pgsql::Client<A> to friend PgEventStore; event store will share same connection.
        PgEventStore events{ this };

        // Default logger.
        shared_ptr<ILogger> logger = make_shared<ILogger>();

        string save(const string& sql) {
          string retval = "Succeeded";

          Postgres::Connection cnx;
          try {
            cnx.connect(connectionInfo.c_str());
            cnx.execute(sql.c_str());
          } catch (Postgres::ConnectionException e) {
            retval = e.what();
            logger->error(e.what());
          } catch (Postgres::ExecutionException e) {
            retval = e.what();
            logger->error(e.what());
          } catch (exception e) {
            retval = e.what();
            logger->error(e.what());
          }
          
          return move(retval);
        }

        template<typename A>
        string createStore(const string& tableSchema){
          const auto tableName = resolve_type_to_string<A>();
          
          string createTable = Extensions::string_format(R"SQL(
            create table if not exists "%s"."%s" (
              id varchar(120) primary key,
              name character varying(500),
              ts timestamp default current_timestamp,
              type varchar(250),
              related varchar(120),
              current jsonb,
              history jsonb
            );                
          )SQL", tableSchema.c_str(), tableName.c_str()); 

          vector<string> indxs{"name", "ts", "type", "related", "current", "history"};
          string createIndexes = std::accumulate(indxs.begin(), indxs.end(),
            string{}, 
            [&tableSchema, &tableName](auto m, const auto& name){
              string ret = m + Extensions::string_format("create index if not exists %s_%s_idx on %s.%s ",
                tableName.c_str(), name.c_str(), tableSchema.c_str(), tableName.c_str());
              
              if (regex_match(name, regex("\\bcurrent\\b|\\bhistory\\b")))
                ret.append("using gin(" + name + " jsonb_ops)");
              else
                ret.append("using btree(" + name + ")");
              
              ret.append(";");
              return ret;
            }
          );

          string retval = this->save(createTable);
          retval.append(";");
          retval.append(this->save(createIndexes));

          return move(retval);
        }

        template<typename U>
        U save(string version, U doc) {
          auto func = this->Save([this, &doc](string version) {
            const auto tableName = resolve_type_to_string<U>();

            json j = doc;              

            string sql = Extensions::string_format(R"SQL(
              insert into %s.%s (id, name, ts, type, related, current)
              values ('%s', '%s', now(), '%s', '%s', '%s')
              on conflict (id) do update set ts = now(), current = EXCLUDED.current, history = EXCLUDED.history
            )SQL", version.c_str(), tableName.c_str(), j.value("id", "").c_str(), j.value("name", "").c_str(), j.value("type", "").c_str(), j.value("related", "").c_str(), j.dump().c_str());

            this->save(sql);

            return doc;
          });

          return func(version);
        }

        template<typename U, typename... Params>
        string insertOne(const string& version, const std::initializer_list<std::string>& fields, Params... params) {
          const auto tableName = resolve_type_to_string<U>();

          stringstream ss;

          ss << "insert into " << version << "." << tableName.c_str() << " (";

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
          
          const auto retval = this->save(ss.str());
          return move(retval);
        }

        string createStreamId(const std::map<string, string>& streamIds, const string& dbSchema) {
          string retval;
          for(auto& e : streamIds) {
            string sql = store::extensions::string_format(R"SQL(
              insert into %s.%s (id, type, version)
              values ('%s', '%s', 0)
              on conflict (id) do nothing
            )SQL", dbSchema.c_str(), "mt_streams", e.second.c_str(), e.first.c_str());
      
            retval = this->save(sql);
            retval.append(";");
          }

          return move(retval);
        }

        template<typename U>
        vector<U> list(string version = "master", int offset = 0, int limit = 10, string sortKey = "id", string sortDirection = "Asc") {
          
          auto func = this->List([this](string version, int offset, int limit, string sortKey, string sortDirection) {
            Postgres::Connection cnx;
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

              cnx.connect(connectionInfo.c_str());
              auto& resp = cnx.execute(sql.c_str());

              for (auto &row : resp) {
                json j = json::parse(strip_soh(row.template as<string>(0)));
                jsons.push_back(move(j));
              }

              for_each(jsons.begin(), jsons.end(), [&](const json& j) { U o = j; pocos.push_back(move(o)); });
            } catch (Postgres::ConnectionException e) {
              logger->error(e.what());
            } catch (Postgres::ExecutionException e) {
              logger->error(e.what());
            } catch (exception e) {
              logger->error(e.what());
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
