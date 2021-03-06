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
      virtual pair<int, string> Save() {
        throw("Not implemented error");
        return make_pair(0, "Failed");
      }

      /// <summary>
      /// User defines name of version.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <returns></returns>
      virtual pair<int, string> Save(const string& version) {
        throw("Not implemented error");
        return make_pair(0, "Failed");
      }

      /// <summary>
      /// Returns a set of IEvent starting from a sequence id offset.
      /// </summary>
      /// <param name="type">The type of the IEvent.</param>
      /// <param name="fromSeqId">The sequence id offset to search from.</param>
      /// <param name="limit">The number of IEvent to return.</param>
      /// <param name="exact">Match the type exactly.</param>
      /// <param name="mustExistKeys">List of keys that the json data must contain.</param>
      /// <param name="filters">Filter rows by map of field name and a set of values.</param>
      /// <returns>vector[IEvent]</returns>
      virtual vector<IEvent> Search(const string& type, int64_t fromSeqId, int limit = 10, bool exact = true, vector<string> mustExistKeys = {}, map<string, vector<string>> filters = {} ) {
        throw("Not implemented error");
        return vector<IEvent>{};
      }

      /// <summary>
      /// Returns the sequence id offset of an event type.
      /// </summary>
      /// <param name="type">The type of the IEvent.</param>
      /// <param name="subscriber">The application interested in the IEvent.</param>
      /// <param name="newSeqId">If value is greater than -1, this value will be used as the new sequence id.</param>
      /// <returns>[BigInt]</returns>
      virtual int64_t LastExecution(const string& type, const string& subscriber, int64_t newSeqId = -1) {
        throw("Not implemented error");
        return 0;
      }
    protected:
      vector<IEvent> pending;
      string dbSchema = "dwh";
    };    
  }
}
