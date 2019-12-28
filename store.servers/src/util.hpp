#pragma once

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <evhttp.h>
#include <ctime>
#include <regex>

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

  std::map<std::string, std::string> fileTypes{
    { "txt", "text/plain" },
	  { "js", "application/javascript" },
    { "json", "application/json" },
    { "xml", "text/xml" },
    { "html", "text/html" },
	  { "htm", "text/htm" },
	  { "css", "text/css" },
	  { "gif", "image/gif" },
	  { "jpg", "image/jpeg" },
	  { "jpeg", "image/jpeg" },
	  { "png", "image/png" },
    { "svg", "image/svg+xml" },
	  { "pdf", "application/pdf" }
  };

  auto guessFileType = [](const std::string& path) -> std::string {
    std::smatch seg_match;
    if (std::regex_search(path, seg_match, std::regex(".*(\\..+)$"))) {
      auto ext = seg_match[1];
      auto m = ext.str();
      m.erase(0, 1);
      
      if (fileTypes.find(m) != fileTypes.end())
        return fileTypes[m];

      return "application/octet-stream";
    }
    
    return "application/octet-stream";
  };

  auto fetchAsset = [](struct evhttp_request* req, const std::string& path) -> void {
    int fd = -1;
    struct stat st;
    
    if ((fd = open(path.c_str(), O_RDONLY)) < 0) {
      evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
      return;
    }

    if (fstat(fd, &st) == -1) {
      /* Make sure the length still matches, now that we
      * opened the file :/ */
      evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
      
      if (fd >= 0)
		    close(fd);

      return;
    }

    struct evbuffer* resp = evbuffer_new();
    
    evhttp_add_header(evhttp_request_get_output_headers(req),
        "Content-Type", guessFileType(path).c_str());
    
    evbuffer_add_file(resp, fd, 0, st.st_size);
    
    evhttp_send_reply(req, 200, "OK", resp);
    evbuffer_free(resp);
    // fd will be freed by evbuffer_chain_free
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
