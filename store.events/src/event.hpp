#pragma once

#include "json.hpp"
#include "primitive.hpp"

using namespace std;
namespace Primitive = store::primitive;

using json = nlohmann::json;

namespace store {
  namespace events {
    struct IEvent {
      int64_t seqId;
      Primitive::uuid id;
      Primitive::uuid streamId;
      int version;
      json data;
      string type;
      int64_t timestamp;
    };

    template<typename T>
    struct Event : IEvent {
      T model;
    };
  }
}
