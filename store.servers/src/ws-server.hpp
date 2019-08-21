#pragma once

/*
  Dependencies:
    https://github.com/caosiyang/websocket.git
*/

#include "libwebsocket/include/websocket.h"
#include "libwebsocket/include/connection.h"
#include "store.servers/src/ws-user.hpp"
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>

namespace store::servers {

  class WebSocketServer {
  public:
    WebSocketServer() {}
    ~WebSocketServer() {}
    int serv(int port, int nthreads);
  protected:
    void listencb(struct evconnlistener *listener, evutil_socket_t clisockfd, struct sockaddr *addr, int len, void *ptr);
    void user_disconnect(user_t *user);
    void user_disconnect_cb(void *arg);
    virtual void frame_recv_cb(void *arg);

    int bindSocket(int port);

    vector<user_t*> user_vec;
    struct event_base *base = NULL;
    evconnlistener *listener = NULL;

  };

  void WebSocketServer::frame_recv_cb(void *arg) {
    user_t *user = (user_t*)arg;
    if (user->wscon->frame->payload_len > 0) {
      user->msg += string(user->wscon->frame->payload_data, user->wscon->frame->payload_len);
    }
    
    if (user->wscon->frame->fin == 1) {
      
      user->msg = "";
    }
  }

  void WebSocketServer::user_disconnect(user_t *user) {
    if (user) {
      //update user list
      for (vector<user_t*>::iterator iter = this->user_vec.begin(); iter != user_vec.end(); ++iter) {
        if (*iter == user) {
          user_vec.erase(iter);
          break;
        }
      }
      user_destroy(user);
    }
  }

  void WebSocketServer::user_disconnect_cb(void *arg) {
    user_t* user = (user_t*)arg;
    this->user_disconnect(user);
  }

  void WebSocketServer::listencb(struct evconnlistener *listener, evutil_socket_t clisockfd, struct sockaddr *addr, int len, void *ptr) {
    struct event_base *eb = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(eb, clisockfd, BEV_OPT_CLOSE_ON_FREE);
    LOG("a user logined in, socketfd = %d", bufferevent_getfd(bev));

    //create a user
    user_t *user = user_create();
    user->wscon->bev = bev;
    user_vec.push_back(user);
    ws_conn_setcb(user->wscon, FRAME_RECV, this->frame_recv_cb, user);
    ws_conn_setcb(user->wscon, CLOSE, this->user_disconnect_cb, user);

    ws_serve_start(user->wscon);
  }

  int WebSocketServer::bindSocket(int port) {
    setbuf(stdout, NULL);
    base = event_base_new();
    assert(base);

    struct sockaddr_in srvaddr;
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = INADDR_ANY;
    srvaddr.sin_port = htons(10086);

    listener = evconnlistener_new_bind(base, this->listencb, NULL, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 500, (const struct sockaddr*)&srvaddr, sizeof(struct sockaddr));
    
  }

}
