/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../000-jwt-rsa.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -luuid -lcrypto
*/

#include <cassert>
#include <iomanip>
#include "json.hpp"
#include "jwt/jwt.hpp"
#include "000-fixtures.hpp"

using namespace jwt::params;

auto signWithPrivateKey = []() -> string {  
  auto key = test::jwt::privateKey;
  
  jwt::jwt_object obj{
      algorithm("rs256"), 
      payload({
        { "iss", "store::web::token" },
        { "user", "mickeymouse" }
      }),
      secret(key)      
    };

  int64_t exp = 1559347200000;
  obj.add_claim("exp", exp);

  obj.header().add_header("sid", "sess:abcd");

  std::error_code ec;

  string signedToken = obj.signature(ec);

  assert (!ec);

  return move(signedToken);
};

auto decryptWithPublicKey = [](const string& enc_str) -> pair<string, shared_ptr<jwt::jwt_object>> {
  string error = "";
  shared_ptr<jwt::jwt_object> dec = nullptr;

  auto publicKey = test::jwt::publicKeyPem;

  try {
    auto obj = jwt::decode(enc_str, algorithms({"hs256", "rs256"}), secret(publicKey));
    dec = make_shared<jwt::jwt_object>(obj);
  } catch (...) {
    error = "Caught unknown exception";
  }

  return make_pair(error, dec);
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

  assert (R"({"alg":"RS256","sid":"sess:abcd","typ":"JWT"})" == header);

  oss.str("");
  oss << obj.payload();
  string payload = oss.str();

  assert (R"({"exp":1559347200000,"iss":"store::web::token","user":"mickeymouse"})" == payload);

  cout << header << endl;
  cout << payload << endl;

  return 0;
}
