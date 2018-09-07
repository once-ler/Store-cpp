#pragma once

#include "store.events/src/event.hpp"
#include "json.hpp"

using namespace std;

using json = nlohmann::json;

namespace store {
  namespace events {
    class EventStore {
    public:
      template<typename U>
      void Append(Event<U> doc) {
        IEvent ie{ doc.seqId, doc.id, doc.streamId, doc.type, doc.version, doc.data, doc.timestamp };
        pending.push_back(move(ie));
      }

      /// <summary>
      /// Save() should be overridden.  Then Reset() should be called.
      /// </summary>
      /// <returns></returns>
      void Reset() {
        pending.clear();
      }

      /// <summary>
      /// The database schema that will be used by the event store.
      /// This only makes sense for databases that can support schemas.
      /// </summary>
      /// <returns></returns>
      void setDbSchema(string dbSchema_) {
        dbSchema = dbSchema_;
      }

      string getDbSchema() {
        return dbSchema;
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
      string dbSchema = "dwh";
    };    
  }
}
