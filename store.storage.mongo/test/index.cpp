#include "mongo_base_client.hpp"

int spec_0() {
  using mongoclient = store::storage::mongodb::BaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "foo");
  auto succeeded = client.insert("FOO_TYPE", R"({"firstName": "Foo"})");

  return succeeded;
}

int main(int argc, char *argv[]) {

  return spec_0();
}
