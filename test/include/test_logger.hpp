#pragma once

#include <boost/filesystem.hpp>
#include "store.common/src/logger.hpp"
#include "store.storage.pgsql/src/pg_client.hpp"
#include "json.hpp"
#include "test_fixtures.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"

using namespace std;
using namespace store::storage::pgsql;
using namespace test::fixtures;
using json = nlohmann::json;

namespace test::logger {
  class SpdLogger : public ILogger {
    public:
    explicit SpdLogger(const string& logName_) : ILogger(), logName(logName_) {
      setupLogging();
    }

    void debug (const char* msg) override {
      spdlog::get(logName)->debug(msg);
    }

    void info (const char* msg) override {
      spdlog::get(logName)->info(msg);
    }

    void warn (const char* msg) override {
      spdlog::get(logName)->warn(msg);
    }

    void error (const char* msg) override {
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
      
      spdlog::init_thread_pool(q_size, 3);
      auto asyncRotatingLogger = spdlog::rotating_logger_mt<spdlog::async_factory>(logName.c_str(), string("log/" + logName + ".txt").c_str(), 1048576 * 5, 5);
      spdlog::register_logger(asyncRotatingLogger);
    }
  };

  void test_splog_spec() {
    json dbConfig = json(nullptr);
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
