/*
g++ -std=c++14 -I ../../ -I /usr/local/include -I /usr/include/postgresql -I ../../../libpqmxx/include -I ../../../Store-cpp -I ../../../json/single_include/nlohmann ../004-pgsql-xml.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -luuid -llibpqmxx -lpq
*/

#include "store.storage.pgsql/src/pg_client.hpp"
#include <cassert>

using json = nlohmann::json;

namespace test::xml {
  struct WsiX {
    string id;
    string current;
  };

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

  string insert() {
    pgsql::Client<IEvent> pgClient{ dbContext };
    
    return pgClient.insertOne<WsiX>("crms", {"id", "current"}, "1", "<foo name=\"buzz\"><bar value=\"biz\"></bar></foo>");
  }

  void read() {
    pgsql::Client<IEvent> pgClient{ dbContext };
    
    auto callback = [](auto& r) {
      for(auto& e : r) {
        cout << e.template as<string>(0) << ", " << e.template as<string>(1) << endl;
      }
    };

    pgClient.select("select id, current from crms.wsix where id = $1", "1")(callback);

  }

}

auto main(int argc, char *argv[]) -> int {
  
  auto r = test::xml::insert();

  cout << r << endl;

  // assert(r == 1);

  test::xml::read();

  return 0;
}
