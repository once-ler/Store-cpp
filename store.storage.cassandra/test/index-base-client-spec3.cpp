/*
g++ -std=c++14 -Wall -I ../../../../Store-cpp \
-I ../../../../spdlog/include \
-I ../../../../json/single_include/nlohmann \
-o testing ../index-base-client-spec3.cpp \
-L /usr/lib64 \
-L /usr/lib/x86_64-linux-gnu \
-L/usr/local/lib -lpthread \
-lcassandra \
-luuid
*/

#include <thread>
#include <chrono>
#include "spdlog/spdlog.h"
#include "store.storage.cassandra/src/cassandra_base_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::storage::cassandra;

namespace ioc = store::ioc;

auto getCellAsString = [](const CassRow* row, const vector<string>& fields) -> std::map<string, string> {
  std::map<string, string> kv;

  for (const auto& k : fields) {
    const char* string_value;
    size_t string_value_length;
    cass_value_get_string(cass_row_get_column_by_name(row, k.c_str()), &string_value, &string_value_length);
    string v(string_value, string_value_length);
    kv[k] = v;
  }

  return kv;
};

auto getCellAsInt64 = [](const CassRow* row, const vector<string>& fields) -> std::map<string, cass_int64_t> {
  std::map<string, cass_int64_t> kv;

  for (const auto& k : fields) {
    cass_int64_t int64_value;
    cass_value_get_int64(cass_row_get_column_by_name(row, k.c_str()), &int64_value);
    kv[k] = int64_value;
  }

  return kv;
};

auto getCellAsUuid = [](const CassRow* row, const vector<string>& fields) -> std::map<string, string> {
  std::map<string, string> kv;
  char key_str[CASS_UUID_STRING_LENGTH];

  for (const auto& k : fields) {
    CassUuid key;
    cass_value_get_uuid(cass_row_get_column_by_name(row, k.c_str()), &key);
    cass_uuid_string(key, key_str);
    kv[k] = string(key_str);
  }

  return kv;
};

class ca_resource_modified {
public:
  ca_resource_modified() = default;
  ~ca_resource_modified() = default;
  string environment;
  string store;
  string type;
  cass_int64_t start_time;
  string id;
  string oid;
  string uid;
  string current;

  ca_resource_modified* rowToType(const CassRow* row) {
    this->getString(row)
      ->getInt64(row)
      ->getUuid(row);

    return this;
  }

private:
  ca_resource_modified* getString(const CassRow* row) {
    vector<string> k{"environment", "store", "type", "id", "oid", "current"};
    std::map<string, string> m = getCellAsString(row, k);
    environment = m[k.at(0)];
    store = m[k.at(1)];
    type = m[k.at(2)];
    id = m[k.at(3)];
    oid = m[k.at(4)];
    current = m[k.at(5)];
    return this;
  }

  ca_resource_modified* getInt64(const CassRow* row) {
    vector<string> k{"start_time"};
    std::map<string, cass_int64_t> m = getCellAsInt64(row, k);
    start_time = m[k.at(0)];
    return this;
  }

  ca_resource_modified* getUuid(const CassRow* row) {
    vector<string> k{"uid"};
    std::map<string, string> m = getCellAsUuid(row, k);
    uid = m[k.at(0)];
    return this;
  }
};

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

void print_error(CassFuture* future) {
  const char* message;
  size_t message_length;
  cass_future_error_message(future, &message, &message_length);
  fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
}

auto printCaResourceModified(ca_resource_modified* c) {
  cout << endl << c->environment << endl
    << c->store << endl
    << c->type << endl
    << c->start_time << endl
    << c->uid << endl
    << c->id << endl
    << c->oid << endl
    << c->current << endl;
}

auto rowToCaResourceModifiedTapFunc = [](CassFuture* future) {
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
};

auto rowToCaResourceProcessedTapFunc = [](CassFuture* future) {
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
    
    // getUUIDFromTimestamp

    /*
    CassIterator* iterator = cass_iterator_from_result(result);
    while (cass_iterator_next(iterator)) {
      const CassRow* row = cass_iterator_get_row(iterator);
      
      std::map<string, string> m = getCellAsUuid(row, {"uid"});
      string uid = m["uid"];
      cout << uid << endl;
    }
    
    cass_iterator_free(iterator);
    */

    cass_result_free(result);
  }
};

auto rowToCaResourceModifiedCallback = [](CassFuture* future, void* data) {
  rowToCaResourceModifiedTapFunc(future);
};

auto rowToCaResourceProcessedCallback = [](CassFuture* future, void* data) {
  rowToCaResourceProcessedTapFunc(future);
};


void testCallback2() {
  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
  conn->executeQueryAsync(select_query_2, rowToCaResourceModifiedCallback);
}

void testCallback3() {
  
  auto stmt = fmt::format(ca_resource_modified_select, 
    "dwh",
    "development",
    "IKEA",
    "Sales",
    "322e1f20-ceb4-11ea-b971-d75915cc7140");

  cout << stmt << endl;

  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
  conn->executeQueryAsync(stmt.c_str(), rowToCaResourceModifiedCallback);
}

void testCallback4() {
  
  cout << ca_resource_processed_select << endl;

  auto stmt = fmt::format(ca_resource_processed_select, 
    "dwh",
    "development",
    "IKEA",
    "Sales",
    "forecast");

  cout << stmt << endl;

  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();
  conn->executeQueryAsync(stmt.c_str(), rowToCaResourceProcessedCallback);
}

auto main(int argc, char* argv[]) -> int {

  auto client = make_shared<CassandraBaseClient>("127.0.0.1", "cassandra", "cassandra");
  client->tryConnect();

  // Register in main().  
  ioc::ServiceProvider->RegisterInstance<CassandraBaseClient>(client);

  cout << "Testing another async callback:\n";
  // testCallback2();

  cout << "Testing another async callback with formatted query:\n";
  // testCallback3();

  cout << "Testing yet another async callback with formatted query:\n";
  testCallback4();

  fprintf(stdout, "Staying put");

  std::thread t1([]{ while (true) std::this_thread::sleep_for(std::chrono::milliseconds(2000));});
  t1.join();

  return 0;
}
