/*
g++ -std=c++14 -Wall -I ../../../../Store-cpp \
-I ../../../../spdlog/include \
-I ../../../../json/single_include/nlohmann \
-I /usr/lib/x86_64-linux-gnu \
-o testing ../index-base-client-spec1.cpp \
-L/usr/local/lib -lpthread \
-lcassandra \
-luuid
*/

#include <unistd.h> //usleep
#include <thread>

#include "store.storage.cassandra/src/cassandra_base_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::storage::cassandra;

namespace ioc = store::ioc;

const char* createTable = "CREATE TABLE if not exists examples.async (key text, \
                                              bln boolean, \
                                              flt float, dbl double,\
                                              i32 int, i64 bigint, \
                                              PRIMARY KEY (key));";


int main(int argc, char* argv[]) {

  using namespace store::storage::cassandra;

  auto client = make_shared<CassandraBaseClient>("127.0.0.1", "cassandra", "cassandra");

  ioc::ServiceProvider->RegisterInstance<CassandraBaseClient>(client);

  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();

  conn->tryConnect();

  const char* query = "INSERT INTO async (key, i64) VALUES (?, ?);";
  CassStatement* statement = cass_statement_new(query, 2);
  // cass_statement_bind_string(statement, 0, "ABC123");
  BindCassParameter(statement, 0, "DEF456");
  cass_statement_bind_int64(statement, 1, (cass_int64_t)100);

  conn->executeQuery(createTable);
  conn->executeQuery("use examples");
  conn->insertAsync(statement);
  
  fprintf(stdout, "Staying put");

  std::thread t1([]{ while (true) usleep(20000);});
  t1.join();

}
