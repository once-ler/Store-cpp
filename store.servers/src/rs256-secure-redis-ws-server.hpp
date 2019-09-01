#pragma once

#include <cpp_redis/cpp_redis>
#include "store.models/src/ioc/service_provider.hpp"
#include "store.servers/src/rs256-secure-ws-server.hpp"

using namespace std;

namespace ioc = store::ioc;

#define REDIS_SUBSCRIPTION_KEY "sess:*"

namespace store::servers {
  class RS256SecureRedisWsServer : public RS256SecureWsServer {
    using RS256SecureWsServer::RS256SecureWsServer;
  public:
    RS256SecureRedisWsServer() : RS256SecureWsServer() {
      redis_client = ioc::ServiceProvider->GetInstanceWithKey<cpp_redis::subscriber>(REDIS_SUBSCRIPTION_KEY);

      this->subscribe(REDIS_SUBSCRIPTION_KEY);
    }

  private:
    shared_ptr<cpp_redis::subscriber> redis_client;

    void broadcast(const string& chan, const string& msg) {
      frame_buffer_t *fb = frame_buffer_new(1, 1, msg.size(), msg.c_str());
      
      for (int32_t i = 0; i < user_vec.size(); ++i) {
				if (user_vec[i]->sess.sid == chan)
          send_a_frame(user_vec[i]->wscon, fb);
      }
    }

    void subscribe(const string& topic) {
      redis_client->psubscribe(topic, [this](const std::string& chan, const std::string& msg) {
        broadcast(chan, msg);
      });

      redis_client->commit();
    }

  };
}
