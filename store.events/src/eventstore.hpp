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

      /// <summary>
      /// Default version is "master".
      /// </summary>
      /// <returns></returns>
      virtual int Save() {
        throw("Not implemented error");
        return -1;
      }

      /// <summary>
      /// User defines name of version.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <returns></returns>
      virtual int Save(const string& version) {
        throw("Not implemented error");
        return -1;
      }
    protected:
      vector<IEvent> pending;
    };    
  }
}