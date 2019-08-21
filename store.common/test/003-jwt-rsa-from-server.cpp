/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann ../003-jwt-rsa-from-server.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -luuid -lcrypto -levent
*/
#include <regex>
#include <fstream>
#include "store.common/src/web_token.hpp"
#include "store.servers/src/rs256-secure-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::common;

namespace ioc = store::ioc;

namespace app::routes {
  using Route = std::pair<string, function<void((struct evhttp_request*, vector<string>, json&))>>;

  Route getSession = {
    "^/api/session$",
    [](struct evhttp_request* req, vector<string> segments = {}, json& j){
    
      if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
        return;
      }

      struct evbuffer *resp = evbuffer_new();
      evbuffer_add_printf(resp, "%s", j.dump(2).c_str());
      evhttp_send_reply(req, HTTP_OK, "OK", resp);
      evbuffer_free(resp);
    }
  };
}

auto main(int argc, char *argv[]) -> int {
  using namespace store::servers;
  using namespace app::routes;

  json config_j = json::parse(R"(
    {
      "port": 5555,
      "privateKeyFile": "resources/jwtRS256.sample.key",
      "publicKeyFile": "resources/jwtRS256.sample.key.pub"
    }
  )");
  auto rs256KeyPair = tryGetRS256Key(config_j);
  
  ioc::ServiceProvider->RegisterInstance<RS256KeyPair>(make_shared<RS256KeyPair>(rs256KeyPair));

  // Dump a token for testing.
  json j{{"user", "mickeymouse"}};
  j["private_key"] = rs256KeyPair.privateKey;
  auto enc_str = createJwt(j);
  std::ofstream ofs("test.token");
  ofs << "TOKEN=" << enc_str << "\n"
    << R"(curl -v -H "x-access-token: ${TOKEN}" http://localhost:5555/api/session)" << "\n";
  ofs.close();

  RS256SecureServer rs256SecureServer;
  rs256SecureServer.routes = { getSession };
  rs256SecureServer.serv(config_j["port"].get<int>(), 10);

  /*
  # Typical:
  TOKEN=$(curl -s -X POST -H 'Accept: application/json' -H 'Content-Type: application/json' --data '{"username":"{username}","password":"{password}","rememberMe":false}' https://{hostname}/api/authenticate | jq -r '.id_token')
  curl -H 'Accept: application/json' -H "Authorization: Bearer ${TOKEN}" https://{hostname}/api/myresource

  # This API:
  curl -H 'Accept: application/json' -H "x-access-token: ${TOKEN}" https://{hostname}/api/myresource
  */

   /*
    // j = jwtObjectToJson(*(pa.second));
    // pa.second will look like the following:
    
    {
      "header": {
        "alg": "RS256",
        "sid": "sess:76cd2732-a67a-438b-960b-72573de8b252",
        "typ": "JWT"
      },
      "payload": {
        "exp": 1566497092000,
        "exp_ts": "2019-08-22 14:04:52 EDT-0400",
        "iss": "store::web::token",
        "user": "mickeymouse"
      }
    }

  */
}
