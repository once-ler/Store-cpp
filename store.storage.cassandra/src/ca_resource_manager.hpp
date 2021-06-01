#pragma once

#include <sstream>
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

        stringstream ss;
        ss << this;
        string addr = ss.str();
        ioc::ServiceProvider->RegisterInstanceWithKey<string>("ca_resource_processed_select", make_shared<string>(ca_resource_processed_select));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>("ca_resource_modified_select", make_shared<string>(ca_resource_modified_select));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>("ca_resource_processed", make_shared<string>(caResourceProcessedTable));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>(addr + ":environment", make_shared<string>(environment));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>(addr + ":keyspace", make_shared<string>(keyspace));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>(addr + ":purpose", make_shared<string>(purpose));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>(addr + ":data_type", make_shared<string>(dataType));
        ioc::ServiceProvider->RegisterInstanceWithKey<string>(addr + ":store", make_shared<string>(store));
        ioc::ServiceProvider->RegisterInstanceWithKey<std::chrono::milliseconds>(addr + ":wait_time", make_shared<std::chrono::milliseconds>(wait_time));
    }
    ~CaResourceManager() = default;

    void fetchNextTasks(HandleCaResourceModifiedFunc caResourceModifiedHandler, CaResourceManager* caResourceManager = NULL) {
      // Workflow is processed-functions -> modified-functions, but we define callbacks in reverse order.
      // Capture the user defined function that will be invoked in the callback.
      rowToCaResourceModifiedCallbackHandler = [this](HandleCaResourceModifiedFunc& caResourceModifiedHandler) {
        return [caResourceModifiedHandler, this](CassFuture* future, void* data) {
          rowToCaResourceModifiedTapFunc(future, caResourceModifiedHandler, this);
        };
      };

      // Static rowToCaResourceModifiedHandler() will call rowToCaResourceModifiedCallback().
      rowToCaResourceModifiedCallback = rowToCaResourceModifiedCallbackHandler(caResourceModifiedHandler);

      // Static rowToCaResourceProcessedHandler() will call rowToCaResourceProcessedCallback().
      rowToCaResourceProcessedCallback = [caResourceModifiedHandler, this](CassFuture* future, void* data) {
        rowToCaResourceProcessedTapFunc(future, caResourceModifiedHandler, this);
      };

      // caResourceManager would NOT be NULL if called by callback. 
      stringstream ss;
      ss << (caResourceManager == NULL ? this : caResourceManager);
      string managerAddr = ss.str();

      string ca_resource_processed_select = *(ioc::ServiceProvider->GetInstanceWithKey<string>("ca_resource_processed_select")),
        keyspace = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":keyspace")), 
        environment = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":environment")), 
        store = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":store")), 
        dataType = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":data_type")), 
        purpose = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":purpose"));    

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

      auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
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
    std::chrono::milliseconds wait_time = std::chrono::milliseconds(4000);

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
      and uid > {} limit 20
    )__";

    static void rowToCaResourceProcessedHandler(CassFuture* future, void* data) {
      rowToCaResourceProcessedCallback(future, data);
    }

    static void rowToCaResourceModifiedHandler(CassFuture* future, void* data) {
      rowToCaResourceModifiedCallback(future, data);
    }

    void rowToCaResourceModifiedTapFunc(CassFuture* future, HandleCaResourceModifiedFunc caResourceModifiedHandler, CaResourceManager* caResourceManager) {
      CassError code = cass_future_error_code(future);
      stringstream ss;
      ss << caResourceManager;
      string managerAddr = ss.str();
      
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

          string environment = c1->environment,
            store = c1->store,
            dataType = c1->type,
            purpose = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":purpose")), 
            current = c1->current,
            id = c1->id,
            oid = c1->oid,
            keyspace = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":keyspace")),
            caResourceProcessedTable = *(ioc::ServiceProvider->GetInstanceWithKey<string>("ca_resource_processed"));

          int64_t start_time = c1->start_time;

          #ifdef DEBUG
          cout << "environment: " << environment << endl;
          cout << "store: " << store << endl;
          cout << "dataType: " << dataType << endl;
          cout << "purpose: " << purpose << endl;
          cout << "key: " << c1->uid << endl;
          cout << "current: " << current << endl;
          cout << "id: " << id << endl;
          cout << "oid: " << oid << endl;
          cout << "start_time: " << start_time << endl;
          cout << "keyspace: " << keyspace << endl;
          cout << "caResourceProcessedTable: " << caResourceProcessedTable << endl;
          #endif

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

        auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
        conn->insertAsync(statements);
      }

      // Recurse.
      auto wait_time = ioc::ServiceProvider->GetInstanceWithKey<std::chrono::milliseconds>(managerAddr + ":wait_time");
      std::this_thread::sleep_for(*wait_time);

      #ifdef DEBUG
      cout << "Waited ms: " << to_string(wait_time->count()) << endl;
      #endif
      caResourceManager->fetchNextTasks(caResourceModifiedHandler, caResourceManager);
    }

    void rowToCaResourceProcessedTapFunc(CassFuture* future, HandleCaResourceModifiedFunc caResourceModifiedHandler, CaResourceManager* caResourceManager) {
      CassError code = cass_future_error_code(future);
      stringstream ss;
      ss << caResourceManager;
      string managerAddr = ss.str();
      
      if (code != CASS_OK) {
        // TODO: Write to log.
        string error = get_error(future);

        auto wait_time = ioc::ServiceProvider->GetInstanceWithKey<std::chrono::milliseconds>(managerAddr + ":wait_time");
        std::this_thread::sleep_for(*wait_time);
        caResourceManager->fetchNextTasks(caResourceModifiedHandler, caResourceManager);
      } else {
        const CassResult* result = cass_future_get_result(future);
        
        const CassRow* row = cass_result_first_row(result);

        auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
          
        string uid;
        if (row) {
          std::map<string, string> m = getCellAsUuid(row, {"uid"});
          uid = m["uid"];
        } else {
          // auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
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
        string ca_resource_modified_select = *(ioc::ServiceProvider->GetInstanceWithKey<string>("ca_resource_modified_select")),
        keyspace = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":keyspace")), 
        environment = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":environment")), 
        store = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":store")), 
        dataType = *(ioc::ServiceProvider->GetInstanceWithKey<string>(managerAddr + ":data_type")); 

        #ifdef DEBUG
        cout << "ca_resource_modified_select: " << ca_resource_modified_select << endl
          << "keyspace: " << keyspace << endl
          << "environment: " << environment << endl
          << "store: " << store << endl
          << "dataType: " << dataType << endl
          << "uid: " << uid << endl;
        #endif

        auto compileResourceModifiedStmt = fmt::format(
          ca_resource_processed_select,
          keyspace,
          environment,
          store,
          dataType,
          uid
        );

        #ifdef DEBUG
        cout << compileResourceModifiedStmt << endl;
        #endif

        conn->executeQueryAsync(compileResourceModifiedStmt.c_str(), rowToCaResourceModifiedHandler);
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
