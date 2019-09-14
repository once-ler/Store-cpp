/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../json/single_include/nlohmann ../003-pubsub.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lcpp_redis -ltacopie -lpthread -luuid
*/

#include "store.models/src/extensions.hpp"
#include <cpp_redis/cpp_redis>

using namespace store::extensions;

auto getSomeRandomString = [](int range) -> string {
  string ret;
  ret.resize(range + 1);

  for (int i = 0; i < range; i++) {
    int rd = 97 + (rand() % (int)(122-97+1));
    char ltr = (char)rd;
    ret.at(i) = ltr;
  }
  ret.at(range) = '\0';
  return ret;
};

auto publishIndefinitely = []() -> void {
  cpp_redis::client redis_client;

  redis_client.connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
    if (status == cpp_redis::client::connect_state::dropped) {
      std::cout << "client disconnected from " << host << ":" << port << std::endl;
    }
  });

  while (1) {
    redis_client.publish("sess:" + generate_uuid(), getSomeRandomString(16));
    
    redis_client.sync_commit();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }  
};

auto main(int argc, char *argv[]) -> int {
  thread t1(publishIndefinitely);

  // cpp_redis::subscriber sub;
  // sub.connect();

  t1.join();

  return 0;
}
