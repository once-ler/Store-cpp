#pragma once

#include <regex>
#include "store.common/src/web_token.hpp"
#include "store.servers/src/http-server.hpp"
#include "store.servers/src/session.hpp"
#include "store.servers/src/replies.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::common;
using namespace store::servers::util;

namespace ioc = store::ioc;

namespace store::servers {
  
  namespace secured::routes {

    using Route = std::pair<string, function<void((struct evhttp_request*, vector<string>, session_t&))>>;
  
    Route getSession = {
      "^/api/session$",
      [](struct evhttp_request* req, vector<string> segments = {}, session_t& sess){
      
        if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
          evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
          return;
        }
        
        json j = sess;
        
        store::servers::replyOK(req, j.dump(2), "application/json");
      }
    };

  }

  class RS256SecureServer : public HTTPServer {
    using HTTPServer::HTTPServer;
    
  public:
    RS256SecureServer() : HTTPServer() {
      using namespace secured::routes;

      rs256KeyPair = ioc::ServiceProvider->GetInstance<RS256KeyPair>();
      routes = { getSession };
    }

    map<string, function<void((struct evhttp_request*, vector<string>, session_t&))>> routes{};

    void ProcessRequest(struct evhttp_request* req) override {
      // Handle CORS preflight.
      if (evhttp_request_get_command(req) == EVHTTP_REQ_OPTIONS) {
        auto origin = evhttp_find_header(req->input_headers, "Origin");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Origin", origin != NULL ? origin : "null");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Vary", "origin");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Credentials", "true");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Headers", "Origin, Accept, Content-Type, x-access-token");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Max-Age", "86400");
        evhttp_send_reply(req, HTTP_OK, "OK", NULL);
        return;
      }

      json j;
      bool authenticated = isAuthenticated(req, j);

      if (!authenticated) {
        evhttp_send_error(req, 401, NULL);
        return;
      }
      
      // localhost:1718/a/b/c
      // uri -> /a/b/c

      // Pass the decrypted payload to expose fields such as "user" if needed.
      bool uri_matched = tryMatchUri(req, j);
      
      if (uri_matched)
        return;

      evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
    }

  private:
    shared_ptr<RS256KeyPair> rs256KeyPair;

    bool isAuthenticated(struct evhttp_request* req, json& j) {
      const char* val = evhttp_find_header(req->input_headers, "x-access-token");
      if (val != NULL)
        return isRS256Authenticated(rs256KeyPair->publicKey, val, j);

      // x-access-token not found in header.  Expect in querystring if jsonp.
      map<string, string> querystrings = tryGetQueryString(req);
      string xtoken = querystrings["x-access-token"];
      cout << xtoken << endl;
      if (xtoken.size() > 0)
        return isRS256Authenticated(rs256KeyPair->publicKey, xtoken, j);

      return false;
    }

    bool tryMatchUri(struct evhttp_request* req, json& j) {
      const char* uri = evhttp_request_uri(req);
      string uri_str = string{uri};
      bool uri_matched = false;
      session_t sess = j;

      for (const auto& a : routes) {
        std::smatch seg_match;
        vector<string> segments;
        if (std::regex_search(uri_str, seg_match, std::regex((a.first)))) {
          for (size_t i = 0; i < seg_match.size(); ++i)
            segments.emplace_back(seg_match[i]); 
          
          uri_matched = true;
          
          a.second(req, segments, sess);
          break;
        }        
      }

      return uri_matched;
    }
  };
}