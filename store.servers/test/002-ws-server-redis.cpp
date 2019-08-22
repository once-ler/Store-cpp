/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../websocket ../001-ws-server-redis.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L ../../../../websocket/libwebsocket/lib -lwebsocket -levent -lcrypto -lssl -lrt  -lcpp_redis -ltacopie -lpthread
*/

#include <cpp_redis/cpp_redis>
#include "store.servers/src/ws-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"

namespace ioc = store::ioc;

#define REDIS_SUBSCRIPTION_KEY "api:feedback"

namespace store::servers {
  class EchoServer : public WebSocketServer {
    using WebSocketServer::WebSocketServer;
  public:
    EchoServer() : WebSocketServer() {
      redis_client = ioc::ServiceProvider->GetInstanceWithKey<cpp_redis::subscriber>(REDIS_SUBSCRIPTION_KEY);

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

  private:
    string database;
    shared_ptr<cpp_redis::subscriber> redis_client;

    void broadcast(const string& msg) {

    }

    void subscribe(const string& topic) {
      redis_client->subscribe(REDIS_SUBSCRIPTION_KEY, [this](const std::string& chan, const std::string& msg) {
        // std::cout << "MESSAGE " << chan << ": " << msg << std::endl;
        broadcast(msg);
      });
    }

  };
}

auto main(int argc, char* argv[]) -> int {
  using namespace store::servers;

  // Register subscriber.
  cpp_redis::subscriber sub;
  sub.connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::subscriber::connect_state status) {
    if (status == cpp_redis::subscriber::connect_state::dropped) {
      std::cout << "client disconnected from " << host << ":" << port << std::endl;
    }
  });

  ioc::ServiceProvider->RegisterInstanceWithKey<cpp_redis::subscriber>(REDIS_SUBSCRIPTION_KEY, make_shared<cpp_redis::subscriber>(sub));

  EchoServer echoServer;
  echoServer.serv(3001);

  return 0;
}
