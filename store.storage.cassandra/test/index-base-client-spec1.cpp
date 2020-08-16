/*
g++ -std=c++14 -Wall -I ../../../../Store-cpp \
-I ../../../../spdlog/include \
-I ../../../../json/single_include/nlohmann \
-o testing ../index-base-client-spec1.cpp \
-L /usr/lib64 \
-L /usr/lib/x86_64-linux-gnu \
-L/usr/local/lib -lpthread \
-lcassandra \
-luuid
*/

// #define DEBUG

#include <stdio.h>
#include <time.h>
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
  client->tryConnect();

  // Register in main().  
  ioc::ServiceProvider->RegisterInstance<CassandraBaseClient>(client);

  // Later...
  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();

  conn->executeQuery(createTable);
  conn->executeQuery("use examples");
  
  clock_t t; 
  t = clock(); 

  size_t i = 0;
  for (; i < 25000; i++) {
    auto statement = conn->getInsertStatement("async", {"key", "i64"}, "ABC" + std::to_string(i), (int64_t)i);
    conn->insertAsync(statement);
  }

  vector<CassStatement*> stmts;
  for (; i < 50000; i++) {
    stmts.emplace_back(conn->getInsertStatement("async", {"key", "i64"}, "ABC" + std::to_string(i), (int64_t)i));
  }
  conn->insertAsync(stmts);
  
  t = clock() - t; 
  double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
  
  printf("Inserted %lu in %f sec.\n", i, time_taken); 

  // Inserted 500000 in 7.169531 sec.
  fprintf(stdout, "Staying put");

  std::thread t1([]{ while (true) usleep(20000);});
  t1.join();

}
