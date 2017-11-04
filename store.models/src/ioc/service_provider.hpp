#pragma once

#include "ioc/simple_container.hpp"
#include "singleton.hpp"

using namespace store::common;

namespace store {
  namespace ioc {
    auto ServiceProvider = Singleton<SimpleContainer>::Instance();
  }
}
