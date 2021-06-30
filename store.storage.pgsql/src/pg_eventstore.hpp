#pragma once

#include <libpq-fe.h>
#include "postgres-exceptions.h"
#include "store.events/src/eventstore.hpp"
#include "store.storage.connection-pools/src/pgsql.hpp"
#include "store.common/src/logger.hpp"

using namespace std;
using namespace store::events;

namespace Postgres = db::postgres;
namespace Extensions = store::extensions;

namespace store::storage::pgsql {

  std::regex unexpected_disconnection_rgx("^server closed the connection unexpectedly");

  enum class ErrorType { none = 200, connection = 503, execution = 400, unknown = 500 };

  template<typename> class Client;

  template<typename T>
  class PgEventStore : public EventStore {
  public:
    explicit PgEventStore(Client<T>* session_) : session(session_) {}

    pair<int, string> Save() override {
      const auto& streams = groupBy(pending.begin(), pending.end(), [](IEvent& e) { return e.streamId; });
      string retval = "Succeeded";

      for (const auto& o : streams) {
        auto streamId = o.first;
        auto stream = o.second;
        auto eventIds = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(generate_uuid(), "$Q$"); });
        auto eventTypes = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(e.type, "$Q$"); });
        auto bodies = mapEvents<string>(stream, [](const IEvent& e) { return wrapString(e.data.dump(), "$Q$"); });
        
        auto stmt = Extensions::string_format(R"__(
          select %s.mt_append_event(%s, %s, array[%s]::uuid[], array[%s]::varchar[], array[%s]::jsonb[])
          )__",
          dbSchema.c_str(),
          wrapString(streamId, "$Q$").c_str(),
          eventTypes.at(0).c_str(),
          join(eventIds.begin(), eventIds.end(), string(",")).c_str(),
          join(eventTypes.begin(), eventTypes.end(), string(",")).c_str(),
          join(bodies.begin(), bodies.end(), string(",")).c_str()
        );

        std::shared_ptr<PostgreSQLConnection> conn = nullptr;
        
        try {
          conn = session->pool->borrow();
          
          // resp is [current_version, ...sequence num]
          // https://stackoverflow.com/questions/17947863/error-expected-primary-expression-before-templated-function-that-try-to-us
          const auto& resp = conn->sql_connection->execute(stmt.c_str()).template asArray<int64_t>(0);
          
        } catch (Postgres::ConnectionException e) {
          retval = e.what();
          session->logger->error(retval.c_str());
          return make_pair(static_cast<int>(ErrorType::connection), retval);
        } catch (Postgres::ExecutionException e) {
          retval = e.what();
          session->logger->error(retval.c_str());
          int errType = static_cast<int>(ErrorType::execution);

          if (regex_search(retval, unexpected_disconnection_rgx))
            errType = static_cast<int>(ErrorType::connection);
          
          return make_pair(errType, retval);
        } catch (exception e) {
          retval = e.what();
          session->logger->error(retval.c_str());
          
          return make_pair(static_cast<int>(ErrorType::unknown), retval);
        }

        if (conn) 
          session->pool->unborrow(conn);
      }

      // Clear pending collection.
      this->Reset();
      
      return make_pair(static_cast<int>(ErrorType::none), retval);
    }

    int64_t LastExecution(const string& type, const string& subscriber, int64_t newSeqId = -1) override {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
        
      int64_t seqId = 0;

      try {
        conn = session->pool->borrow();
        
        string createTable = Extensions::string_format(R"SQL(
          create table if not exists "%s"."mt_last_execution" (
            stream uuid not null REFERENCES %s.mt_streams ON DELETE CASCADE,
            subscriber varchar(250) not null,
            seq_id bigint,
            primary key(stream, subscriber)
          );                
        )SQL", dbSchema.c_str(), dbSchema.c_str());

        conn->sql_connection->execute(createTable.c_str());
        
        auto sql = Extensions::string_format("select seq_id from %s.mt_last_execution where stream = '%s' and subscriber = '%s'",
          dbSchema.c_str(),
          generate_uuid_v3(type.c_str()).c_str(),
          subscriber.c_str()
        );

        auto& resp = conn->sql_connection->execute(sql.c_str());

        auto it = resp.begin();

        if ((*it).num() > 0) {
          seqId = (*it).as<int64_t>(0);
        } else {
          string insertDefault = Extensions::string_format(R"SQL(
            insert into "%s"."mt_last_execution" (stream, subscriber, seq_id) values ('%s', '%s', -1) on conflict do nothing;
          )SQL", dbSchema.c_str(), generate_uuid_v3(type.c_str()).c_str(), subscriber.c_str());

          conn->sql_connection->execute(insertDefault.c_str());
        }

        if (newSeqId > -1) {
          string setNextSeqId = Extensions::string_format(R"SQL(
            update "%s"."mt_last_execution" set seq_id = %lld where stream = '%s' and subscriber = '%s';
          )SQL", dbSchema.c_str(), (long long)newSeqId, generate_uuid_v3(type.c_str()).c_str(), subscriber.c_str());

          conn->sql_connection->execute(setNextSeqId.c_str());
          seqId = newSeqId;
        }
        
      } catch (Postgres::ConnectionException e) {
        session->logger->error(e.what());
      } catch (Postgres::ExecutionException e) {
        session->logger->error(e.what());
      } catch (exception e) {
        session->logger->error(e.what());
      }

      if (conn)
        session->pool->unborrow(conn);
      return seqId;
    }

    vector<IEvent> Search(const string& type, int64_t fromSeqId, int limit = 10, bool exact = true, vector<string> mustExistKeys = {}, map<string, vector<string>> filters = {}) override {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;

      vector<IEvent> events;

      string matchExact = exact ? "=" : "~*";

      // data field must contain the following keys.            
      vector<string> mustExistKeysQuoted;
      std::transform(mustExistKeys.begin(), mustExistKeys.end(), std::back_inserter(mustExistKeysQuoted), [](const string& s) { return wrapString(s); });
      string mustExistKeysQuotedString = join(mustExistKeysQuoted.begin(), mustExistKeysQuoted.end(), string(","));
      string matchMustExistKeys = mustExistKeysQuotedString.size() > 0 ? string_format(" and data ?& array[%s]", mustExistKeysQuotedString.c_str()) : "";

      vector<string> filtersQuoted;
      for(auto& e : filters) {
        vector<string> q;
        std::transform(e.second.begin(), e.second.end(), std::back_inserter(q), [](const string& s) { return wrapString(s); });
        string fragment = "data->>'" + e.first + "' in (" + join(q.begin(), q.end(), string(","))  + ")";
        filtersQuoted.push_back(move(fragment));
      }
      string filtersQuotedString = join(filtersQuoted.begin(), filtersQuoted.end(), string(" and "));
      string matchFilters = filtersQuotedString.size() > 0 ? string_format(" and %s", filtersQuotedString.c_str()) : "";

      try {
        conn = session->pool->borrow();

        auto sql = Extensions::string_format("select seq_id, id, stream_id, type, version, data, timestamp from %s.mt_events where type %s '%s' and seq_id > %lld %s %s limit %d",
          dbSchema.c_str(),
          matchExact.c_str(),
          type.c_str(),
          (long long)fromSeqId,
          matchMustExistKeys.c_str(),
          matchFilters.c_str(),
          limit
        );

        auto& resp = conn->sql_connection->execute(sql.c_str());

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

      if (conn)
        session->pool->unborrow(conn);

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
}