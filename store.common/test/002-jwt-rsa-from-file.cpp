/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../002-jwt-rsa-from-file.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -luuid -lcrypto
*/

#include <cassert>
#include <iomanip>
#include "json.hpp"
#include "jwt/jwt.hpp"
#include "../../store.common/src/file.hpp"
#include "../../store.common/src/web_token.hpp"

using json = nlohmann::json;
using namespace store::common;

json config_j = R"(
{
  "privateKeyFile": "resources/jwtRS256.sample.key",
  "publicKeyFile": "resources/jwtRS256.sample.key.pub"
}
)"_json;

const auto decryptWithPublicKey = [](const string& enc_str, const string& publicKey) -> pair<string, shared_ptr<jwt::jwt_object>> {
  return decryptJwt(publicKey, enc_str);
};

auto main(int argc, char *argv[]) -> int {
  json j{{"user", "mickeymouse"}};

  auto rs256KeyPair = tryGetRS256Key(config_j);
  j["private_key"] = rs256KeyPair.privateKey;

  auto enc_str = createJwt(j);
  auto pa = decryptWithPublicKey(enc_str, rs256KeyPair.publicKey);

  auto err = pa.first;
cout << err << endl;  
  assert (err.size() == 0);

  auto obj = *(pa.second);

  ostringstream oss;
  string header;
  oss << obj.header();
  header = oss.str();

  json header_j = json::parse(header);
  assert (header_j["typ"].get<string>() == string("JWT"));
  assert (header_j["alg"].get<string>() == string("RS256"));
  
  oss.str("");
  oss << obj.payload();
  string payload = oss.str();

  json payload_j = json::parse(payload);
  assert (payload_j["iss"].get<string>() == string("store::web::token"));
  assert (payload_j["user"].get<string>() == string("mickeymouse"));

  cout << header << endl;
  cout << payload << endl;

  return 0;
}