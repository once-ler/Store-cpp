#pragma once
// https://github.com/mongodb/mongo-cxx-driver/blob/master/examples/mongocxx/instance_management.cpp

#include <mongocxx/instance.hpp>
#include <mongocxx/logger.hpp>
// #include <mongocxx/pool.hpp>
// #include <mongocxx/uri.hpp>

namespace store::storage::mongo {

  class mongo_access {
    public:
      static mongo_access& instance() {
          static mongo_access instance;
          return instance;
      }

      void configure(std::unique_ptr<mongocxx::instance> instance,
                    std::unique_ptr<mongocxx::pool> pool) {
          _instance = std::move(instance);
          _pool = std::move(pool);
      }

      using connection = mongocxx::pool::entry;

      connection get_connection() {
          return _pool->acquire();
      }

      bsoncxx::stdx::optional<connection> try_get_connection() {
          return _pool->try_acquire();
      }

    private:
      mongo_access() = default;

      std::unique_ptr<mongocxx::instance> _instance = nullptr;
      std::unique_ptr<mongocxx::pool> _pool = nullptr;
  };

  void configure(mongocxx::uri uri) {
    class noop_logger : public mongocxx::logger {
      public:
        virtual void operator()(mongocxx::log_level,
                                bsoncxx::stdx::string_view,
                                bsoncxx::stdx::string_view) noexcept {}
    };

    auto instance =
        bsoncxx::stdx::make_unique<mongocxx::instance>(bsoncxx::stdx::make_unique<noop_logger>());

    mongo_access::instance().configure(std::move(instance),
                                       bsoncxx::stdx::make_unique<mongocxx::pool>(std::move(uri)));
  }

}
