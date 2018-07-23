#pragma once

#include "json.hpp"
#include "store.models/src/primitive.hpp"

using namespace std;
namespace Primitive = store::primitive;

using json = nlohmann::json;

namespace store::events {

  struct IEventStream {
    Primitive::uuid id;
    string type;
    int64_t version;
    int64_t timestamp;
    json snapshot;
    int64_t snapshotVersion;
  };

  void to_json(json& j, const IEventStream& p) {
    j = json{
      { "id", p.id },
      { "type", p.type },
      { "version", p.version },
      { "timestamp", p.timestamp },
      { "snapshot", p.snapshot },      
      { "snapshotVersion", p.snapshotVersion }
    };
  }

  void from_json(const json& j, IEventStream& p) {
    p.id = j.value("id", 0);
    p.type = j.value("type", "");
    p.version = j.value("version", 0);
    p.timestamp = j.value("timestamp", 0);
    p.snapshot = j.value("snapshot", json(nullptr));    
    p.snapshotVersion = j.value("snapshotVersion", 0);
  }

  template<typename T>
  struct EventStream : IEventStream {
    T model;
  };

}
