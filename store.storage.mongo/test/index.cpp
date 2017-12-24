#include "mongo_base_client.hpp"

int spec_0() {
  using mongoclient = store::storage::mongo::BaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "foo");
  auto succeeded = client.insert("FOO_TYPE", R"({"firstName": "Foo"})");

  return succeeded;
}

int spec_1() {
  using mongoclient = store::storage::mongo::BaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "dump");
  
  auto nextval = client.getNextSequenceValue("foo-a");

  return 0;
}

int main(int argc, char *argv[]) {
  return spec_1();
  return spec_0();
}
