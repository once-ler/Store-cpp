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
              auto eventIds = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(generate_uuid(), "$Q$"); });
              auto eventTypes = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(e.type, "$Q$"); });
              auto bodies = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(e.data.dump(), "$Q$"); });
              
              auto stmt = Extensions::string_format("select %s.mt_append_event(%s, %s, array[%s]::uuid[], array[%s]::varchar[], array[%s]::jsonb[])",
                dbSchema.c_str(),
                wrapString(streamId, "$Q$").c_str(),
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

          int64_t LastExecution(const string& type, const string& subscriber, int64_t newSeqId = -1) override {
            Postgres::Connection cnx;
            int64_t seqId = 0;

            try {
              cnx.connect(session->connectionInfo.c_str());

              string createTable = Extensions::string_format(R"SQL(
                create table if not exists "%s"."mt_last_execution" (
                  stream uuid not null REFERENCES %s.mt_streams ON DELETE CASCADE,
                  subscriber varchar(250) not null,
                  seq_id bigint,
                  primary key(stream, subscriber)
                );                
              )SQL", dbSchema.c_str(), dbSchema.c_str());

              cnx.execute(createTable.c_str());

              auto sql = Extensions::string_format("select seq_id from %s.mt_last_execution where stream = '%s' and subscriber = '%s'",
                dbSchema.c_str(),
                generate_uuid_v3(type.c_str()).c_str(),
                subscriber.c_str()
              );

              auto& resp = cnx.execute(sql.c_str());

              auto it = resp.begin();

              if ((*it).num() > 0) {
                seqId = (*it).as<int64_t>(0);
              } else {
                string insertDefault = Extensions::string_format(R"SQL(
                  insert into "%s"."mt_last_execution" (stream, subscriber, seq_id) values ('%s', '%s', -1) on conflict do nothing;
                )SQL", dbSchema.c_str(), generate_uuid_v3(type.c_str()).c_str(), subscriber.c_str());

                cnx.execute(insertDefault.c_str());
              }

              if (newSeqId > -1) {
                string setNextSeqId = Extensions::string_format(R"SQL(
                  update "%s"."mt_last_execution" set seq_id = %lld where stream = '%s' and subscriber = '%s';
                )SQL", dbSchema.c_str(), (long long)newSeqId, generate_uuid_v3(type.c_str()).c_str(), subscriber.c_str());

                cnx.execute(setNextSeqId.c_str());
                seqId = newSeqId;
              }
              
            } catch (Postgres::ConnectionException e) {
              session->logger->error(e.what());
            } catch (Postgres::ExecutionException e) {
              session->logger->error(e.what());
            } catch (exception e) {
              session->logger->error(e.what());
            }

            return seqId;
          }

          vector<IEvent> Search(const string& type, int64_t fromSeqId, int limit = 10, bool exact = true, vector<string> mustExistKeys = {}) override {
            Postgres::Connection cnx;
            vector<IEvent> events;

            string matchExact = exact ? "=" : "~*";

            // data field must contain the following keys.            
            vector<string> mustExistKeysQuoted;
            std::transform(mustExistKeys.begin(), mustExistKeys.end(), std::back_inserter(mustExistKeysQuoted), [](const string& s) { return wrapString(s); });
            string mustExistKeysQuotedString = join(mustExistKeysQuoted.begin(), mustExistKeysQuoted.end(), string(","));
            string matchMustExistKeys = mustExistKeysQuotedString.size() > 0 ? string_format(" and data ?& array[%s]", mustExistKeysQuotedString.c_str()) : "";

            try {
              cnx.connect(session->connectionInfo.c_str());

              auto sql = Extensions::string_format("select seq_id, id, stream_id, type, version, data, timestamp from %s.mt_events where type %s '%s' and seq_id > %lld %s limit %d",
                dbSchema.c_str(),
                matchExact.c_str(),
                type.c_str(),
                (long long)fromSeqId,
                matchMustExistKeys.c_str(),
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
                  row.as<db::postgres::timestamptz_t>(6)
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

        /*
          By default, a tablespace with the name of the schema is expected to exist.
          If not, pass false to useSchemaTablespace.
        */
        template<typename A>
        string createStore(const string& tableSchema, bool useSchemaTablespace = true){
          const auto tableName = resolve_type_to_string<A>();
          string tablespaceName = useSchemaTablespace ? tableSchema : "pg_default";
          string indexTablespace = useSchemaTablespace ? tableSchema + "_idx" : "pg_default";
          
          string createTable = Extensions::string_format(R"SQL(
            create table if not exists "%s"."%s" (
              id varchar(120),
              name character varying(500),
              ts timestamp with time zone default current_timestamp,
              type varchar(250),
              related varchar(120),
              current jsonb,
              history jsonb,
              primary key(id, type)
            ) tablespace %s;                
          )SQL",
            tableSchema.c_str(),
            tableName.c_str(),
            tablespaceName.c_str()  
          ); 

          vector<string> indxs{"name", "ts", "type", "related", "current", "history"};
          string createIndexes = std::accumulate(indxs.begin(), indxs.end(),
            string{}, 
            [&tableSchema, &tableName, &indexTablespace](auto m, const auto& name){
              string ret = m + Extensions::string_format("create index if not exists %s_%s_idx on %s.%s ",
                tableName.c_str(), name.c_str(), tableSchema.c_str(), tableName.c_str());
              
              if (regex_match(name, regex("\\bcurrent\\b|\\bhistory\\b")))
                ret.append("using gin(" + name + " jsonb_ops) tablespace " + indexTablespace);
              else
                ret.append("using btree(" + name + ") tablespace " + indexTablespace);
              
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
        string save(string version, U doc) {
          auto func = this->Save([this, &doc](string version) {
            const auto tableName = resolve_type_to_string<U>();

            json j = doc;              

            string sql = Extensions::string_format(R"SQL(
              insert into %s.%s (id, name, ts, type, related, current)
              values (%s, %s, now(), %s, %s, %s)
              on conflict (id, type) do update set ts = now(), current = EXCLUDED.current, history = EXCLUDED.history
            )SQL", version.c_str(), tableName.c_str(), 
              wrapString(j.value("id", ""), "$Q$").c_str(), 
              wrapString(j.value("name", ""), "$Q$").c_str(), 
              wrapString(j.value("type", ""), "$Q$").c_str(),
              wrapString(j.value("related", ""), "$Q$").c_str(),
              wrapString(j.dump(), "$Q$").c_str()
            );

            auto res = this->save(sql);

            return res;
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
        vector<U> list(
          string version = "master", 
          int offset = 0, 
          int limit = 10, 
          string sortKey = "id", 
          string sortDirection = "Asc",
          string query = "select current from %s.%s order by current->>'%s' %s offset %d limit %d"
        ) {
          
          auto func = this->List([this, &query](string version, int offset, int limit, string sortKey, string sortDirection) {
            Postgres::Connection cnx;
            vector<json> jsons;
            vector<U> pocos;

            try {
              auto sql = Extensions::string_format(
                query,
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

        template<typename A>
        vector<A> search(
          const string& version, 
          const string& field, 
          const string& search,
          const string& type = "",
          int offset = 0, 
          int limit = 10, 
          const string& sortKey = "id", 
          const string& sortDirection = "Asc",
          bool exact = true
        ) {

          string matchExact = exact ? "=" : "~*";
          string sql = "select current from %s.%s where current->>'%s' %s '%s' and type = coalesce(%s, type) order by current->>'%s' %s offset %d limit %d";
          string typeFilter = type.size() > 0 ? wrapString(type) : "null";

          auto query = Extensions::string_format(
            sql,
            version.c_str(),
            resolve_type_to_string<A>().c_str(),
            field.c_str(),
            matchExact.c_str(),
            search.c_str(),
            typeFilter.c_str(),
            sortKey.c_str(),
            sortDirection.c_str(),
            offset,
            limit
          );

          return list<A>(version, offset, limit, sortKey, sortDirection, query);
        }

        template<typename A>
        vector<A> collect(const string& version, const string& field, const string& type, const vector<string>& keys) {
          string inCollection = join(keys.begin(), keys.end(), string(","));

          string sql = "select current from %s.%s where type = '%s' and %s in (%s) order by %s";

          auto query = string_format(
            sql,
            version.c_str(),
            resolve_type_to_string<A>().c_str(),
            type.c_str(),
            field.c_str(),
            inCollection.c_str(),
            field.c_str()
          );

          auto limit = keys.size();

          return list<A>(version, 0, limit, "id", "ASC", query);
        }

        template<typename A>
        shared_ptr<A> one(const string& version, const string& field, const string& search, const string& type = "") {
        
          string sql = "select current from %s.%s where current->>'%s' = '%s' and type = coalesce(%s, type) limit 1";
          string typeFilter = type.size() > 0 ? wrapString(type) : "null";

          auto query = Extensions::string_format(
            sql,
            version.c_str(),
            resolve_type_to_string<A>().c_str(),
            field.c_str(),
            search.c_str(),
            typeFilter.c_str()
          );

          auto a = list<A>(version, 0, 1, "id", "ASC", query);

          return a.size() == 0 ? nullptr : make_shared<A>(a.at(0));
        }

        template<typename A>
        int64_t count(const string& version, string type = "", string search = "") {
          string sql = "select count(1) result from %s.%s where type = coalesce(%s, type) %s";
          string typeFilter = type.size() > 0 ? wrapString(type) : "null";
          string searchFilter = search.size() > 0 ? (" and " + search) : "";

          auto query = Extensions::string_format(
            sql,
            version.c_str(),
            resolve_type_to_string<A>().c_str(),
            typeFilter.c_str(),
            searchFilter.c_str()
          );

          try {
            Postgres::Connection cnx;
            cnx.connect(connectionInfo.c_str());
            auto& resp = cnx.execute(query.c_str());
            return resp.template as<int64_t>(0);
          } catch (Postgres::ConnectionException e) {
            logger->error(e.what());
          } catch (Postgres::ExecutionException e) {
            logger->error(e.what());
          } catch (exception e) {
            logger->error(e.what());
          }

          return -1;
        }

      protected:
        string connectionInfo;        

      private:

      };     

    }
  }
}
