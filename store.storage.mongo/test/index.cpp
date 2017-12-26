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
  };

  void to_json(json& j, const HL7EventMessage& p) {
    j = json{ { "id", p.id },{ "data", p.data } };
  }

  void from_json(const json& j, HL7EventMessage& p) {
    p.id = j.value("id", "");
    p.data = j.value("data", "");
  }
}

int spec_0() {
  using mongoclient = store::storage::mongo::MongoBaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "foo");
  auto succeeded = client.insert("FOO_TYPE", R"({"firstName": "Foo"})");

  return succeeded;
}

int spec_1() {
  using mongoclient = store::storage::mongo::MongoBaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "dump");
  
  auto nextval = client.getNextSequenceValue("foo-a");

  return 0;
}

int spec_2() {
  using mongoclient = store::storage::mongo::MongoBaseClient;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "eventstream");

  auto new_uuid = generate_uuid();
  auto uid = client.getUid("foo-a", new_uuid);

  return 0;
}

int spec_3() {
  using namespace test::mongo::fixtures;
  using mongoclient = store::storage::mongo::MongoClient<IEvent>;
  mongoclient clientStreams("mongodb://127.0.0.1:27017", "test", "eventstreams");
  mongoclient clientEvents("mongodb://127.0.0.1:27017", "test", "events");  

  // Get the stream id that represents the Type.
  auto new_uuid = generate_uuid();
  auto uid = clientStreams.getUid("foo-a", new_uuid);

  // Get the global events counter.
  auto globalseq = clientEvents.getNextSequenceValue("all_events");

  // Get version id that represents the lastest Type.
  auto nextseq = clientEvents.getNextSequenceValue("foo-a");

  HL7EventMessage hl7Message;
  hl7Message.data = "MSH\r";

  Event<HL7EventMessage> hl7Event;
  hl7Event.seqId = globalseq;
  hl7Event.id = generate_uuid();
  hl7Event.streamId = uid;
  hl7Event.version = nextseq;
  hl7Event.type = "foo-a";
  hl7Event.data = hl7Message;
  // timestamp = << "start_time" << bsoncxx::types::b_date(std::chrono::system_clock::now())

  clientStreams.events.Append<HL7EventMessage>(hl7Event);
  clientStreams.events.Save();

  return 0;
}

int spec_4() {
  using namespace test::mongo::fixtures;
  using mongoclient = store::storage::mongo::MongoClient<HL7EventMessage>;
  mongoclient client("mongodb://127.0.0.1:27017", "test", "hl7x");

  HL7EventMessage hl7Message;
  hl7Message.data = "MSH\r";

  json j = hl7Message;

  client.events.SaveOne("foo-a", j);
}

int main(int argc, char *argv[]) {
  return spec_4();
  return spec_3();
  return spec_2();
  return spec_1();
  return spec_0();
}
