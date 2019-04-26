#pragma once

#include <bsoncxx/stdx/string_view.hpp>
#include <mongocxx/logger.hpp>

#include "store.common/src/spdlogger.hpp"

/*
  enum class log_level {
    k_error,
    k_critical,
    k_warning,
    k_message,
    k_info,
    k_debug,
    k_trace,
  };
*/
namespace store::storage::mongo {
  class MongoLogger : public mongocxx::logger {
  public:
    MongoLogger() : mongocxx::logger() {
      logger_ = make_unique<store::common::SpdLogger>("mongo_client_driver");
    }

    void operator()(mongocxx::log_level lvl, bsoncxx::stdx::string_view dom, bsoncxx::stdx::string_view msg) noexcept override {
      switch (static_cast<int>(lvl)) {
        case 0:
        case 1:        
        logger_->error("DOMAIN: {} MESSAGE: {}", dom.data(), msg.data());
        break;

        case 2:
        logger_->warn("DOMAIN: {} MESSAGE: {}", dom.data(), msg.data());
        break;

        case 3:
        case 4:
        logger_->info("DOMAIN: {} MESSAGE: {}", dom.data(), msg.data());
        break;

        case 5:
        case 6:
        logger_->debug("DOMAIN: {} MESSAGE: {}", dom.data(), msg.data());
        break;

        default:
        break; 
      }
    }

  private:
    unique_ptr<store::common::SpdLogger> logger_;  
  };

  class noop_logger : public mongocxx::logger {
    public:
      void operator()(mongocxx::log_level lvl, bsoncxx::stdx::string_view dom, bsoncxx::stdx::string_view msg) noexcept override {
        cout << "LEVEL: " << mongocxx::to_string(lvl) << " DOMAIN: " << dom << " MESSAGE: " << msg << endl;
      }
  };

}
