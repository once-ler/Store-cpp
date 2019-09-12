#pragma once

/*
  Dependencies:
    https://github.com/caosiyang/websocket.git
*/

#include <functional>
#include "libwebsocket/include/websocket.h"
#include "libwebsocket/include/connection.h"
#include "store.servers/src/ws-user.hpp"
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include <sys/socket.h>
#include <netinet/in.h>
// #include <assert.h>
// #include <signal.h>
#include <iostream>
// #include <vector>
// #include <string>
// #include <fstream>
// #include <map>

namespace store::servers {

  class WebSocketServer {
  public:
    WebSocketServer() {}
    ~WebSocketServer() {}
    int serv(int port);
    function<void(user_t*, string)> onMessageReceived = [](user_t* user, string message){};
    
  protected:
    static void listencb(struct evconnlistener *listener, evutil_socket_t clisockfd, struct sockaddr *addr, int len, void *ptr);
    static void user_disconnect_cb(void *arg);
    static void user_disconnect(user_t *user);
    static void frame_recv_cb(void* arg);
    static void frame_write_cb(void* arg);
    static function<void(void* arg)> frame_recv_cb_handler;
    static function<void(void* arg)> frame_write_cb_handler;
    static function<void(user_t* user)> handshake_cb_handler;

    int bindSocket(int port);

    static vector<user_t*> user_vec;
    struct event_base *base = NULL;
    evconnlistener *listener = NULL;

  };

  vector<user_t*> WebSocketServer::user_vec = {};

  // Override the callback by redefining the handler in the inherited class.  See test/001-ws-server.cpp
  function<void(void* arg)> WebSocketServer::frame_recv_cb_handler = [](void* arg){};

  function<void(void* arg)> WebSocketServer::frame_write_cb_handler = [](void* arg){};

  // Override the callback by redefining the handler in the inherited class.
  function<void(user_t* user)> WebSocketServer::handshake_cb_handler = [](user_t* user){
    cout << "ws_req_str >> " << user->wscon->ws_req_str << endl;
    // GET /token?a=2 HTTP/1.1
    // TODO: parse this req str for path and querystrings.
    // authenticate the user with the JWT token.
    // If the JWT doesn't check out, use"
    // ws_serve_exit(user->wscon);
  };

  void WebSocketServer::frame_recv_cb(void *arg) {
    frame_recv_cb_handler(arg);
  }

  void WebSocketServer::frame_write_cb(void *arg) {
    frame_write_cb_handler(arg);
  }

  void WebSocketServer::user_disconnect(user_t *user) {
    if (user) {
      //update user list
      for (vector<user_t*>::iterator iter = user_vec.begin(); iter != user_vec.end(); ++iter) {
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
    user_disconnect(user);
  }

  void WebSocketServer::listencb(struct evconnlistener *listener, evutil_socket_t clisockfd, struct sockaddr *addr, int len, void *ptr) {
    struct event_base *eb = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(eb, clisockfd, BEV_OPT_CLOSE_ON_FREE);
    // LOG("a user logined in, socketfd = %d", bufferevent_getfd(bev));

    //create a user
    user_t *user = user_create();
    user->wscon->bev = bev;

    user->wscon->handshake_cb_unit.cbarg = user;
    user->wscon->handshake_cb_unit.cb = [](void* arg) {
      user_t* user = (user_t*) arg;
      handshake_cb_handler(user);
    };

    user_vec.push_back(user);
    ws_conn_setcb(user->wscon, FRAME_RECV, frame_recv_cb, user);
    ws_conn_setcb(user->wscon, WRITE, frame_write_cb, user);
    ws_conn_setcb(user->wscon, CLOSE, user_disconnect_cb, user);
    ws_serve_start(user->wscon);

  }

  int WebSocketServer::bindSocket(int port) {
    setbuf(stdout, NULL);
    base = event_base_new();
    
    struct sockaddr_in srvaddr;
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = INADDR_ANY;
    srvaddr.sin_port = htons(port);

    listener = evconnlistener_new_bind(base, WebSocketServer::listencb, NULL, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 500, (const struct sockaddr*)&srvaddr, sizeof(struct sockaddr));
  }

  int WebSocketServer::serv(int port) {
    bindSocket(port);

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);

    return 0;
  }

}
