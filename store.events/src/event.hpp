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
      string type;
      json data;      
      int64_t timestamp;
    };

    void to_json(json& j, const IEvent& p) {
      j = json{
        { "seqId", p.seqId },
        { "id", p.id },
        { "streamId", p.streamId },
        { "version", p.version },
        { "type", p.type },
        { "data", p.data },        
        { "timestamp", p.timestamp }
      };
    }

    void from_json(const json& j, IEvent& p) {
      p.seqId = j.value("seqId", 0);
      p.id = j.value("id", "");
      p.streamId = j.value("streamId", "");
      p.version = j.value("version", 0);
      p.type = j.value("type", "");
      p.data = j.value("data", json(nullptr));
      p.timestamp = j.value("timestamp", 0);
    }

    template<typename T>
    struct Event : IEvent {
      T model;
    };
  }
}
