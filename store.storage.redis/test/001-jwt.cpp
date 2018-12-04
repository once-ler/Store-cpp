/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../001-jwt.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lcpp_redis -ltacopie -lpthread -luuid -lcrypto
*/

#include "json.hpp"

#include "store.storage.redis/src/redis_client.hpp"
#include "store.common/src/web_token.hpp"

using RedisClient = store::storage::redis::Client;
using json = nlohmann::json;

using namespace store::common;

auto main(int argc, char *argv[]) -> int {
  auto client = RedisClient();

  json j{{"user", "Mickey.Mouse@disney.com"}};

  auto resp = createJwt(j);

  cout << resp << endl;

  return 0;
}
