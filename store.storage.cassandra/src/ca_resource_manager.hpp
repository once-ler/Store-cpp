#pragma once

#include <thread>
#include <algorithm>
#include "spdlog/spdlog.h"
#include "store.common/src/time.hpp"
#include "store.common/src/group_by.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.storage.cassandra/src/ca_resource_common.hpp"
#include "store.storage.cassandra/src/ca_resource_modified.hpp"
#include "store.storage.cassandra/src/cassandra_base_client.hpp"
#include "store.storage.cassandra/src/ca_resource_purpose_handler.hpp"

using namespace std;
using namespace date;
using namespace chrono;
using namespace store::common;

namespace ioc = store::ioc;

namespace store::storage::cassandra {
  
  using RowToCaResourceModifiedCallback = std::function<void(CassFuture* future, void* data)>;
  using RowToCaResourceModifiedCallbackHandler = std::function<RowToCaResourceModifiedCallback(HandleCaResourceModifiedFunc&)>;
  using RowToCaResourceProcessedCallback = std::function<void(CassFuture* future, void* data)>;
  using ProcessUidOnCompleteFunc = std::function<void(const string&)>;

  RowToCaResourceModifiedCallback rowToCaResourceModifiedCallback;
  RowToCaResourceModifiedCallbackHandler rowToCaResourceModifiedCallbackHandler;
  RowToCaResourceProcessedCallback rowToCaResourceProcessedCallback;

  class CaResourceManager {
  public:
    const string version = "0.1.3";

    CaResourceManager(const string& keyspace_, const string& environment_, const string& store_, const string& dataType_, const string& purpose_) : 
      keyspace(keyspace_), environment(environment_), store(store_), dataType(dataType_), purpose(purpose_) {
        conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();

        // Inject compiled cql for threads to share.
        auto compileResourceModifiedPreStmt = fmt::format(
          ca_resource_modified_select_pre,
          keyspace,
          environment,
          store,
          dataType
        );
        ioc::ServiceProvider->RegisterInstanceWithKey<string>("ca_resource_modified_select_pre", make_shared<string>(compileResourceModifiedPreStmt));
        
    }
    ~CaResourceManager() = default;

    void fetchNextTasks(HandleCaResourceModifiedFunc caResourceModifiedHandler) {
      // Workflow is processed-functions -> modified-functions, but we define callbacks in reverse order.
      // Capture the user defined function that will be invoked in the callback.
      rowToCaResourceModifiedCallbackHandler = [this](HandleCaResourceModifiedFunc& caResourceModifiedHandler) {
        return [&caResourceModifiedHandler, this](CassFuture* future, void* data) {
          rowToCaResourceModifiedTapFunc(future, caResourceModifiedHandler);
        };
      };

      // Static rowToCaResourceModifiedHandler() will call rowToCaResourceModifiedCallback().
      rowToCaResourceModifiedCallback = rowToCaResourceModifiedCallbackHandler(caResourceModifiedHandler);

      // Static rowToCaResourceProcessedHandler() will call rowToCaResourceProcessedCallback().
      rowToCaResourceProcessedCallback = [this](CassFuture* future, void* data) {
        rowToCaResourceProcessedTapFunc(future);
      };

      #ifdef DEBUG
      cout << "ca_resource_processed_select: " << ca_resource_processed_select << endl
        << "keyspace: " << keyspace << endl
        << "environment: " << environment << endl
        << "store: " << store << endl
        << "dataType: " << dataType << endl
        << "purpose: " << purpose << endl;
      #endif

      auto compileResourceProcessedStmt = fmt::format(
        ca_resource_processed_select,
        keyspace,
        environment,
        store,
        dataType,
        purpose
      );

      #ifdef DEBUG
      cout << compileResourceProcessedStmt << endl;
      #endif

      conn->executeQueryAsync(compileResourceProcessedStmt.c_str(), rowToCaResourceProcessedHandler);
    }
    
  private:
    string caResourceProcessedTable = "ca_resource_processed";
    shared_ptr<CassandraBaseClient> conn = nullptr;
    string keyspace;
    string environment;
    string store;
    string dataType;
    string purpose;

    string ca_resource_processed_select = R"__(
      select uid from {}.ca_resource_processed
      where environment = '{}'
      and store = '{}'
      and type = '{}'
      and purpose = '{}' limit 1
    )__";

