#pragma once

#include <regex>
#include <evhttp.h>
#include "store.common/src/web_token.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.servers/src/ws-server.hpp"

using namespace std;

using namespace store::common;
namespace ioc = store::ioc;

namespace store::servers {
  class RS256SecureWsServer : public WebSocketServer {
    using WebSocketServer::WebSocketServer;
  public:
    RS256SecureWsServer() : WebSocketServer() {
      rs256KeyPair = ioc::ServiceProvider->GetInstance<RS256KeyPair>();
      
      // Override handshake callback.
      this->handshake_cb_handler = [this](user_t* user) {
        std::string req = user->wscon->ws_req_str;
        parseWebRequestLine(req, user);

        bool tokenIsValid = false;
        json j;
        parseQuerystringForAuthentication(user->req.path, j, tokenIsValid);

        if (!tokenIsValid || !j["header"].is_object() || !j["payload"].is_object())
          return;

        user->sess.sid = j["header"].value("sid", "");
        user->sess.user = j["payload"].value("user", "");
        user->sess.exp_ts = j["payload"].value("exp_ts", "");
        user->sess.is_valid = true;

        #ifdef DEBUG
        cout << user->sess.sid << " " << user->sess.user << " " << user->sess.exp_ts << endl;
        #endif
      };
    }

  private:
    shared_ptr<RS256KeyPair> rs256KeyPair;

    bool isAuthenticated(const string& val, json& j) {
      auto pa = decryptJwt(rs256KeyPair->publicKey, val);
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
    }

    void parseQuerystringForAuthentication(const std::string& path, json& j, bool& tokenIsValid) {
      struct evkeyvalq* headers = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq));
      int rc = evhttp_parse_query(path.c_str(), headers);
      struct evkeyval* kv = headers->tqh_first;
      while (kv) {

        std::string key(kv->key);
        std::string val(kv->value);
        
        if (key == "x-access-token") {
          tokenIsValid = isAuthenticated(val, j);
          break;
        }        
        kv = kv->next.tqe_next;
      }

      free(headers);
    }

    void parseWebRequestLine(const std::string& req, user_t* user) {
      if(!req.size())
        return;

      string buf = "";
      vector<string> v;
      size_t i = 0;
      char delim = ' ';
      while (i < req.size()) {
        if (req[i] != delim)
          buf += req[i];
        else {
          v.push_back(buf);
          buf = "";
        }
        i++;
      }

      for (size_t i = 0; i < v.size(); i++) {
        switch (i) {
          case 0:
            user->req.method = v.at(i);
            break;
          case 1:
            user->req.path = v.at(i);
            break;
          case 2:
            user->req.http_version = v.at(i);
            break;
          default:
            break;
        }
      }

    }
  };
}