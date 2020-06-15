/*
g++ -std=c++14 -I /usr/local/include \
-I ../../../../spdlog/include \
-I ../../../../tdspp \
-I ../../../../Store-cpp \
-I ../../../../connection-pool \
-I ../../../../json/single_include/nlohmann \
../index-notifier.cpp -o bin/testing \
-L /usr/lib/x86_64-linux-gnu \
-L /usr/local/lib \
-L /usr/lib64 \
-luuid \
-ltdsclient -lsybdb \
-lpthread -lboost_system -lboost_filesystem

*/

#include "store.models/src/ioc/service_provider.hpp"
#include "store.storage.sqlserver/src/mssql_dblib_base_client.hpp"
#include "store.storage.sqlserver/src/mssql_notifier.hpp"

using namespace std;
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

auto main(int argc, char* argv[]) -> int {
  auto client = getDbLibClient();
  auto notifier = getSqlNotifier(client);
  
  // Start the listener.
  notifier->start([](string msg) {
    cout << "Received:\n\n" << msg << endl;
  });

  // Allow background thread to start.
  this_thread::sleep_for(chrono::seconds(1));

  // Send an update to trigger a message.
  notifier->executeNonQuery("update dbo.testdata set [text] = 'abcd' where id = 4");
  this_thread::sleep_for(chrono::milliseconds(200));
  notifier->executeNonQuery("update dbo.testdata set [text] = 'efgh' where id = 4");
  this_thread::sleep_for(chrono::milliseconds(200));
  notifier->executeNonQuery("update dbo.testdata set [text] = 'ijkl' where id = 4");
  this_thread::sleep_for(chrono::milliseconds(200));

  // Monitor.
  auto fieldValues = notifier->monitor();

  for (const auto& e : fieldValues) {
    for (int i = 0; i < e.size(); i++)
      cout << e.at(i) << endl;
  }
  
  notifier->stop();

  // notifier->uninstall();

  // notifier->cleanDatabase();

  // Don't leave.
  while (true) {this_thread::sleep_for(chrono::seconds(1));}

  return 0;
}
