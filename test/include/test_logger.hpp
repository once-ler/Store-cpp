#pragma once

#include <boost/filesystem.hpp>
#include "store.common/src/logger.hpp"
#include "spdlog/spdlog.h"
#include "store.storage.pgsql/src/pg_client.hpp"
#include "json.hpp"
#include "test_fixtures.hpp"

using namespace store::storage::pgsql;
using namespace test::fixtures;
using json = nlohmann::json;

namespace test::logger {
  class SpdLogger : ILogger {
    public:
    explicit SpdLogger(const string& logName_) : ILogger(), logName(logName_) {
      setupLogging(logName)
    }

    void debug override(const char* msg) {
      spdlog::get(logName)->debug(msg);
    }

    void info override(const char* msg) {
      spdlog::get(logName)->info(msg);
    }

    void warn override(const char* msg) {
      spdlog::get(logName)->warn(msg);
    }

    void error override(const char* msg) {
      spdlog::get(logName)->error(msg);
    }

    inline void dropLogging(const string& logName_ = "") noexcept {
      if (logName_.size() == 0)
        return spdlog::drop_all();
      try {
        spdlog::drop(logName_);
      } catch (std::exception e) {
        std::cerr << e.what() << std::endl;
      }
    }

    private:
    string logName = "logger";

    void setupLogging() { 
      if (boost::filesystem::create_directory("./log")){
        boost::filesystem::path full_path(boost::filesystem::current_path());
        std::cout << "Successfully created directory"
          << full_path
          << "/log"
          << "\n";
      }
  
      if (spdlog::get(logName) != nullptr)
        return;

      size_t q_size = 1048576; //queue size must be power of 2
      spdlog::set_async_mode(q_size);
  
      std::vector<spdlog::sink_ptr> sinks;
      sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_mt>("log/" + logName, "txt", 0, 0));
      auto combined_logger = std::make_shared<spdlog::logger>(logName.c_str(), begin(sinks), end(sinks));
      combined_logger->set_pattern("[%Y-%m-%d %H:%M:%S:%e] [%l] [thread %t] %v");
      spdlog::register_logger(combined_logger);
    }
  };

  void test_splog_spec() {
    DBContext dbContext{ 
      dbConfig.value("applicationName", "store_pq"), 
      dbConfig.value("server", "127.0.0.1"),
      dbConfig.value("port", 5432), 
      dbConfig.value("database", "eventstore"),
      dbConfig.value("user", "streamer"),
      dbConfig.value("password", "streamer"),
      10 
    };

    pgsql::Client<IEvent> pgClient{ dbContext };
    
    pgClient.events.setDbSchema("dwh");
    // Set the logger
    auto spdLogger = make_shared<SpdLogger>("test_logger");
    pgClient.logger = spdLogger;

    // Error!
    Droid droid{};
    pgClient.save(pgClient.events.getDbSchema(), droid);

  }
}
