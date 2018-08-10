#pragma once

#include <boost/filesystem.hpp>
#include "store.common/src/logger.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"

namespace store::common {
  class SpdLogger : public ILogger {
    public:
    explicit SpdLogger(const string& logName_) : ILogger(), logName(logName_) {
      setupLogging();
    }

    void debug (const char* msg) override {
      spdlog::get(logName)->debug(msg);
    }

    template<typename... Args>
    void debug(const char *fmt, const Args &... args) {
      spdlog::get(logName)->debug(fmt, std::forward<const Args&>(args)...);
    }

    void info (const char* msg) override {
      spdlog::get(logName)->info(msg);
    }

    template<typename... Args>
    void info(const char *fmt, const Args &... args) {
      spdlog::get(logName)->info(fmt, std::forward<const Args&>(args)...);
    }

    void warn (const char* msg) override {
      spdlog::get(logName)->warn(msg);
    }

    template<typename... Args>
    void warn(const char *fmt, const Args &... args) {
      spdlog::get(logName)->warn(fmt, std::forward<const Args&>(args)...);
    }

    void error (const char* msg) override {
      spdlog::get(logName)->error(msg);
    }

    template<typename... Args>
    void error(const char *fmt, const Args &... args) {
      spdlog::get(logName)->error(fmt, std::forward<const Args&>(args)...);
    }

    void flush() {
      spdlog::get(logName)->flush();
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

      auto asyncRotatingLogger = spdlog::rotating_logger_mt<spdlog::async_factory>(logName, "log/" + logName + ".txt", 1048576 * 5, 5);
      asyncRotatingLogger->set_pattern("[%Y-%m-%d %H:%M:%S:%e] [%l] [thread %t] %v");

      spdlog::get(logName)->info("Logging started...");

      spdlog::get(logName)->flush();

      spdlog::flush_every(std::chrono::seconds(3));

    }
  };
}
