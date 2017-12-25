#include "json.hpp"
#include "mongo_base_client.hpp"
#include "mongo_client.hpp"
#include "models.hpp"
#include "extensions.hpp"

using namespace store::models;
using namespace store::extensions;
using json = nlohmann::json;

namespace test::mongo::fixtures {
  struct HL7EventMessage : Model {
    using Model::Model;
    string data;
    HL7EventMessage(const string& data_) : data(data_) {}
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

int spec_3() {
  using namespace test::mongo::fixtures;
  using mongoclient = store::storage::mongo::Client<IEvent>;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "eventstream");

  // Get the stream id that represents the Type./
  auto new_uuid = generate_uuid();
  auto uid = client.getUid("foo-a", new_uuid);

  HL7EventMessage hl7Message{ "MSH\r" };

  Event<HL7EventMessage> hl7Event;
  hl7Event.streamId = uid;
  hl7Event.type = "foo-a";
  hl7Event.data = hl7Message;

  client.events.Append<HL7EventMessage>(hl7Event);
  client.events.Save();
}

int main(int argc, char *argv[]) {
  return spec_3();
  return spec_2();
  return spec_1();
  return spec_0();
}
