#pragma once

#include <cpp_redis/cpp_redis>

using namespace std;

namespace store::storage::redis {

  class Client {
  public:
    Client(const Client& r) = default;
    Client(Client&& r) = default;
    Client& operator=(const Client& r) = default;
    Client& operator=(Client&& r) = default;

    Client(string host_ = "127.0.0.1", int port_ = 6379) {
      client = make_shared<cpp_redis::client>();

      client->connect(host_, port_, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
        if (status == cpp_redis::client::connect_state::dropped) {
          std::cerr << "client disconnected from " << host << ":" << port << std::endl;
        }
      });      
    }

    class RedisSessionStore {
    public:
      explicit RedisSessionStore(Client* session_) : session(session_) {}


    private:
      Client* session;
    };

    RedisSessionStore sessions{ this };

    /*
      The value stored per the session id is the key to the encrypted string.
    */
    void get(string key, function<void(string&&)> callback) {
      
      client->get(key, [&callback](cpp_redis::reply& reply) {
        if (reply.is_string()) {
          auto r = reply.as_string();
          callback(move(r));
        } else {
          callback("");
        }      
      });
      
      client->sync_commit();
    }

    template<typename A>
    void get(string key, A& context, function<void(A&, string&&)> callback) {
      
      client->get(key, [&](cpp_redis::reply& reply) {
        if (reply.is_string()) {
          auto r = reply.as_string();
          callback(context, move(r));
        } else {
          callback(context, "");
        }      
      });
      
      client->sync_commit();
    }

    void set(string key, string value) {      
      client->set(key, value);      
      client->sync_commit();
    }

  private:
    shared_ptr<cpp_redis::client> client;    
  };

}
