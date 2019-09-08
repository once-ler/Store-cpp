#pragma once

#include <chrono>
#include <iomanip>
#include "json.hpp"
#include "jwt/jwt.hpp"
#include "store.models/src/extensions.hpp"
#include "store.common/src/file.hpp"

using namespace jwt::params;
using namespace store::extensions;

using json = nlohmann::json;

namespace store::common {
  struct Time {
    std::chrono::system_clock::time_point timePoint;
    std::time_t epochTime;
    std::string timeString;
  };

  struct RS256KeyPair {
    string privateKey;
    string publicKey;

    RS256KeyPair() {}
    explicit RS256KeyPair(const string& privateKey_, const string& publicKey_): privateKey(privateKey_), publicKey(publicKey_) {}

    RS256KeyPair(const RS256KeyPair& next)
      : privateKey(next.privateKey), publicKey(next.publicKey) {}

    RS256KeyPair& operator=(const RS256KeyPair& next) {
      if (this != &next) {
        privateKey = next.privateKey;
        publicKey = next.publicKey;
      }
      return *this;
    }

    RS256KeyPair(const RS256KeyPair&& next)
      : privateKey(""), publicKey("") {
        privateKey = next.privateKey;
        publicKey = next.publicKey;

        next.privateKey.empty();
        next.publicKey.empty();
      }

    RS256KeyPair& operator=(RS256KeyPair&& next){
      if (this != &next) {
        privateKey = next.privateKey;
        publicKey = next.publicKey;

        next.privateKey.empty();
        next.publicKey.empty();
      }
      return *this;
    }
  };

  auto getExpirationTime = [](int num_hr) {
    auto exp = std::chrono::system_clock::now() + std::chrono::hours{num_hr};
    std::time_t exp_time = std::chrono::system_clock::to_time_t(exp); 
    string ts = "";
    char str[100];
    if (std::strftime(str, sizeof(str), "%F %T %Z%z", std::localtime(&exp_time))) {
      ts.assign(str);
    }
    
    return Time{ exp, exp_time, ts };
  };

  auto createJwt = [](json& j) -> string {
    if (j["user"].is_null())
      return "";

    // Check whether we are use public key signing.  The field "private_key" will be provided by user.
    string alg = "rs256";
    string privateKey = j.value("private_key", "");

    if (privateKey.size() == 0) {
      alg = "hs256"; 
      privateKey = generate_uuid();
    }

    auto user = j["user"].get<string>();
    auto sid = "sess:" + generate_uuid();
    auto et = getExpirationTime(24);

    j["sid"] = sid;
    j["secret"] = privateKey;
    j["expire"] = static_cast<int64_t>(et.epochTime * 1000);
    j["expire_ts"] = et.timeString; 
    j["user_uuid"] = generate_uuid_v3(user.c_str());

    jwt::jwt_object obj{
      algorithm(alg), 
      payload({
        { "iss", "store::web::token" },
        { "user", user }
      }),
      secret(privateKey)      
    };

    obj.add_claim("exp", j["expire"].get<int64_t>());
    obj.add_claim("exp_ts", j["expire_ts"].get<string>());
    obj.header().add_header("sid", sid);

    j["signature"] = obj.signature();

    return j["signature"];
  };

  // header: x-access-token
  /* 
    @param
      key: Either the an actual random key phrase or a public RSA key (or public key from a certificate).
      enc_str: Encrypted or signed token.
  */
  auto decryptJwt = [](string key, string enc_str) -> pair<string, shared_ptr<jwt::jwt_object>> {
    string error = "";
    shared_ptr<jwt::jwt_object> dec = nullptr;

    try {
      auto obj = jwt::decode(enc_str, algorithms({"hs256", "rs256"}), secret(key));
      dec = make_shared<jwt::jwt_object>(obj);
    } catch (const jwt::TokenExpiredError& e) {
        //Handle Token expired exception here
        error = e.what();
      } catch (const jwt::SignatureFormatError& e) {
        //Handle invalid signature format error
        error = e.what();
      } catch (const jwt::DecodeError& e) {
        //Handle all kinds of other decode errors
        error = e.what();
      } catch (const jwt::VerificationError& e) {
        // Handle the base verification error.
        error = e.what();
      } catch (...) {
        error = "Caught unknown exception";
      }

    return make_pair(error, dec);

  };

  /*
    Use case: in main() of a server, load the private/public key into memory.
    Later, on routes that require authorization, decryption of the RS256 encrypted key can be done in memory.
  */
  auto tryGetRS256Key = [](const json& config_j, const string& privateKeyFieldName = "privateKeyFile", const string& publicKeyFieldName = "publicKeyFile") -> RS256KeyPair {
    RS256KeyPair rs256KeyPair;
    string privateKeyFile = config_j.value(privateKeyFieldName, "");
    string publicKeyFile = config_j.value(publicKeyFieldName, "");
    // If private key location was provided in the config, read it and include it.
    if (privateKeyFile.size() > 0 && publicKeyFile.size() > 0) {
      auto privateKey = getFile(privateKeyFile);
      auto publicKey = getFile(publicKeyFile);
      rs256KeyPair = std::move(RS256KeyPair(privateKey, publicKey));
    }
    return rs256KeyPair;
  };

  auto jwtObjectToJson = [](const jwt::jwt_object& obj) -> json {
    ostringstream oss;
    json j;

    oss << obj.header();
    string header = oss.str();
    auto header_j = json::parse(header);
    j["header"] = header_j;

    oss.str("");
    oss << obj.payload();
    string payload = oss.str();
    auto payload_j = json::parse(payload);
    j["payload"] = payload_j;

    return move(j);
  };

  auto tokenExpired = [](const json& j) -> bool {
    using namespace std::chrono;
    int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    int64_t exp = 0;
    auto payload = j.value("payload", json(nullptr));
    if (!payload.is_null() && !payload["exp"].is_null())
      exp = payload["exp"].get<int64_t>();

    return now > exp;
  };

  auto isRS256Authenticated = [](const string& publicKey, const string& token, json& j) -> bool {
    auto pa = decryptJwt(publicKey, token);
    if (pa.first.size() > 0) {
      return false;
    }

    j = jwtObjectToJson(*(pa.second));

    // Has the token expired?
    bool expired = tokenExpired(j);

    if (expired)
      return false;
    else
      return true;
  };

}
