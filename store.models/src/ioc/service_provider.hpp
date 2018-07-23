#pragma once

#include "store.models/src/ioc/simple_container.hpp"
#include "store.common/src/singleton.hpp"

using namespace store::common;

namespace store {
  namespace ioc {
    auto ServiceProvider = Singleton<SimpleContainer>::Instance();
  }
}
