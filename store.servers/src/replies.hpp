#pragma once

#include "store.servers/src/util.hpp"

using namespace std;

using namespace store::servers::util;

namespace store::servers {
  
  auto replyOK = [](struct evhttp_request* req, const string& content, const string& contentType = "text/plain") {
    struct evbuffer* resp = evbuffer_new();
    evbuffer_add_printf(resp, "%s", content.c_str());
    // For CORS
    auto origin = evhttp_find_header(req->input_headers, "Origin");
    evhttp_add_header (evhttp_request_get_output_headers(req),
        "Access-Control-Allow-Origin", origin != NULL ? origin : "null");
    
    evhttp_add_header (evhttp_request_get_output_headers(req),
      "Content-Type", contentType.c_str());
    evhttp_send_reply(req, HTTP_OK, "OK", resp);
    evbuffer_free(resp);
  };

  auto respondAfterProcessed = [](struct evhttp_request* req, const string& body) {
    // Handle jsonp requests.
    map<string, string> querystrings = tryGetQueryString(req);

    string jsonp = querystrings["jsonp"];

    if (jsonp.size() > 0) {
      replyOK(req, jsonp + "(" + body + ")", "application/javascript");
      return;
    } 

    replyOK(req, body, "application/json");
  };

}
