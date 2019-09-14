/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../006-rs256-server-publish.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -luuid -lcrypto -levent -lcpp_redis -ltacopie
*/
#include <regex>
#include <fstream>
#include "store.common/src/web_token.hpp"
#include "store.servers/src/rs256-secure-redis-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::common;

namespace ioc = store::ioc;

auto createRedisClient = []() {
  auto redis_client = make_shared<cpp_redis::client>();
  redis_client->connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
    cout << "redis connection status: " << static_cast<std::underlying_type_t<cpp_redis::client::connect_state>>(status) << endl; 

    if (status != cpp_redis::client::connect_state::ok) {
      std::cerr << "redis client disconnected from " << host << ":" << port << std::endl;
    } else {
      std::cout << "redis client connected from " << host << ":" << port << std::endl;
    }
  });

  ioc::ServiceProvider->RegisterInstance<cpp_redis::client>(redis_client);

  return redis_client;
};

auto main(int argc, char *argv[]) -> int {
  using namespace store::servers;
  
  json config_j = json::parse(R"(
    {
      "port": 5555,
      "privateKeyFile": "resources/jwtRS256.sample.key",
      "publicKeyFile": "resources/jwtRS256.sample.key.pub"
    }
  )");
  auto rs256KeyPair = tryGetRS256Key(config_j);
  
  ioc::ServiceProvider->RegisterInstance<RS256KeyPair>(make_shared<RS256KeyPair>(rs256KeyPair));

  createRedisClient();

  RS256SecureRedisServer rs256SecureRedisServer;

  thread t1(
    [&rs256SecureRedisServer]() -> void { 

      while (1) {  
        rs256SecureRedisServer.publish("sess:", "HELLO");
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });

  t1.join();

  return 0;
}
