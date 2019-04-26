/*
g++ -std=c++14 -Wall -I ../../../ -I ../../../../json/single_include/nlohmann -I /usr/local/include/bsoncxx/v_noabi -I /usr/local/include/mongocxx/v_noabi -o testing ../test-pool.cpp -L/usr/local/lib -lpthread -lmongocxx -lbsoncxx -luuid
*/

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

#include "store.storage.mongo/src/mongo_instance.hpp"
#include "store.storage.mongo/src/mongo_base_client.hpp"
#include "store.storage.mongo/src/mongo_client.hpp"

struct Foo {};

int spec_1(mongocxx::pool* px) {
  using mongoclient = store::storage::mongo::MongoClient<Foo>;
  mongoclient client(px, "test", "bar");
  auto succeeded = client.insert("BAR_TYPE", R"({"firstName": "Bar"})");

  return succeeded;
}

int spec_0(mongocxx::pool* px) {
  using mongoclient = store::storage::mongo::MongoBaseClient;
  mongoclient client(px, "test", "foo");
  auto succeeded = client.insert("FOO_TYPE", R"({"firstName": "Foo"})");

  return succeeded;
}

int main1(int argc, char *argv[]) {

  mongocxx::instance inst{};
  mongocxx::uri uri{"mongodb://localhost:27017/?minPoolSize=3&maxPoolSize=3"};

  mongocxx::pool px{uri};
  
  int a = spec_0(&px);

  int b = spec_1(&px);
  
  cout << a << endl;

  cout << b << endl;
  
  return 0;
}