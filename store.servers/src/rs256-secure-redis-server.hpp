#pragma once

#include <cpp_redis/cpp_redis>
#include "store.servers/src/rs256-secure-server.hpp"
#include "store.models/src/ioc/service_provider.hpp"

using namespace store::common;

namespace ioc = store::ioc;

namespace store::servers {

  class RS256SecureRedisServer : public RS256SecureServer {
    using RS256SecureServer::RS256SecureServer;
    
  public:
    RS256SecureRedisServer() : RS256SecureServer() {
      redis_client = ioc::ServiceProvider->GetInstance<cpp_redis::client>();
    }

    void publish(const string& session_id, const string& message) {
      redis_client->publish(session_id, message);    
      redis_client->sync_commit();
    }
    
  protected:
    shared_ptr<cpp_redis::client> redis_client;
  };

}
