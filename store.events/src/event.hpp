#pragma once

#include "json.hpp"

#if __has_include("primitive.hpp")
#include "primitive.hpp"
#else
#include "store.models/src/primitive.hpp"
#endif

using namespace std;
namespace Primitive = store::primitive;

using json = nlohmann::json;

namespace store::events {

  struct IEvent {
    int64_t seqId;
    Primitive::uuid id;
    Primitive::uuid streamId;
    string type;
    int64_t version;    
    json data;      
    int64_t timestamp;
  };

  void to_json(json& j, const IEvent& p) {
    j = json{
      { "seqId", p.seqId },
      { "id", p.id },
      { "streamId", p.streamId },
      { "type", p.type },
      { "version", p.version },      
      { "data", p.data },        
      { "timestamp", p.timestamp }
    };
  }

  void from_json(const json& j, IEvent& p) {
    p.seqId = j.value("seqId", 0);
    p.id = j.value("id", "");
    p.streamId = j.value("streamId", "");
    p.type = j.value("type", "");
    p.version = j.value("version", 0);    
    p.data = j.value("data", json(nullptr));
    p.timestamp = j.value("timestamp", 0);
  }

  template<typename T>
  struct Event : IEvent {
    T model;
  };

}
