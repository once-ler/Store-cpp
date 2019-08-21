/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../003-jwt-rsa-from-server.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -luuid -lcrypto -levent
*/
#include <regex>
#include <chrono>
#include "store.common/src/web_token.hpp"
#include "store.servers/src/http-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace std::chrono;
using namespace store::common;

namespace ioc = store::ioc;

namespace store::servers {
  class SecureServer : public HTTPServer {
    using HTTPServer::HTTPServer;
    
  public:
    SecureServer() : HTTPServer() {
      rs256KeyPair = ioc::ServiceProvider->GetInstance<RS256KeyPair>();
    }

    map<string, function<void((struct evhttp_request*, vector<string>, json&))>> routes{};

    void ProcessRequest(struct evhttp_request* req) override {
      json j;
      bool authenticated = isAuthenticated(req, j);

      if (!authenticated) {
        evhttp_send_error(req, 401, NULL);
        return;
      }
      
      // localhost:1718/a/b/c
      // uri -> /a/b/c

      // Pass the decrypted payload to expose fields such as "user".
      bool uri_matched = tryMatchUri(req, j);
      
      if (uri_matched)
        return;

      evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
    }

  private:
    shared_ptr<RS256KeyPair> rs256KeyPair;

    bool isAuthenticated(struct evhttp_request* req, json& payload_j) {
      struct evkeyvalq* headers;
      struct evkeyval *header;

      headers = evhttp_request_get_input_headers(req);
      for (header = headers->tqh_first; header; header = header->next.tqe_next) {
        printf("  %s: %s\n", header->key, header->value);
        std::string key(header->key), val(header->value);

        if (key == "x-access-token") {
          auto pa = decryptJwt(rs256KeyPair->publicKey, val);
          if (pa.first.size() > 0)
            return false;

          /*
          {
            "header": {
              "alg": "HS256",
              "sid": "sess:9a198993-1fca-4624-9692-0f883539e339",
              "typ": "JWT"
            },
            "payload": {
              "exp": 1566425547000,
              "iss": "store::web::token",
              "user": "mickeymouse"
            }
          }
          */
          // Get the payload.
          ostringstream oss;
          auto obj = *(pa.second);
          oss << obj.payload();
          string payload = oss.str();
          payload_j = json::parse(payload);

          // Has the token expired?
          // double now = util::currentMilliseconds;
          const auto p0 = std::chrono::time_point<std::chrono::system_clock>{};
          int64_t now = duration_cast<std::chrono::milliseconds>(p0.time_since_epoch()).count();
          int64_t exp = payload_j.value("exp", 0);

          if (now > exp)
            return false;
          else
            return true;          
        }
      }

      return false;
    }

    bool tryMatchUri(struct evhttp_request* req, json& j) {
      const char* uri = evhttp_request_uri(req);
      string uri_str = string{uri};
      bool uri_matched = false;

      for (const auto& a : routes) {
        std::smatch seg_match;
        vector<string> segments;
        if (std::regex_search(uri_str, seg_match, std::regex((a.first)))) {
          for (size_t i = 0; i < seg_match.size(); ++i)
            segments.emplace_back(seg_match[i]); 
          
          uri_matched = true;
          
          a.second(req, segments, j);
          break;
        }        
      }

      return uri_matched;
    }
  };
}
