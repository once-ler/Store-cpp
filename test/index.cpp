#include <set>
#include "json.hpp"
#include "models.hpp"
#include "interfaces.hpp"
#include "extensions.hpp"
#include "primitive.hpp"
#include "ioc/simple_container.hpp"
#include "ioc/service_provider.hpp"
#include "base_client.hpp"
#include "pg_client.hpp"
#include "test_events.hpp"

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
    DROP TABLE events;

    CREATE TABLE events (
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
    struct Events {
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

    void to_json(json& j, const Events& p) {
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

    void from_json(const json& j, Events& p) {
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

template<typename F, typename ... Args>
void aFunction(F f, const Args... args) {
  f(args...);
}

int main() {
  using namespace test::fixtures;

  Droid c3po, k2so;
  
  Participant p0;
  p0.id = "1234";
  p0.name = "c3po";
  p0.party = c3po;

  vector<Participant> goodRoster = { p0 };
  
  RebelAlliance rebels;

  rebels.id = "rebels";
  rebels.roster = goodRoster;

  History<Droid> h0;
  h0.id = "0";
  h0.source = c3po;

  Record<Droid> r0;
  r0.current = c3po;
  r0.history = { h0 };

  RogueOne rogue1;
  
  ParticipantExtended p1 = { string("5678"), string("k2so"), store::primitive::dateTime(""), k2so };
  
  vector<ParticipantExtended> goodRoster2 = { p1 };

  rogue1.roster = goodRoster2;

  // Should be the same object.
  {
    ioc::SimpleContainer container1;
    container1.RegisterSingletonClass<RogueOne>();

    auto a = container1.GetInstance<RogueOne>();

    auto b = container1.GetInstance<RogueOne>();

    auto same = test::AreEqual(*(a.get()), *(b.get()));
    
    cout << same << endl;

    auto sameShared = test::AreEqual(*a, *b);

    cout << sameShared << endl;
  }

  // Should not be the same object.
  {
    ioc::SimpleContainer container1;
    container1.register_class<RogueOne>();

    auto a = container1.GetInstance<RogueOne>();

    auto b = container1.GetInstance<RogueOne>();

    auto same = test::AreEqual(*(a.get()), *(b.get()));

    cout << same << endl;

    auto sameShared = test::AreEqual(*a, *b);

    cout << sameShared << endl;
  }

  // Use singleton provider.
  {
    ioc::ServiceProvider->RegisterSingletonClass<RogueOne>();

    auto a = ioc::ServiceProvider->GetInstance<RogueOne>();

    auto b = ioc::ServiceProvider->GetInstance<RogueOne>();

    auto sameShared = test::AreEqual(*a, *b);

    cout << sameShared << endl;    
  }

  {
    MockClient<Widget> mockClient;
    mockClient.test_type_name<RogueOne>();
    auto v = mockClient.list<Droid>();

    // Should be 10.
    cout << "# of Droids: " << v.size() << endl;
  }

  // Should also work using IOC.
  {
    using namespace test;

    ioc::ServiceProvider->RegisterSingletonClass<MockClient<RogueOne>>();

    auto mockClient = ioc::ServiceProvider->GetInstance<MockClient<RogueOne>>();

    mockClient->test_type_name<RogueOne>();
    auto v = mockClient->list<Droid>();
  }
  
  {
    using namespace store::storage::pgsql;
    
    // Struct
    DBContext dbContext{ "store_pq", "127.0.0.1", 5432, "pccrms", "editor", "editor", 10 };
    pgsql::Client<RogueOne> pgClient{ dbContext };
    Droid d{ "4", "c3po", "", "old" };
    pgClient.save<Droid>("master", d);

    // Arbitrary
    string sql = string_format(R"SQL(
      insert into %s.%s (id, name,ts,current)
      values ('%s', '%s', now(), '%s')
      on conflict (id) do update set ts = now(), current = EXCLUDED.current, history = EXCLUDED.history
    )SQL", "master", "droid", "5", "k2so", R"({ "id": "5", "name": "k2so", "ts": "" })");

    pgClient.save(sql);

    // Params
    // (version, { Fields... }, Params...)    
    pgClient.insertOne<Droid>("master", { "id", "name", "current" }, "7", "r2d2", serializeToJsonb(d));

    auto& v = pgClient.list<Droid>("master", 0, 10, "id", "Asc");
    for (const auto& o : v) {
      cout << o.id << " " << o.name << endl;
    }
    
    pgClient.insertOne<Events>("public", { "actor", "ip_address", "key", "process_id", "session_id", "connection_id", "aggregate_root", "data" }, serializeToJsonb(d), "127.0.0.1", "DROID", "1a6ee016-7a7a-437e-8458-8fed3676d45f", "f959cf99-96a2-4645-ba41-3717da12f3aa", 1, "REBEL_ALLIANCE", serializeToJsonb(d));
        
  }
}