    string ca_resource_modified_select_pre = R"__(
      select * from {}.ca_resource_modified
      where environment = '{}'
      and store = '{}'
      and type = '{}'
    )__";

    static void rowToCaResourceProcessedHandler(CassFuture* future, void* data) {
      rowToCaResourceProcessedCallback(future, data);
    }

    static void rowToCaResourceModifiedHandler(CassFuture* future, void* data) {
      rowToCaResourceModifiedCallback(future, data);
    }

    void rowToCaResourceModifiedTapFunc(CassFuture* future, HandleCaResourceModifiedFunc& caResourceModifiedHandler) {
      CassError code = cass_future_error_code(future);
      if (code != CASS_OK) {
        // TODO: Write to log.
        string error = get_error(future);
      } else {
        const CassResult* result = cass_future_get_result(future);
        CassIterator* iterator = cass_iterator_from_result(result);
        
        vector<shared_ptr<ca_resource_modified>> processed;
        vector<ca_resource_modified> unprocessed;

        while (cass_iterator_next(iterator)) {
          const CassRow* row = cass_iterator_get_row(iterator);
          ca_resource_modified crm;
          crm.rowToType(row);

          unprocessed.push_back(crm);
        }
        cass_iterator_free(iterator);
        cass_result_free(result);

        // Make sure each id is the latest.
        std::map<string, vector<ca_resource_modified>> grouped = groupBy(unprocessed.begin(), unprocessed.end(), [](const ca_resource_modified& crm){ return crm.id; }); 
        for (auto& c : grouped) {
          auto l = c.second;
          std::sort(l.begin(), l.end(), 
            [](const ca_resource_modified& lhs, const ca_resource_modified& rhs) { 
              return lhs.start_time > rhs.start_time;  
            });

          auto h = l.begin();
          ca_resource_modified crm = *h;

          #ifdef DEBUG
          printCaResourceModified(&crm);
          #endif 

          auto c1 = make_shared<ca_resource_modified>(
            crm.environment,
            crm.store,
            crm.type,
            crm.start_time,
            crm.id,
            crm.oid,
            crm.uid,
            crm.current
          );

          // User defined handler can manipulate "current" field.
          // This will be written back to ca_resource_processed table.
          auto c2 = caResourceModifiedHandler(c1);

          // Log as processed in cassandra.
          processed.push_back(c2);
        }
        
        // Create batch statements.
        vector<CassStatement*> statements;
        for (auto& c1 : processed) {
          // Log.
          CassUuid key;
          cass_uuid_from_string(c1->uid.c_str(), &key);

          string current = c1->current,
            id = c1->id,
            oid = c1->oid;

          int64_t start_time = c1->start_time; 

          auto stmt = conn->getInsertStatement(fmt::format("{}.{}", keyspace, caResourceProcessedTable), 
            {"environment", "store", "type", "purpose", "uid", "current", "id", "oid", "start_time"}, 
            environment, store, dataType, purpose, key, current, id, oid, start_time);

          #ifdef DEBUG
          char key_str[CASS_UUID_STRING_LENGTH];
          cass_uuid_string(key, key_str);
          cout << "Logging\n" << key_str << endl;
          #endif

          statements.push_back(stmt);
        }

        conn->insertAsync(statements);
      }
    }

    void rowToCaResourceProcessedTapFunc(CassFuture* future) {
      CassError code = cass_future_error_code(future);
      if (code != CASS_OK) {
        // TODO: Write to log.
        string error = get_error(future);
      } else {
        const CassResult* result = cass_future_get_result(future);
        
        const CassRow* row = cass_result_first_row(result);

        string uid;
        if (row) {
          std::map<string, string> m = getCellAsUuid(row, {"uid"});
          uid = m["uid"];
        } else {
          auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
          CassUuid uuid;
          auto twoDaysAgoMilli = store::common::getMillisecondsFromTimePoint(daysAgo(2));

          conn->getUUIDFromTimestamp(twoDaysAgoMilli, uuid);
          char key_str[CASS_UUID_STRING_LENGTH];
          cass_uuid_string(uuid, key_str);
          uid = string(key_str);
        }

        #ifdef DEBUG
        cout << "Next uuid: " << uid << endl;
        #endif

        cass_result_free(result);

        // Apply user defined tap function given uuid.
        processUidOnCompleteHandler(uid);
      }
    }

    void processUidOnCompleteHandler(const string& uid) {
      // auto pre_stmt = (char*)data;
      auto pre_stmt = ioc::ServiceProvider->GetInstanceWithKey<string>("ca_resource_modified_select_pre");
      #ifdef DEBUG
      cout << "pre_stmt: " << *pre_stmt << endl << "uid: " << uid << endl;
      #endif

      // After obtaining the next uuid to process, compile next select statement for ca_resource_modified table.
      auto compileResourceModifiedStmt = fmt::format(
        "{} and uid > {} limit 20",
        *pre_stmt, 
        uid
      );

      #ifdef DEBUG
      cout << compileResourceModifiedStmt << endl;
      #endif
      
      auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
      conn->executeQueryAsync(compileResourceModifiedStmt.c_str(), rowToCaResourceModifiedHandler);
    }
   
    string get_error(CassFuture* future) {
      const char* message;
      size_t message_length;
      cass_future_error_message(future, &message, &message_length);
      #ifdef DEBUG
      fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
      #endif
      return string(message, message_length);
    }

    void printCaResourceModified(ca_resource_modified* c) {
      cout << endl << c->environment << endl
        << c->store << endl
        << c->type << endl
        << c->start_time << endl
        << c->uid << endl
        << c->id << endl
        << c->oid << endl
        << c->current << endl;
    }

  };

}
