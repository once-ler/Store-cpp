#pragma once

#include <string>
#include <evhttp.h>

using namespace std;

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

}
