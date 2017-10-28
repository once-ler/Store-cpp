#pragma once

#include "json.hpp"
#include "primitive.hpp"

using namespace std;
namespace Primitive = store::primitive;

using json = nlohmann::json;

namespace store {
  namespace events {
    struct IEventStream {
      Primitive::uuid id;
      string type;
      int version;
      int64_t timestamp;
      json snapshot;
      int snapshotVersion;
    };

    template<typename T>
    struct EventStream : IEventStream {
      T model;
    };
  }
}
