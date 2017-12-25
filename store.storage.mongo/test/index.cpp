#include "json.hpp"
#include "mongo_base_client.hpp"
#include "models.hpp"
#include "extensions.hpp"

using namespace store::models;
using namespace store::extensions;
using json = nlohmann::json;

namespace test::mongo::fixtures {
  struct HL7EventMessage : Model {
    using Model::Model;
    string data;
  };

  void to_json(json& j, const HL7EventMessage& p) {
    j = json{ { "id", p.id },{ "data", p.name } };
  }

  void from_json(const json& j, HL7EventMessage& p) {
    p.id = j.value("id", "");
    p.data = j.value("data", "");
  }
}

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

int spec_2() {
  using mongoclient = store::storage::mongo::BaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "eventstream");

  auto new_uuid = generate_uuid();
  auto uid = client.getUid("foo-a", new_uuid);

  return 0;
}

int main(int argc, char *argv[]) {
  return spec_2();
  return spec_1();
  return spec_0();
}
