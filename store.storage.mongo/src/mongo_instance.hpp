#pragma once
// https://github.com/mongodb/mongo-cxx-driver/blob/master/examples/mongocxx/instance_management.cpp

#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/logger.hpp>
#include "store.models/src/ioc/service_provider.hpp"
#include "store.storage.mongo/src/mongo_logger.hpp"

namespace store::storage::mongo {

  class MongoInstance {
    public:
      MongoInstance() = default;

      void configure(std::unique_ptr<mongocxx::instance> instance,
                    std::unique_ptr<mongocxx::pool> pool) {
          _instance = std::move(instance);
          _pool = std::move(pool);
      }

      using connection = mongocxx::pool::entry;

      connection getConnection() {
        return _pool->acquire();
      }

      bsoncxx::stdx::optional<connection> tryGetConnection() {
        return _pool->try_acquire();
      }

    private:
      std::unique_ptr<mongocxx::instance> _instance = nullptr;
      std::unique_ptr<mongocxx::pool> _pool = nullptr;
  };

  void configure(mongocxx::uri uri, bool persistLogging = true) {
    auto instance = persistLogging ?
        bsoncxx::stdx::make_unique<mongocxx::instance>(bsoncxx::stdx::make_unique<MongoLogger>())
        :
        bsoncxx::stdx::make_unique<mongocxx::instance>(bsoncxx::stdx::make_unique<noop_logger>());

    ioc::ServiceProvider->GetInstance<MongoInstance>()->configure(std::move(instance),
      bsoncxx::stdx::make_unique<mongocxx::pool>(std::move(uri)));

  }

}
