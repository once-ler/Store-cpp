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
#include "test_events.hpp"
#include "test_logger.hpp"
#include "test_fixtures.hpp"

using namespace std;
using namespace store::models;
using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;
namespace Primitive = store::primitive;

namespace ioc = store::ioc;

using json = nlohmann::json;

int main() {
  {
    test::logger::test_splog_spec();
    return 0;
  }
  {
    test::events::test_events();
    return 0;
  }

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

    const auto& v = pgClient.list<Droid>("master", 0, 10, "id", "Asc");
    for (const auto& o : v) {
      cout << o.id << " " << o.name << endl;
    }
    
    pgClient.insertOne<TestEvent>("public", { "actor", "ip_address", "key", "process_id", "session_id", "connection_id", "aggregate_root", "data" }, serializeToJsonb(d), "127.0.0.1", "DROID", "1a6ee016-7a7a-437e-8458-8fed3676d45f", "f959cf99-96a2-4645-ba41-3717da12f3aa", 1, "REBEL_ALLIANCE", serializeToJsonb(d));
        
  }
}
