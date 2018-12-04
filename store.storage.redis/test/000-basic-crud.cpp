/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include ../000-basic-crud.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lcpp_redis -ltacopie -lpthread -luuid
*/

#include "store.storage.redis/src/redis_client.hpp"

using RedisClient = store::storage::redis::Client;

struct Dummy {
  string response;
};

void simple_set(RedisClient& client) {
  client.set("foo", "buzz");
}

void simple_get(RedisClient& client) {
  auto d = Dummy{"before"};
  
  auto responseCallback = [&d](string&& s) {
    d.response = s;
    cout << d.response << endl;
  };

  // client.get("foo", responseCallback);

  client.get<Dummy>("foo", d, [](Dummy& du, string&& resp){
    cout << du.response << endl;
    du.response = resp;
    cout << du.response << endl;
  });
}

auto main(int argc, char *argv[]) -> int {
  auto client = RedisClient();

  simple_set(client);

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  simple_get(client);

  thread t([]{while(true) std::this_thread::sleep_for(std::chrono::milliseconds(500));});
  t.join();

  return 0;
}