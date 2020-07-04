/*
g++ -g3 -std=c++14 -Wall \
-I ../../../../spdlog/include \
-I ../../../../tdspp \
-I ../../../../Store-cpp \
-I ../../../../connection-pool \
-I ../../../../json/single_include/nlohmann \
-o testing ../index-sql-notifier-spec.cpp \
-L /usr/lib64 \
-L /usr/lib/x86_64-linux-gnu \
-L/usr/local/lib -lpthread \
-lrdkafka \
-lcppkafka \
-luuid \
-ltdsclient -lsybdb \
-lpthread -lboost_system -lboost_filesystem
*/

#include <cppkafka/cppkafka.h>
#include <thread>
#include <chrono>
#include "store.storage.kafka/src/kafka_base_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.storage.sqlserver/src/mssql_dblib_base_client.hpp"
#include "store.storage.sqlserver/src/mssql_notifier.hpp"

using namespace std;
using namespace cppkafka;
using namespace store::storage::kafka;

using MsSqlDbLibBaseClient = store::storage::mssql::MsSqlDbLibBaseClient;
using Notifier = store::storage::mssql::Notifier;
  
shared_ptr<MsSqlDbLibBaseClient> getDbLibClient() {
  DBContext db_ctx("testing", "localhost", 1433, "test", "admin", "12345678", 30);  
  return make_shared<MsSqlDbLibBaseClient>(db_ctx, "test:pool");
}

shared_ptr<Notifier> getSqlNotifier(shared_ptr<MsSqlDbLibBaseClient> client) {
  string databaseName = "test", schemaName = "dbo", tableName = "testdata";
  return make_shared<Notifier>(client, databaseName, schemaName, tableName);
}

auto main(int agrc, char* argv[]) -> int {

}
