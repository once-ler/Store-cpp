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

#include "store.storage.cassandra/src/cassandra_base_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::storage::cassandra;

namespace ioc = store::ioc;

int main(int argc, char* argv[]) {

  auto client = make_shared<CassandraBaseClient>("127.0.0.1", "cassandra", "cassandra");

  ioc::ServiceProvider->RegisterInstance<CassandraBaseClient>(client);

  auto conn = ioc::ServiceProvider->GetInstance<CassandraBaseClient>();

  conn->tryConnect();

  const char* query = "INSERT INTO async (key, i64) VALUES (?, ?);";
  CassStatement* statement = cass_statement_new(query, 2);
  cass_statement_bind_string(statement, 0, "ABC123");
    


}
