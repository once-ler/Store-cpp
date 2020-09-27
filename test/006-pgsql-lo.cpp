/*
g++ -g3 -std=c++14 -I ../../ -I /usr/local/include \
  -I /usr/include/postgresql \
  -I ../../../libpqmxx/include \
  -I ../../../Store-cpp \
  -I ../../../connection-pool \
  -I ../../../json/single_include/nlohmann \
  -I ../../../spdlog/include \
  ../006-pgsql-lo.cpp -o bin/testing \
  -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib \
  -luuid -llibpqmxx -lpq

*/

#include "store.storage.pgsql/src/pg_client.hpp"
#include "spdlog/spdlog.h"
#include <cassert>

namespace test::lo {

  string fake_large_object = "I am supposed to be a very large object!",
    fake_large_object_sha256 = "7ab6e65584ceaa03edb59bfa4a8e3754576d7b98095effe2a9240f4da922470d",
    store = "crms", 
    tblspc = "pg_default", 
    type = "Billing", 
    id = "ABC123";

  template<typename A>
  shared_ptr<pgsql::Client<A>> getClient() {
    int poolSize = 1;
    json dbConfig = {{"applicationName", "store_pq"}};
    DBContext dbContext{ 
      dbConfig.value("applicationName", "store_pq"), 
      dbConfig.value("server", "127.0.0.1"),
      dbConfig.value("port", 5432), 
      dbConfig.value("database", "eventstore"),
      dbConfig.value("user", "streamer"),
      dbConfig.value("password", "streamer"),
      10 
    };

    auto pgClient = make_shared<pgsql::Client<A>>(dbContext, poolSize);
    pgClient->events.setDbSchema(store);

    return pgClient;
  }

  template<typename A>
  string createTable(shared_ptr<pgsql::Client<A>> pgClient) {
    auto stmt = fmt::format(R"(
      create table if not exists "{0}"."objectx_lo" (
        type varchar(80) not null,
        id varchar(120) not null,
        hash varchar(80) not null,
        ts timestamp with time zone default current_timestamp,
        current oid,
        primary key(type, id, hash)
      ) tablespace {1};
    )", store, tblspc);
    
    return pgClient->save(stmt);
  }

  template<typename A>
  std::function<string(shared_ptr<pgsql::Client<A>>)> insertTable(const string& type, const string& id, const string& hash, Oid current) {
    auto stmt = fmt::format(R"(
      insert into "{0}"."objectx_lo (type, id, hash, current)
      values ('{1}', '{2}', '{3}', {})
      on conflict (type, id, hash) do update set current = excluded.current;
    )", store, type, id, hash, current);

    return [stmt](shared_ptr<pgsql::Client<A>> pgClient) -> string {
      return pgClient->save(stmt);
    };
  }

  template<typename A>
  Oid readTable(shared_ptr<pgsql::Client<A>> pgClient) {
    auto stmt = fmt::format(R"(
      select type, id, hash, current from "{0}"."objectx_lo where type = '{1}' and id = '{2}';
    )", store, type, id);

    Oid oid;
      
    auto cb = [&oid](auto& s) {
      for(const auto& e : s) {
        auto type = e.template as<string>(0);
        auto id = e.template as<string>(1);
        auto hash = e.template as<string>(2);
        oid = e.template as<int>(3);
        cout << type << endl
          << id << endl
          << hash << endl
          << oid << endl;
      }
      return oid;
    };

    pgClient->select(stmt)(cb);

    return oid;
  }

}

auto main(int argc, char* argv [] ) -> int {

  using namespace test::lo;

  auto client = getClient<IEvent>();
  auto resp = createTable<IEvent>(client);
  cout << resp << endl;
  
  // Create a large object.
  auto oid = client->loHandler.upload(fake_large_object);
  cout << "upload: " << oid << endl;

  // Assign large object to table.
  auto a = insertTable<IEvent>(type, id, fake_large_object_sha256, oid)(client);
  cout << "insertTable: " << a << endl;

  // Read back from
  auto b = readTable<IEvent>(client);
  cout << "readTable: " << b << endl;

  return 0;
}
