/*
g++ -std=c++14 -Wall -I ../../../../Store-cpp \
-I ../../../../spdlog/include \
-I ../../../../json/single_include/nlohmann \
-I ../../../../date/include \
-o testing ../index-base-client-spec4.cpp \
-L /usr/lib64 \
-L /usr/lib/x86_64-linux-gnu \
-L/usr/local/lib -lpthread \
-lcassandra \
-luuid
*/

#include "store.common/src/time.hpp"
#include "store.storage.cassandra/src/ca_resource_manager.hpp"

using namespace store::storage::cassandra;

namespace ioc = store::ioc;

auto main(int argc, char* argv[]) -> int {

  string environment = "development",
    keyspace = "dwh", store = "IKEA", dataType = "Sales", purpose = "forecast";

  auto client = make_shared<CassandraBaseClient>("127.0.0.1", "cassandra", "cassandra");
  client->tryConnect();

  // Register in main().  
  ioc::ServiceProvider->RegisterInstance<CassandraBaseClient>(client);

  auto caResourceManager = CaResourceManager(environment);

  auto fetchNextTasksHandler = caResourceManager.fetchNextTasks(keyspace, store, dataType, purpose);

  auto handler = [](shared_ptr<ca_resource_modified> caResourceModified) {
    string current = caResourceModified->current;
    string nextCurrent = fmt::format("Time: {}\n{}", store::common::getCurrentDate(), current);
    caResourceModified->current = nextCurrent;

    return caResourceModified;
  };

  fetchNextTasksHandler(handler);

  fprintf(stdout, "Staying put");

  std::thread t1([]{ while (true) std::this_thread::sleep_for(std::chrono::milliseconds(2000));});
  t1.join();

  return 0;
}
