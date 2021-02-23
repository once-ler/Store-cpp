/*
g++ -g3 -std=c++14 -I ../../ -I /usr/local/include \
  -I /usr/include/postgresql \
  -I ../../../libpqmxx/include \
  -I ../../../Store-cpp \
  -I ../../../connection-pool \
  -I ../../../json/single_include/nlohmann \
  -I ../../../spdlog/include \
  ../007-pgsql-err-handling.cpp -o bin/testing \
  -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib \
  -luuid -llibpqmxx -lpq

*/

#define DEBUG

#include "store.storage.pgsql/src/pg_client.hpp"
#include "spdlog/spdlog.h"
#include <cassert>

using namespace store::storage::pgsql;

namespace test::err::handling {
  string store = "crms";

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

}

auto main(int argc, char* argv []) -> int {
  using namespace test::err::handling;
  

  // Make sure postgres is up.
  // Start the client.
  auto client = getClient<IEvent>();

  auto errType = make_shared<ErrorType>();
  
  // Should be no errors.
  client->save("select current_timestamp", errType);
  assert(*errType == ErrorType::none); 

  // Should be execution error.
  client->save("select current_timestamp_bad", errType);
  assert(*errType == ErrorType::execution);

  // Wait for user to hit return.
  // Shutdown postgres.
  do {
    cout << '\n' << "Please shutdown local instance of postgres.\nPress a key to continue...";
  } while (cin.get() != '\n'); 

  // Should be connection error.
  client->save("select current_timestamp", errType);
  assert(*errType == ErrorType::connection);

  return 0;
}

/*
Expected result:

Creating PostgreSQL connections...
PostgreSQL connection pool count: 1
Testing PostgreSQL connection pool with key PN5store7storage5pgsql6ClientINS_6events6IEventEEE
2021-02-23 01:24:24.973
ERROR:  column "current_timestamp_bad" does not exist
LINE 1: select current_timestamp_bad
               ^


Please shutdown local instance of postgres.
Press a key to continue...
server closed the connection unexpectedly
	This probably means the server terminated abnormally
	before or while processing the request.

*/
