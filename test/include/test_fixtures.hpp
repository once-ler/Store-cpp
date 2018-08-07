#pragma once

#include <set>
#include "json.hpp"
#include "store.models/src/models.hpp"
#include "store.models/src/interfaces.hpp"
#include "store.models/src/extensions.hpp"
#include "store.models/src/primitive.hpp"
#include "store.models/src/ioc/simple_container.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.storage/src/base_client.hpp"
#include "store.storage.pgsql/src/pg_client.hpp"

using namespace std;
using namespace store::models;
using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;
namespace Primitive = store::primitive;

namespace ioc = store::ioc;

using json = nlohmann::json;

namespace test {
  namespace fixtures {
    /*
    DROP TABLE TestEvent;

    CREATE TABLE TestEvent (
    id SERIAL PRIMARY KEY,
    timestamp TIMESTAMP default now(),
    actor JSONB,
    ip_address TEXT,
    key TEXT,
    process_id TEXT,
    session_id TEXT,
    connection_id INTEGER,
    aggregate_root TEXT,
    data JSONB NOT NULL
    );
    */
    struct TestEvent {
      int64_t id;
      int64_t  timestamp;
      json actor;
      Primitive::inet ip_address;
      string key;
      Primitive::uuid process_id;
      Primitive::uuid session_id;
      int connection_id;
      string aggregate_root;
      json data;
    };

    void to_json(json& j, const TestEvent& p) {
      j = json{
        { "id", p.id },
        { "timestamp", p.timestamp },
        { "actor", p.actor },
        { "ip_address", p.ip_address },
        { "key", p.key },
        { "process_id", p.process_id },
        { "session_id", p.session_id },
        { "connection_id", p.connection_id },
        { "aggregate_root", p.aggregate_root },
        { "data", p.data }
      };
    }

    void from_json(const json& j, TestEvent& p) {
      p.id = j.value("id", 0);
      p.timestamp = j.value("timestamp", 0);
      p.actor = j.value("actor", json(nullptr));
      p.ip_address = j.value("ip_address", "");
      p.key = j.value("key", "");
      p.process_id = j.value("process_uuid", "");
      p.session_id = j.value("session_id", "");
      p.connection_id = j.value("connection_id", 0);
      p.aggregate_root = j.value("aggregate_root", "");
      p.data = j.value("data", json(nullptr));
    }

    struct Droid : Model {
      Droid() = default;
      ~Droid() = default;
      string model;
      Droid(string id, string name, store::primitive::dateTime ts, string _model) : Model(id, name, ts), model(_model) {}
    };

    // Must be in type's namespace.
    void to_json(json& j, const Droid& p) {
      j = json{ { "id", p.id },{ "name", p.name },{ "ts", p.ts },{ "model", p.model } };
    }

    void from_json(const json& j, Droid& p) {
      p.id = j.value("id", "");
      p.name = j.value("name", "");
      p.ts = j.value("ts", "");
      p.model = j.value("model", "");
    }

    struct ParticipantExtended : public Participant {
      ParticipantExtended() = default;
      ~ParticipantExtended() = default;

      ParticipantExtended(string id, string name, store::primitive::dateTime ts, boost::any party) : Participant{ id, name, ts, party } {}
    };

    struct RebelAlliance : Affiliation<Participant> {
    
    };

    struct RogueOne : Affiliation<ParticipantExtended> {
    
    };

    struct Widget : Model {
      template<typename U>
      U ONE(U o) { return o; };
    };

    template<typename T>
    struct Widget2 : public Widget {
      T ONE(T o) override { return o; };
    };

    template<typename T>
    class MockClient : public BaseClient<T> {
    public:
      
      template<typename U>
      void test_type_name() {
        cout << resolve_type_to_string<U>(false) << endl;
      }

      template<typename U>
      vector<U> list(string version = "master", int offset = 0, int limit = 10, string sortKey = "id", string sortDirection = "Asc") {
        // Define the callback.
        auto f = this->List([](string version, int offset, int limit, string sortKey, string sortDirection) {
          auto sql = string_format("select current from %s.%s order by current->>'%s' %s offset %d limit %d", version.c_str(), resolve_type_to_string<U>().c_str(), sortKey.c_str(), sortDirection.c_str(), offset, limit);
          
          cout << "sql: " << sql << endl;

          vector<json> fake_json;
          vector<U> fake_pocos;
          vector<string> fake_parsed_response = {
            R"({ "id": "0" })", R"({ "id": "1" })", R"({ "id": "2" })", R"({ "id": "3" })", R"({ "id": "4" })", R"({ "id": "5" })", R"({ "id": "6" })", R"({ "id": "7" })", R"({ "id": "8" })", R"({ "id": "9" })"
          };
         
          for_each(fake_parsed_response.begin(), fake_parsed_response.end(), [&](const string& d) { fake_json.push_back(move(json::parse(d))); });

          for_each(fake_json.begin(), fake_json.end(), [&](const json& j) { U o = j; fake_pocos.push_back(move(o)); });
          
          return fake_pocos;
        });

        return f(version, offset, limit, sortKey, sortDirection);
      }

    };

  }

  template<typename T>
  bool AreEqual(const T& a, const T& b) {
    return &a == &b;
  }  
}
