/*
g++ -std=c++14 -I ../../ -I /usr/local/include -I /usr/include/postgresql -I ../../../libpqmxx/include -I ../../../Store-cpp -I ../../../json/single_include/nlohmann ../003-dangerously-execute.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -luuid -llibpqmxx -lpq
*/

#include "store.storage.pgsql/src/pg_client.hpp"

using json = nlohmann::json;

namespace test::count {

  shared_ptr<db::postgres::Result> run(const string& sql) {
    json dbConfig = {{"applicationName", "store_pq"}};
    cout << dbConfig.dump(2) << endl;
    DBContext dbContext{ 
      dbConfig.value("applicationName", "store_pq"), 
      dbConfig.value("server", "127.0.0.1"),
      dbConfig.value("port", 5432), 
      dbConfig.value("database", "eventstore"),
      dbConfig.value("user", "streamer"),
      dbConfig.value("password", "streamer"),
      10 
    };

    pgsql::Client<IEvent> pgClient{ dbContext };
    
    return pgClient.dangerouslyExecute(sql);
  }
}

int main(int argc, char *argv[]) {
  
  auto r = test::count::run("");

}
