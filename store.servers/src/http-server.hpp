#pragma once

#include <event.h>
#include <evhttp.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h> // usleep
#include <iostream>
#include <string>
#include <ctime>
#include "store.servers/src/util.hpp"

namespace store::servers {

  class HTTPServer {
  public:
    HTTPServer() {}
    ~HTTPServer() {}
    int serv(int port, int nthreads);
  protected:
    static void* Dispatch(void *arg);
    static void GenericHandler(struct evhttp_request *req, void *arg);
    virtual void ProcessRequest(struct evhttp_request *request);
    int BindSocket(int port);
  };

  int HTTPServer::BindSocket(int port) {
    int r;
    int nfd;
    nfd = socket(AF_INET, SOCK_STREAM, 0);
    if (nfd < 0) return -1;

    int one = 1;
    r = setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    r = bind(nfd, (struct sockaddr*)&addr, sizeof(addr));
    if (r < 0) return -1;
    r = listen(nfd, 10240);
    if (r < 0) return -1;

    int flags;
    if ((flags = fcntl(nfd, F_GETFL, 0)) < 0
        || fcntl(nfd, F_SETFL, flags | O_NONBLOCK) < 0)
      return -1;

    return nfd;
  }

  int HTTPServer::serv(int port, int nthreads) {
    int r;
    int nfd = BindSocket(port);
    if (nfd < 0) return -1;
    pthread_t ths[nthreads];
    for (int i = 0; i < nthreads; i++) {
      struct event_base *base = event_init();
      if (base == NULL) return -1;
      struct evhttp *httpd = evhttp_new(base);
      if (httpd == NULL) return -1;
      r = evhttp_accept_socket(httpd, nfd);
      if (r != 0) return -1;
      evhttp_set_gencb(httpd, HTTPServer::GenericHandler, this);
      evhttp_set_allowed_methods(httpd, EVHTTP_REQ_GET|EVHTTP_REQ_POST|EVHTTP_REQ_HEAD|EVHTTP_REQ_PUT|EVHTTP_REQ_DELETE|EVHTTP_REQ_OPTIONS);
      r = pthread_create(&ths[i], NULL, HTTPServer::Dispatch, base);
      if (r != 0) return -1;
    }
    for (int i = 0; i < nthreads; i++) {
      pthread_join(ths[i], NULL);
    }
  }

  void* HTTPServer::Dispatch(void *arg) {
    event_base_dispatch((struct event_base*)arg);
    return NULL;
  }

  void HTTPServer::GenericHandler(struct evhttp_request *req, void *arg) {
    ((HTTPServer*)arg)->ProcessRequest(req);
  }

  void HTTPServer::ProcessRequest(struct evhttp_request *req) {
    
    usleep(1);
    struct evbuffer *buf = evbuffer_new();
    if (buf == NULL) return;
    evbuffer_add_printf(buf, "Requested: %s\n", evhttp_request_uri(req));
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    if (buf)
		  evbuffer_free(buf);
  }

}
