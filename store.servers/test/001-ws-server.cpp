/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../websocket ../001-ws-server.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L ../../../../websocket/libwebsocket/lib -lwebsocket -levent -lcrypto -lssl -lrt
*/

#include "store.servers/src/ws-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"

namespace store::servers {
  class EchoServer : public WebSocketServer {
    using WebSocketServer::WebSocketServer;
  public:
    EchoServer() : WebSocketServer() {
      this->frame_recv_cb_handler = [](void* arg) {
        user_t *user = (user_t*)arg;
        if (user->wscon->frame->payload_len > 0) {
          user->msg += string(user->wscon->frame->payload_data, user->wscon->frame->payload_len);
        }
        
        if (user->wscon->frame->fin == 1) {
          // Simply echo back to user.
          frame_buffer_t *fb = frame_buffer_new(1, 1, 
            user->wscon->frame->payload_len, 
            user->wscon->frame->payload_data
          );
          if (fb)
            send_a_frame(user->wscon, fb);
          
          user->msg = "";
        }
      };
    }

  };
}

auto main(int argc, char *argv[]) -> int {
  using namespace store::servers;

  EchoServer echoServer;
  echoServer.serv(3001);
  
  return 0;
}
