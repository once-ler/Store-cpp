#pragma once

#include <iostream>

namespace store::common {
  struct ILogger {
    virtual void debug(const char* msg) {
      std::cout << msg << std::endl;
    }

    virtual void info(const char* msg) {
      std::cout << msg << std::endl;
    }

    virtual void warn(const char* msg) {
      std::cerr << msg << std::endl;
    }

    virtual void error(const char* msg) {
      std::cerr << msg << std::endl;
    }
  };
}
