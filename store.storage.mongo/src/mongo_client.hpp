#pragma once

#include <mongocxx/pool.hpp>
#include "json.hpp"
#include "store.storage/src/base_client.hpp"
#include "store.models/src/extensions.hpp"
#include "store.common/src/time.hpp"
#include "store.storage.mongo/src/mongo_base_client.hpp"

using namespace std;
using namespace store::common;
using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;
using namespace bsoncxx::builder::stream;

using json = nlohmann::json;

using MongoBaseClient = store::storage::mongo::MongoBaseClient;

using bsoncxx::builder::basic::kvp;

namespace store::storage::mongo {
  
  template<typename T>
  class MongoClient : public BaseClient<T>, public MongoBaseClient {          
  public:
    const string version = "0.2.1";
    using BaseClient<T>::BaseClient;
    
    class MongoEventStore : public EventStore {
      const string eventstreams_ = "eventstreams";
      const string events_counter_ = "events_counter";
      const string events_ = "events";
      const string all_events_ = "all_events";

      public:
      explicit MongoEventStore(MongoClient<T>* session_) : session(session_) {}

      pair<int, string> Save() override {
        throw("Not implemented error");
        return make_pair(0, "Failed");
      }

      int SaveOne(const string& streamType, const json& streamObj) {        
        int rc = 0;
        MongoBaseClient clientStreams(session->database_, eventstreams_);
        MongoBaseClient clientEventsCounter(session->database_, events_counter_);
        
        // Get the stream id that represents the Type.
        auto new_uuid = generate_uuid();
        auto streamId = clientStreams.getUid(streamType, new_uuid);

        // Get the global events counter.
        auto globalseq = clientEventsCounter.getNextSequenceValue(all_events_);

        // Get version id that represents the lastest Type.
        auto nextver = clientEventsCounter.getNextSequenceValue(streamType);

        // Random uuid for the event.
        auto nextuid = generate_uuid();

        auto now = getCurrentTimeMilliseconds();

        rc += saveOneEvent(
          globalseq,
          nextuid,
          streamId,
          streamType,
          nextver,
          streamObj,
          now
        );

        rc += saveOneEventStream(
          streamId,
          streamType,
          nextver,
          now,
          streamObj,
          nextver
        );

        return rc;
      }

      private:
      MongoClient<T>* session;

      int saveOneEvent(        
        int64_t globalseq,
        const string& nextuid,
        const string& streamId,
        const string& streamType,
        int64_t nextver,
        const json& streamObj,
        int64_t now
      ) {
        IEvent ev{
          globalseq,
          nextuid,
          streamId,
          streamType,
          nextver,
          streamObj,
          now
        };

        json j = ev;
        auto b = session->makeBsonFromJson(j);
        MongoBaseClient clientEvents(session->database_, events_);
        clientEvents.logger = session->logger;
        return clientEvents.insertOne(b->view());
      }

      int saveOneEventStream(
        const string& streamId,
        const string& streamType,
        int64_t nextver,
        int64_t now,
        const json& snapshot,
        int64_t snapshotver
      ) {
        IEventStream es{
          streamId,
          streamType,
          nextver,
          now,
          snapshot,
          snapshotver
        };

        json j1 = es;
        auto b1 = session->makeBsonFromJson(j1);
        MongoBaseClient clientStreams(session->database_, eventstreams_);
        clientStreams.logger = session->logger;
        return clientStreams.upsertOne(b1->view(), streamId);
      }
    };
    
   explicit MongoClient(const string& database, const string& collection, shared_ptr<ILogger> logger_ = nullptr) : 
      MongoBaseClient(database, collection) {
        if (logger_)
          this->logger = logger_;
      }
    
    shared_ptr<bsoncxx::document::value> makeBsonFromJson(const json& o) {
      auto x = bsoncxx::from_json(o.dump());
      
      return make_shared<bsoncxx::document::value>(x);
	  }

    MongoEventStore events{ this };

  };
}
