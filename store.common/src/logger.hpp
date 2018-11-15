#pragma once

#include <iostream>

namespace store::common {
  struct ILogger {
    virtual void debug(const char* msg) {
      std::cout << msg << std::endl;
    }

    template<typename... Args>
    void debug(const char *fmt, const Args &... args) {}

    virtual void info(const char* msg) {
      std::cout << msg << std::endl;
    }

    template<typename... Args>
    void info(const char *fmt, const Args &... args) {}

    virtual void warn(const char* msg) {
      std::cerr << msg << std::endl;
    }

    template<typename... Args>
    void warn(const char *fmt, const Args &... args) {}

    virtual void error(const char* msg) {
      std::cerr << msg << std::endl;
    }

    template<typename... Args>
    void error(const char *fmt, const Args &... args) {}

    virtual void flush() {}

    virtual inline void dropLogging(const std::string& logName_ = "") noexcept {}

    virtual void append_error(const std::string& errmsg) {}

    virtual std::string get_errors() {}

    virtual void flush_errors() {}

  };
}
