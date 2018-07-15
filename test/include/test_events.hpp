#pragma once

#include "json.hpp"
#include "pg_client.hpp"
#include "extensions.hpp"
#include "primitive.hpp"
#include "event.hpp"
#include "models.hpp"
#include "time.hpp"

using namespace std;
using namespace store::common;
using namespace store::models;
using namespace store::extensions;
using namespace store::events;
using namespace store::storage::pgsql;

namespace Primitive = store::primitive;

using json = nlohmann::json;

namespace test {
  namespace events {
    namespace fixtures {
      struct Droid : Model {
        using Model::Model;
      };

      void to_json(json& j, const Droid& p) {
        j = json{ { "id", p.id },{ "name", p.name },{ "ts", p.ts } };
      }

      void from_json(const json& j, Droid& p) {
        p.id = j.value("id", "");
        p.name = j.value("name", "");
        p.ts = j.value("ts", "");
      }

      struct Human : Model {
        using Model::Model;
      };

      void to_json(json& j, const Human& p) {
        j = json{ { "id", p.id },{ "name", p.name },{ "ts", p.ts } };
      }

      void from_json(const json& j, Human& p) {
        p.id = j.value("id", "");
        p.name = j.value("name", "");
        p.ts = j.value("ts", "");
      }
    }
    int test_events() {      
      using namespace test::events::fixtures;

      json dbConfig = {
        {"applicationName", "store_pq_test"},
        {"server", "127.0.0.1"},
        {"port", 5432},
        {"database", "eventstore"},
        {"user", "streamer"},
        {"password", "streamer"}
      };

      Droid r2{ "1", "r2d2", getCurrentTimeString(true) };
      Human han{ "2", "han solo", getCurrentTimeString(true) };
      
      enum class StreamType { Droid, Human };

      // Should be looked up from database.
      map<StreamType, string> streamMap = {
        { StreamType::Droid, "06606420-0bc4-4661-8f4a-9a04ef7caea1"},
        { StreamType::Human, "c455c4fe-f1d2-473d-905b-0aa5671a95e2" }
      };
      
      Event<Droid> droidEvent;
      droidEvent.streamId = streamMap[StreamType::Droid];
      droidEvent.type = "DROID";
      droidEvent.data = r2;
      
      Event<Human> humanEvent;
      humanEvent.streamId = streamMap[StreamType::Human];
      humanEvent.type = "HUMAN";
      humanEvent.data = han;

      DBContext dbContext{ 
        dbConfig.value("applicationName", "store_pq"), 
        dbConfig.value("server", "127.0.0.1"),
        dbConfig.value("port", 5432), 
        dbConfig.value("database", "eventstore"),
        dbConfig.value("user", "streamer"),
        dbConfig.value("password", "streamer"),
        10 
      };

      // Eventstore schema will be "dwh".
      pgsql::Client<IEvent> pgClient{ dbContext };
      
      pgClient.events.setDbSchema("dwh");
      pgClient.events.Append<Droid>(droidEvent);
      pgClient.events.Append<Human>(humanEvent);

      pgClient.events.Save();

      auto new_uuid = generate_uuid();
      // TODO: Need EventSource prototype.
      // fixtures::Event e{ new_uuid, "06606420-0bc4-4661-8f4a-9a04ef7caea1", -1, json(nullptr).dump(), "FOO", -1 };
      // pgClient.insertOne<fixtures::Event>("master", { "id", "name", "current" }, "7", "r2d2", serializeToJsonb(e));
      return 0;
    }
  }
}
