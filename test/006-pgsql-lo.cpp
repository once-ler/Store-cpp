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

#define DEBUG

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
        version varchar(80) not null,
        ts timestamp with time zone default current_timestamp,
        current oid,
        primary key(type, id, version)
      ) tablespace {1}
    )", store, tblspc);
    
    return pgClient->save(stmt);
  }
 

  template<typename A>
  string insertTable(shared_ptr<pgsql::Client<A>> pgClient, const string& type, const string& id, const string& version, Oid current) {
    auto stmt = fmt::format(R"(
      insert into "{0}"."objectx_lo" (type, id, version, current)
      values ('{1}', '{2}', '{3}', {4})
      on conflict (type, id, version) do update set current = excluded.current
    )", store, type, id, version, current);
    cout << stmt << endl;
    return pgClient->save(stmt);
  }
  

  template<typename A>
  Oid readTable(shared_ptr<pgsql::Client<A>> pgClient) {
    auto stmt = fmt::format(R"(
      select type, id, version, current::text from "{0}"."objectx_lo" where type = '{1}' and id = '{2}' order by ts desc limit 1)", store, type, id);

    Oid oid;
    string oidstr;
      
    auto cb = [&](auto& s) {
      for(const auto& e : s) {
        auto type = e.template as<string>(0);
        auto id = e.template as<string>(1);
        auto version = e.template as<string>(2);
        oidstr = e.template as<string>(3);
        cout << type << endl
          << id << endl
          << version << endl
          << oidstr << endl;
      }

      oid = std::stoul(oidstr);
      return oid;
    };

    cout << stmt << endl;
    pgClient->select(stmt)(cb);

    return oid;
  }

}

auto main(int argc, char* argv [] ) -> int {

  using namespace test::lo;

  auto client = getClient<IEvent>();
  auto resp = createTable<IEvent>(client);
  cout << resp << endl;
  
  // Upload large object.
  auto oid = client->loHandler.upload(fake_large_object);
  cout << "upload: " << oid << endl;

  // Assign large object to table.
  auto a = insertTable<IEvent>(client, type, id, fake_large_object_sha256, oid);
  cout << "insertTable: " << a << endl;

  // Read back from
  auto b = readTable<IEvent>(client);
  cout << "readTable: " << b << endl;

  // Download large object.
  auto data = client->loHandler.download(b);
  cout << "download:\n" << data << endl;

  // Delete large object.
  client->loHandler.deleteOid(33289);

  // Upload file.
  client->loHandler.uploadFile("wrappers.xml");

  return 0;
}
