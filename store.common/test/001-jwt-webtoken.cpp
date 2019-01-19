/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../001-jwt-webtoken.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -luuid -lcrypto
*/

#include <cassert>
#include <iomanip>
#include "json.hpp"
#include "jwt/jwt.hpp"
#include "000-fixtures.hpp"
#include "../../store.common/src/web_token.hpp"

using namespace store::common;
using namespace jwt::params;

using json = nlohmann::json;

const auto signWithPrivateKey = []() -> string {
  auto key = test::jwt::privateKey;

  json j{
    {"private_key", key},
    {"user", "mickeymouse"}
  };

  auto enc_str = createJwt(j);

  return move(enc_str);
};

const auto decryptWithPublicKey = [](const string& enc_str) -> pair<string, shared_ptr<jwt::jwt_object>> {

  auto publicKey = test::jwt::publicKeyPem;

  return decryptJwt(publicKey, enc_str);
};

auto main(int argc, char *argv[]) -> int {
  auto token = signWithPrivateKey();

  auto pa = decryptWithPublicKey(token);

  auto err = pa.first;

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
