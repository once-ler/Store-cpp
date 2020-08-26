#pragma once

#include <thread>
#include "spdlog/spdlog.h"
#include "store.common/src/time.hpp"
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
    const string version = "0.1.1";

    CaResourceManager(const string& environment_): environment(environment_) {
      conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
    }
    ~CaResourceManager() = default;

    std::function<void(HandleCaResourceModifiedFunc)> fetchNextTasks(const string& keyspace, const string& store, const string& dataType, const string& purpose) {
      return [&, this](HandleCaResourceModifiedFunc caResourceModifiedHandler) {

        // Workflow is processed-functions -> modified-functions, but we define callbacks in reverse order.
        
        // Capture the user defined function that will be invoked in the callback.
        rowToCaResourceModifiedCallbackHandler = [this](HandleCaResourceModifiedFunc& caResourceModifiedHandler) {
          return [&caResourceModifiedHandler, this](CassFuture* future, void* data) {
            rowToCaResourceModifiedTapFunc(future, caResourceModifiedHandler);
          };
        };

        // Static rowToCaResourceModifiedHandler() will call rowToCaResourceModifiedCallback().
        rowToCaResourceModifiedCallback = rowToCaResourceModifiedCallbackHandler(caResourceModifiedHandler);

        auto processUidOnCompleteHandler = [&, this](const string& uid) {          
          auto compileResourceModifiedStmt = fmt::format(
            ca_resource_modified_select,
            environment,
            store,
            dataType,
            purpose,
            uid
          );

          conn->executeQueryAsync(compileResourceModifiedStmt.c_str(), rowToCaResourceModifiedHandler);
        };

        // Static rowToCaResourceProcessedHandler() will call rowToCaResourceProcessedCallback().
        // Capture the processUidOnCompleteHandler that will be invoked in the callback.
        rowToCaResourceProcessedCallback = [&processUidOnCompleteHandler, this](CassFuture* future, void* data) {
          rowToCaResourceProcessedTapFunc(future, processUidOnCompleteHandler);
        };

        auto compileResourceProcessedStmt = fmt::format(
          ca_resource_processed_select,
          environment,
          store,
          dataType,
          purpose
        );

        conn->executeQueryAsync(compileResourceProcessedStmt.c_str(), rowToCaResourceProcessedHandler);
      };
    }

  private:
    shared_ptr<CassandraBaseClient> conn = nullptr;
    string environment; 
    string caResourceProcessedTable = "ca_resource_processed";
    
    static void rowToCaResourceProcessedHandler(CassFuture* future, void* data) {
      rowToCaResourceProcessedCallback(future, data);
    }

    static void rowToCaResourceModifiedHandler(CassFuture* future, void* data) {
      rowToCaResourceModifiedCallback(future, data);
    }

    string ca_resource_processed_select = R"__(
      select uid from {}.ca_resource_processed
      where environment = '{}'
      and store = '{}'
      and type = '{}'
      and purpose = '{}' limit 1
    )__";

    string ca_resource_modified_select = R"__(
      select * from {}.ca_resource_modified
      where environment = '{}'
      and store = '{}'
      and type = '{}'
      and uid > {}
      limit 20
    )__";

    void rowToCaResourceModifiedTapFunc(CassFuture* future, HandleCaResourceModifiedFunc& caResourceModifiedHandler) {
      CassError code = cass_future_error_code(future);
      if (code != CASS_OK) {
        // TODO: Write to log.
        string error = get_error(future);
      } else {
        const CassResult* result = cass_future_get_result(future);
        CassIterator* iterator = cass_iterator_from_result(result);
        while (cass_iterator_next(iterator)) {
          const CassRow* row = cass_iterator_get_row(iterator);
          ca_resource_modified crm;
          auto c = crm.rowToType(row);

          #ifdef DEBUG
          printCaResourceModified(c);
          #endif 

          shared_ptr<ca_resource_modified> c1(c);
          
          caResourceModifiedHandler(c1);

          // Log.
          CassUuid key;
          cass_uuid_from_string(c1->uid.c_str(), &key);

          auto stmt = conn->getInsertStatement(caResourceProcessedTable, 
            {"environment", "store", "type", "start_time", "id", "oid", "uid", "current"}, 
            c1->environment, c1->store, c1->type, c1->start_time, c1->id, c1->oid, key, c1->current);
          
          conn->insertAsync(stmt);
        }
        cass_iterator_free(iterator);
        cass_result_free(result);
      }
    }

    // ProcessUidOnCompleteFunc must be const-qualified.
    void rowToCaResourceProcessedTapFunc(CassFuture* future, const ProcessUidOnCompleteFunc& processUidOnCompleteHandler) {
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
        cout << uid << endl;
        #endif

        cass_result_free(result);

        // Apply user defined tap function given uuid.
        processUidOnCompleteHandler(uid);
      }
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
