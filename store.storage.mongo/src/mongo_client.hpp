#pragma once

#include "json.hpp"
#include "base_client.hpp"
#include "extensions.hpp"
#include "eventstore.hpp"
#include "time.hpp"
#include "mongo_base_client.hpp"

using namespace std;
using namespace store::common;
using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;
using namespace bsoncxx::builder::stream;

using json = nlohmann::json;

using MongoBaseClient = store::storage::mongo::MongoBaseClient;

namespace store::storage::mongo {
  template<typename T>
  class MongoClient : public BaseClient<T>, public MongoBaseClient {          
  public:
    using BaseClient<T>::BaseClient;
    
    class MongoEventStore : public EventStore {
      const string eventstreams_ = "eventstreams";
      const string events_counter_ = "events_counter";
      const string events_ = "events";
      const string all_events_ = "all_events";

      public:
      explicit MongoEventStore(MongoClient<T>* session_) : session(session_) {}

      int Save() override {

      }

      int SaveOne(const string& streamType, const json& streamObj) {
        MongoBaseClient clientStreams(session->url_, session->database_, eventstreams_);
        MongoBaseClient clientEventsCounter(session->url_, session->database_, events_counter_);
        MongoBaseClient clientEvents(session->url_, session->database_, events_);  

        // Get the stream id that represents the Type.
        auto new_uuid = generate_uuid();
        auto uid = clientStreams.getUid(streamType, new_uuid);

        // Get the global events counter.
        auto globalseq = clientEventsCounter.getNextSequenceValue(all_events_);

        // Get version id that represents the lastest Type.
        auto nextseq = clientEventsCounter.getNextSequenceValue(streamType);

        // Random uuid for the event.
        auto nextuid = generate_uuid();

        auto now = getCurrentTimeMilliseconds();

        IEvent ev;
        ev.seqId = globalseq;
        ev.id = nextuid;
        ev.streamId = uid;
        ev.version = nextseq;
        ev.type = streamType;
        ev.data = streamObj;
        ev.timestamp = now;

        auto b = makeBsonFromEvent(ev);

        clientEvents.insertOne(b->view());
      }

      private:
      MongoClient<T>* session;

      shared_ptr<document> addTimeFields(shared_ptr<document> instream) {
        *instream << "dateCreated" << bsoncxx::types::b_date(std::chrono::system_clock::now())
          << "dateLocal" << getCurrentTime()
          << "dateTimezoneOffset" << getTimezoneOffset();
        return instream;
      }

      shared_ptr<bsoncxx::document::value> makeBsonFromEvent(IEvent ev) {
        auto builder = make_shared<bsoncxx::builder::stream::document>();

        *builder
          << "_id" << ev.id
          << "seqId" << ev.seqId
          << "id" << ev.id
          << "streamId" << ev.streamId
          << "version" << ev.version
          << "type" << ev.type
          << "data" << bsoncxx::types::b_document{ bsoncxx::from_json(ev.data.dump()) }
          << "timestamp" << ev.timestamp;

        builder = addTimeFields(builder);
        
        auto doc = *builder << finalize;
        
        return make_shared<bsoncxx::document::value>(doc);
      }
    };
    
    explicit MongoClient(const string& url, const string& database, const string& collection) : 
      MongoBaseClient(url, database, collection) {}

    MongoEventStore events{ this };

  };
}
