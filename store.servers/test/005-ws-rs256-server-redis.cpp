/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../websocket -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann  ../005-ws-rs256-server-redis.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L ../../../../websocket/libwebsocket/lib -lwebsocket -levent -lcrypto -lssl -lrt -luuid -lcpp_redis -ltacopie -lpthread
*/
#define DEBUG

#include "store.servers/src/rs256-secure-redis-ws-server.hpp"

using namespace std;

auto getSomeRandomString = [](int range) -> string {
  string ret;
  ret.resize(range + 1);

  for (int i = 0; i < range; i++) {
    int rd = 97 + (rand() % (int)(122-97+1));
    char ltr = (char)rd;
    // oss << ltr;
    ret.at(i) = ltr;
  }
  ret.at(range) = '\0';
  return ret;
};

auto publishIndefinitely = [](const string& fixture_sid) -> void {
  cout << "Publishing to: " << fixture_sid << endl;

  cpp_redis::client redis_client;

  redis_client.connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
    if (status == cpp_redis::client::connect_state::dropped) {
      std::cout << "client disconnected from " << host << ":" << port << std::endl;
    }
  });

  while (1) {
    redis_client.publish(fixture_sid, getSomeRandomString(16));
    
    redis_client.sync_commit();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }  
};

auto main(int argc, char* argv[]) -> int {
  using namespace store::servers;

  json config_j = json::parse(R"(
    {
      "port": 5555,
      "privateKeyFile": "resources/jwtRS256.sample.key",
      "publicKeyFile": "resources/jwtRS256.sample.key.pub"
    }
  )");

  // Register RS256 keys.
  auto rs256KeyPair = tryGetRS256Key(config_j);  
  ioc::ServiceProvider->RegisterInstance<RS256KeyPair>(make_shared<RS256KeyPair>(rs256KeyPair));

  int port = config_j["port"].get<int>();

  // Register subscriber.
  auto sub = make_shared<cpp_redis::subscriber>();
  try {
    sub->connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::subscriber::connect_state status) {
      if (status == cpp_redis::subscriber::connect_state::dropped) {
        std::cout << "client disconnected from " << host << ":" << port << std::endl;
      }
    });
  } catch (const cpp_redis::redis_error& ex) {
    cout << ex.what() << endl;
    return 1;
  }
  ioc::ServiceProvider->RegisterInstanceWithKey<cpp_redis::subscriber>(REDIS_SUBSCRIPTION_KEY, sub);

  // Dump a token for testing.
  json j{{"user", "mickeymouse"}};
  j["private_key"] = rs256KeyPair.privateKey;
  auto enc_str = createJwt(j);
  std::ofstream ofs("test.ws.token");
  ofs << "TOKEN=" << enc_str << "\n"
    << R"(websocat ws://localhost:5555/?x-access-token=${TOKEN})" << "\n";
  ofs.close();

  // Create a infinite publisher for this session.
  auto pa = decryptJwt(rs256KeyPair.publicKey, enc_str);
  auto j1 = jwtObjectToJson(*(pa.second));
  string sid = j1["header"].value("sid", "");
  thread t1(publishIndefinitely, sid);

  RS256SecureRedisWsServer rs256RedisWsServer;
  cout << "Starting RS256RedisWsServer Server...\n";
  rs256RedisWsServer.serv(port);
}