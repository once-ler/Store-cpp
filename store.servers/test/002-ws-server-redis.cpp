/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../websocket -I ../../../../json/single_include/nlohmann  ../002-ws-server-redis.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L ../../../../websocket/libwebsocket/lib -lwebsocket -levent -lcrypto -lssl -lrt  -lcpp_redis -ltacopie -lpthread -luuid
*/

#include <stdlib.h>
#include <thread>
#include <chrono> 
#include <cpp_redis/cpp_redis>
#include "store.servers/src/ws-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.models/src/extensions.hpp"

namespace ioc = store::ioc;
using namespace store::extensions;

#define REDIS_SUBSCRIPTION_KEY "sess:*"

namespace store::servers {
  class EchoServer : public WebSocketServer {
    using WebSocketServer::WebSocketServer;
  public:
    EchoServer() : WebSocketServer() {
      redis_client = ioc::ServiceProvider->GetInstanceWithKey<cpp_redis::subscriber>(REDIS_SUBSCRIPTION_KEY);

      this->subscribe(REDIS_SUBSCRIPTION_KEY);

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
      frame_buffer_t *fb = frame_buffer_new(1, 1, msg.size(), msg.c_str());
      
      for (int32_t i = 0; i < user_vec.size(); ++i)
				send_a_frame(user_vec[i]->wscon, fb);
    }

    void subscribe(const string& topic) {
      redis_client->psubscribe(topic, [this](const std::string& chan, const std::string& msg) {
        std::cout << "MESSAGE " << chan << ": " << msg << std::endl;
        broadcast(msg);
      });

      redis_client->commit();
    }

  };
}

auto getSomeRandomString = [](int range) -> string {
  string ret;
  ret.resize(range + 1);

  // std::ostringstream oss;

  for (int i = 0; i < range; i++) {
    int rd = 97 + (rand() % (int)(122-97+1));
    char ltr = (char)rd;
    // oss << ltr;
    ret.at(i) = ltr;
  }
  ret.at(range) = '\0';
  return ret;
};

auto publishIndefinitely = []() -> void {
  cpp_redis::client redis_client;

  redis_client.connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
    if (status == cpp_redis::client::connect_state::dropped) {
      std::cout << "client disconnected from " << host << ":" << port << std::endl;
    }
  });

  while (1) {
    // "sess:" + generate_uuid()
    // redis_client.publish("session", getSomeRandomString(16));
    redis_client.publish("sess:" + generate_uuid(), getSomeRandomString(16));
    
    redis_client.sync_commit();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }  
};

auto main(int argc, char* argv[]) -> int {
  using namespace store::servers;

  // Register subscriber.
  auto sub = make_shared<cpp_redis::subscriber>();
  sub->connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::subscriber::connect_state status) {
    if (status == cpp_redis::subscriber::connect_state::dropped) {
      std::cout << "client disconnected from " << host << ":" << port << std::endl;
    }
  });

  ioc::ServiceProvider->RegisterInstanceWithKey<cpp_redis::subscriber>(REDIS_SUBSCRIPTION_KEY, sub);

  // Publish some periodically.
  thread t1(publishIndefinitely);

  EchoServer echoServer;
  cout << "Starting Echo Server...";
  echoServer.serv(3001);

  return 0;
}
