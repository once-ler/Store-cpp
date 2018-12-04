/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../001-jwt.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lcpp_redis -ltacopie -lpthread -luuid -lcrypto
*/

#include "json.hpp"

#include "store.storage.redis/src/redis_client.hpp"
#include "store.common/src/web_token.hpp"

using RedisClient = store::storage::redis::Client;
using json = nlohmann::json;

using namespace store::common;

void save_secret(RedisClient& client, string& sid, int ttl, string& secret) {
  cout << "Session id: " << sid << endl;
  cout << "Session will expire in " << ttl << " ms" << endl;
  client.set(sid, ttl, secret);
}

struct SomeType { string header; string payload; };

void retrieve_token(RedisClient& client, SomeType& d, string& sid, string& enc_str) {

  client.get<SomeType>(sid, d, [&enc_str](SomeType& du, string&& secret){
    if (secret.size() == 0) return;

    cout << "Decrypted token -----" << endl;

    auto pa = decryptJwt(secret, enc_str);

    if (pa.first.size() > 0) {
      cerr << pa.first << endl;
      return;
    }

    auto obj = *(pa.second);

    ostringstream oss;

    oss << obj.header();

    du.header = oss.str();
    oss.str("");
    oss << obj.payload();
    du.payload = oss.str(); 

    cout << du.header << endl;
    cout << du.payload << endl;
  });

}

void expectError(string& enc_str) {
  auto pa = decryptJwt("Ooops!", enc_str);

  if (pa.first.size() > 0) {
    cerr << "Error reported: " << pa.first << endl;
    return;
  }
}

void spec_0() {
  auto client = RedisClient();

  json j{{"user", "Mickey.Mouse@disney.com"}};

  auto enc_str = createJwt(j);

  cout << "Expecting error with no key" << endl;

  expectError(enc_str);

  cout << "Encrypted token -----" << endl;
  cout << enc_str << endl;

  string sid = j["sid"];
  string secret = j["secret"];

  cout << "Token set to expire on " << j["expire_ts"] << endl; 

  save_secret(client, sid, 5000, secret);

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  SomeType a{"some", "thing"};
  retrieve_token(client, a, sid, enc_str);
}

void spec_1() {
  json j{{"user", "Mickey.Mouse@disney.com"}};

  auto client = RedisClient();

  client.sessions.set(j);
  string sid = j["sid"];
  string enc_str = j["signature"];

  cout << "Encrypted\n" << enc_str << endl;

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  client.sessions.get(sid, enc_str, [](pair<string, shared_ptr<jwt::jwt_object>>& pa){
    if (pa.first.size() > 0) {
      cerr << pa.first << endl;
      return;
    }

    auto obj = *(pa.second);

    cout << obj.header() << endl;
    cout << obj.payload() << endl;
    
  });

  // Expect error
  client.sessions.get("i-have-no-idea", enc_str, [](pair<string, shared_ptr<jwt::jwt_object>>& pa){
    if (pa.first.size() > 0) {
      cerr << "Expected erorr: " << pa.first << endl;
      return;
    }
  });
}

auto main(int argc, char *argv[]) -> int {
  
  // spec_0();

  spec_1();

  thread t([]{while(true) std::this_thread::sleep_for(std::chrono::milliseconds(500));});
  t.join();

  return 0;
}
