#pragma once

#include <regex>
#include "store.common/src/web_token.hpp"
#include "store.servers/src/http-server.hpp"
#include "store.servers/src/session.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::common;

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

        evhttp_add_header(evhttp_request_get_output_headers(req),
		      "Content-Type", "application/json");
          
        struct evbuffer *resp = evbuffer_new();
        json j = sess;
        evbuffer_add_printf(resp, "%s", j.dump(2).c_str());
        evhttp_send_reply(req, HTTP_OK, "OK", resp);
        evbuffer_free(resp);
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
      if (evhttp_request_get_command(req) != EVHTTP_REQ_OPTIONS) {
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Origin", "*");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        evhttp_add_header (evhttp_request_get_output_headers(req),
          "Access-Control-Allow-Headers", "x-access-token, Content-Type");
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
      struct evkeyvalq* headers;
      struct evkeyval *header;

      headers = evhttp_request_get_input_headers(req);
      for (header = headers->tqh_first; header; header = header->next.tqe_next) {
        // printf("  %s: %s\n", header->key, header->value);
        std::string key(header->key), val(header->value);

        if (key == "x-access-token") {
          return isRS256Authenticated(rs256KeyPair->publicKey, val, j);       
        }
      }

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