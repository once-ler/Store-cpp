#pragma once

#include <iostream>
#include <map>
#include <evhttp.h>
#include <ctime>

namespace store::servers::util {
  // Can be called from <namespace>::routes.
  std::string getRequestBody(struct evhttp_request *req) {
    struct evbuffer* buf = NULL;
    size_t len = 0;
    char* data = NULL;

    // get the event buffer containing POST body
    buf = evhttp_request_get_input_buffer(req);

    // get the length of POST body
    len = evbuffer_get_length(buf);

    if (len == 0) {
      evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
      return "";
    }

    // create a char array to extract POST body
    data = (char*)malloc(len + 1);
    data[len] = 0;

    // copy POST body into your char array
    evbuffer_copyout(buf, data, len);

    std::string content(data);

    free(data);
    
    return content;
  }

  auto tryGetQueryString = [](struct evhttp_request* req) {
    struct evkeyvalq* headers = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq));
    evhttp_parse_query(evhttp_request_get_uri(req), headers);
    struct evkeyval* kv = headers->tqh_first;

    std::map<std::string, std::string> querystrings;
    while (kv) {
      querystrings.insert({kv->key, kv->value});
      kv = kv->next.tqe_next;
    }
    free(headers);
    return querystrings;
  };

  double currentMilliseconds() {
    std::time_t rawtime;
    std::tm* timeinfo;

    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);
    std::time_t timeSinceEpoch = mktime(timeinfo);

    return 1000 * timeSinceEpoch;
  }

  const std::string currentDate(bool with_time = false) {
    time_t now = time(0);
    struct tm tstruct;
    char buf[21];
    tstruct = *localtime(&now);
    const char* format = with_time ? "%F %T" : "%F";   
    strftime(buf, sizeof(buf), format, &tstruct);

    return buf;
  }
}
