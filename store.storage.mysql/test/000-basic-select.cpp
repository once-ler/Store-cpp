/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../amy/include -I /usr/local/include/boost -I /usr/local/include ../000-basic-select.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/lib -L /usr/local/lib -lmysqlclient -lpthread -lboost_system
*/

#include "store.storage.mysql/src/my_client.hpp"

using MyClient = store::storage::mysql::MyBaseClient;

void simple_get(MyClient& client) {
  client.connectAsync(R"(
    SELECT character_set_name, maxlen
    FROM information_schema.character_sets
    WHERE character_set_name LIKE 'latin%'";
  )")
}

auto main(int argc, char *argv[]) -> int {
  auto client = MyClient("127.0.0.1", 3306, "test", "admin", "12345678");

  simple_get(client);

  thread t([]{while(true) std::this_thread::sleep_for(std::chrono::milliseconds(500));});
  t.join();

  return 0;
}
