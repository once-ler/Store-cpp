#pragma once
#include "event.hpp"
#include "json.hpp"

using namespace std;

using json = nlohmann::json;

namespace store {
  namespace events {
    class EventStore {
    public:
      template<typename U>
      void Append(Event<U> doc) {
        json j = doc.model;

        IEvent ie{ doc.seqId, doc.id, doc.streamId, doc.version, j, doc.type, doc.timestamp };
        pending.push_back(move(ie));
      }

      virtual int Save() {
        throw("Not implemented error");
        return -1;
      }
    protected:
      vector<IEvent> pending;
    };    
  }
}
