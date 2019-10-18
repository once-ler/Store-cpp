#pragma once

#include <boost/filesystem.hpp>
#include "store.common/src/logger.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <vector>
#include <numeric> // Default accumulate is sum
#include <algorithm>

namespace store::common {
  std::mutex log_mutex;

  class SpdLogger : public ILogger {
    public:
    explicit SpdLogger(const std::string& logName_, const bool& runtime_err_save_ = false) : ILogger(), logName(logName_), runtime_err_save(runtime_err_save_) {
      setupLogging();
    }

    explicit SpdLogger(const std::string& logName_, const std::string& subFolder, const bool& runtime_err_save_ = false) : ILogger(), logName(logName_), runtime_err_save(runtime_err_save_) {
      setupLogging(subFolder);
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

      if (runtime_err_save) {
        append_error(msg);
      }
    }

    template<typename... Args>
    void error(const char *fmt, const Args &... args) {
      auto s = fmt::format(fmt, std::forward<const Args&>(args)...);
      spdlog::get(logName)->error(s);

      if (runtime_err_save) {
        append_error(s);
      }
    }

    void flush() {
      spdlog::get(logName)->flush();
    }

    inline void dropLogging(const std::string& logName_ = "") noexcept {
      if (logName_.size() == 0)
        return spdlog::drop_all();
      try {
        spdlog::drop(logName_);
      } catch (std::exception e) {
        std::cerr << e.what() << std::endl;
      }
    }

    // For storing the errors during runtime.
    // Implementer is responsible for emptying the collection.
    void append_error(const std::string& errmsg) override {
      std::lock_guard<std::mutex> lock(log_mutex);
      errors.emplace_back(errmsg + "\n");
    }

    std::string get_errors() override {
      std::lock_guard<std::mutex> lock(log_mutex);
      std::string s;
      s = std::accumulate(std::begin(errors), std::end(errors), s);
      return s;
    }

    void flush_errors() override {
      std::lock_guard<std::mutex> lock(log_mutex);
      errors.clear();
    }

    private:
    std::string logName = "logger";
    
    std::vector<std::string> errors;
    bool runtime_err_save = false;

    void setupLogging(const std::string& subFolder = "") {
      std::string dir = "log";
      if (subFolder.size() > 0) {
        dir.append("/" + subFolder);
      } 
      if (boost::filesystem::create_directories("./" + dir)){
        boost::filesystem::path full_path(boost::filesystem::current_path());
        std::cout << "Successfully created directory"
          << full_path
          << dir
          << "\n";
      }
  
      if (spdlog::get(logName) != nullptr)
        return;

      auto asyncRotatingLogger = spdlog::rotating_logger_mt<spdlog::async_factory>(logName, dir + "/" + logName + ".txt", 1048576 * 5, 5);
      asyncRotatingLogger->set_pattern("[%Y-%m-%d %H:%M:%S:%e] [%l] [thread %t] %v");

      #ifdef DEBUG
        asyncRotatingLogger->set_level(spdlog::level::debug);
      #endif

      spdlog::get(logName)->info("Logging started...");

      spdlog::get(logName)->flush();

      spdlog::flush_every(std::chrono::seconds(3));

    }
  };
}
