#pragma once

#include "json.hpp"
#include "pg_client.hpp"
#include "extensions.hpp"
#include "primitive.hpp"

using namespace std;
using namespace store::extensions;
namespace Primitive = store::primitive;
using namespace store::storage::pgsql;

using json = nlohmann::json;

namespace test {
  namespace events {
    namespace fixtures {
      struct EventStream {
        Primitive::uuid id;
        string type;
        int version;
        int64_t timestamp;
        json snapshot;
        int snapshotVersion;
      };

      struct Event {
        Primitive::uuid id;
        Primitive::uuid streamId;
        int version;
        json data;
        string type;
        int64_t timestamp;
      };
    }
    int test_events() {
      DBContext dbContext{ "store_pq", "127.0.0.1", 5432, "pccrms", "editor", "editor", 10 };
      pgsql::Client<fixtures::Event> pgClient{ dbContext };
      auto new_uuid = generate_uuid();
      // TODO: Need EventSource prototype.
      fixtures::Event e{ new_uuid, "06606420-0bc4-4661-8f4a-9a04ef7caea1", -1, json(nullptr).dump(), "FOO", -1 };
      pgClient.insertOne<fixtures::Event>("master", { "id", "name", "current" }, "7", "r2d2", serializeToJsonb(e));
    }
  }
}
