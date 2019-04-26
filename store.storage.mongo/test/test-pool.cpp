/*
g++ -std=c++14 -Wall -I ../../../../Store-cpp -I ../../../../spdlog/include -I ../../../../json/single_include/nlohmann -I /usr/local/include/bsoncxx/v_noabi -I /usr/local/include/mongocxx/v_noabi -o testing ../test-pool.cpp -L/usr/local/lib -lpthread -lmongocxx -lbsoncxx -luuid -lboost_thread -lboost_regex -lboost_system -lboost_filesystem
*/

#include <cassert>

// #include <mongocxx/instance.hpp>
// #include <mongocxx/pool.hpp>

#include <mongocxx/uri.hpp>

#include "store.storage.mongo/src/mongo_instance.hpp"
#include "store.storage.mongo/src/mongo_base_client.hpp"
#include "store.storage.mongo/src/mongo_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"

struct Foo {};

int spec_1() {
  using mongoclient = store::storage::mongo::MongoClient<Foo>;
  mongoclient client("test", "bar");
  auto succeeded = client.insert("BAR_TYPE", R"({"firstName": "Bar"})");

  return succeeded;
}

int spec_0() {
  using mongoclient = store::storage::mongo::MongoBaseClient;
  mongoclient client("test", "foo");
  auto succeeded = client.insert("FOO_TYPE", R"({"firstName": "Foo"})");

  return succeeded;
}

int main1(int argc, char *argv[]) {

  mongocxx::instance inst{};
  mongocxx::uri uri{"mongodb://localhost:27017/?minPoolSize=3&maxPoolSize=3"};

  mongocxx::pool px{uri};
  
  int a = spec_0();

  int b = spec_1();
  
  cout << a << endl;

  cout << b << endl;
  
  return 0;
}

int main(int argc, char *argv[]) {
  namespace ioc = store::ioc;

  ioc::ServiceProvider->RegisterSingletonClass<store::storage::mongo::MongoInstance>();

  auto uri = mongocxx::uri{(argc >= 2) ? argv[1] : mongocxx::uri::k_default_uri};
    
  store::storage::mongo::configure(std::move(uri));

  auto p1 = ioc::ServiceProvider->GetInstance<store::storage::mongo::MongoInstance>();

  auto p2 = ioc::ServiceProvider->GetInstance<store::storage::mongo::MongoInstance>();
  
  // Should be the same pointer.
  assert (p1 == p2);
  
  int a = spec_0();

  int b = spec_1();

  cout << a << endl;

  cout << b << endl;
  
  return 0;
}
