/*
g++ -std=c++14 -Wall -I ../../../../Store-cpp \
-I ../../../../spdlog/include \
-I ../../../../json/single_include/nlohmann \
-o testing ../index-base-client-spec2.cpp \
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

struct Basic_ {
  const char* key;
  cass_bool_t bln;
  cass_float_t flt;
  cass_double_t dbl;
  cass_int32_t i32;
  cass_int64_t i64;
};
typedef struct Basic_ Basic;

const char* select_query = "SELECT * FROM examples.async limit 10";

void print_error(CassFuture* future) {
  const char* message;
  size_t message_length;
  cass_future_error_message(future, &message, &message_length);
  fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
}

auto tapFunc = [](CassFuture* future) {
  CassError code = cass_future_error_code(future);
  if (code != CASS_OK) {
    print_error(future);
  } else {
    const CassResult* result = cass_future_get_result(future);
    CassIterator* iterator = cass_iterator_from_result(result);
    while (cass_iterator_next(iterator)) {
      CassUuid key;
      char key_str[CASS_UUID_STRING_LENGTH];
      cass_uint64_t value = 0;
      const CassRow* row = cass_iterator_get_row(iterator);

      cass_value_get_uuid(cass_row_get_column(row, 0), &key);

      cass_uuid_string(key, key_str);
      cass_value_get_int64(cass_row_get_column(row, 1), (cass_int64_t*)&value);

      printf("%s, %llu\n", key_str, (unsigned long long)value);

      Basic output;
      Basic* basic = &output;

      // https://datastax.github.io/cpp-driver/topics/basics/handling_results/
      size_t string_value_length;
      cass_value_get_string(cass_row_get_column_by_name(row, "key"), &basic->key, &string_value_length);
      cass_value_get_bool(cass_row_get_column(row, 1), &basic->bln);
      cass_value_get_double(cass_row_get_column(row, 2), &basic->dbl);
      cass_value_get_float(cass_row_get_column(row, 3), &basic->flt);
      cass_value_get_int32(cass_row_get_column(row, 4), &basic->i32);
      cass_value_get_int64(cass_row_get_column(row, 5), &basic->i64);

      cout << string_value_length << endl;
      cout << string(basic->key, string_value_length) << endl;
    }
    cass_iterator_free(iterator);
    cass_result_free(result);
  }
};

auto callback = [](CassFuture* future, void* data) {
  tapFunc(future);
};

void testCallback() {
  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();

  conn->executeQueryAsync(select_query, callback);
}

void testTapFunc() {
  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();

  conn->executeQuery(select_query, make_shared<CassandraFutureTapFunc>(tapFunc));
}

auto main(int argc, char* argv[]) -> int {

  auto client = make_shared<CassandraBaseClient>("127.0.0.1", "cassandra", "cassandra");
  client->tryConnect();

  // Register in main().  
  ioc::ServiceProvider->RegisterInstance<CassandraBaseClient>(client);

  cout << "Testing async callback:\n";
  testCallback();

  cout << "Testing blocking tap function:\n";
  testTapFunc();

  fprintf(stdout, "Staying put");

  std::thread t1([]{ while (true) std::this_thread::sleep_for(std::chrono::milliseconds(2000));});
  t1.join();

  return 0;
}

