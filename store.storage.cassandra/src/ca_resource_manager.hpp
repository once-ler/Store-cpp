#pragma once

#include <thread>
#include <chrono>
#include "spdlog/spdlog.h"
#include "store.storage.cassandra/src/ca_resource_common.hpp"
#include "store.storage.cassandra/src/ca_resource_modified.hpp"
#include "store.storage.cassandra/src/cassandra_base_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace std;

namespace ioc = store::ioc;

namespace store::storage::cassandra {
  
  using RowToCaResourceModifiedCallback = std::function<void(CassFuture* future, void* data)>;
  using RowToCaResourceProcessedCallback = std::function<void(CassFuture* future, void* data)>;

  class CaResourceManager {
  public:
    const string version = "0.1.0";

  private:
    const char* select_query_2 = "SELECT * FROM dwh.ca_resource_modified limit 10";

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

    void rowToCaResourceModifiedTapFunc(CassFuture* future) {
      CassError code = cass_future_error_code(future);
      if (code != CASS_OK) {
        print_error(future);
      } else {
        const CassResult* result = cass_future_get_result(future);
        CassIterator* iterator = cass_iterator_from_result(result);
        while (cass_iterator_next(iterator)) {
          const CassRow* row = cass_iterator_get_row(iterator);
          ca_resource_modified crm;
          auto c = crm.rowToType(row);

          printCaResourceModified(c);  
        }
        cass_iterator_free(iterator);
        cass_result_free(result);
      }
    }

    void rowToCaResourceProcessedTapFunc(CassFuture* future) {
      CassError code = cass_future_error_code(future);
      if (code != CASS_OK) {
        print_error(future);
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
          conn->getUUIDFromTimestamp(0L, uuid);
          char key_str[CASS_UUID_STRING_LENGTH];
          cass_uuid_string(uuid, key_str);
          uid = string(key_str);
        }

        cout << uid << endl;
        
        cass_result_free(result);
      }
    }

    void print_error(CassFuture* future) {
      const char* message;
      size_t message_length;
      cass_future_error_message(future, &message, &message_length);
      fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
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
