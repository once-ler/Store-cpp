#pragma once

#include "json.hpp"
#include "base_client.hpp"
#include "extensions.hpp"
// #include "eventstore.hpp"
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
    const string version = "0.2.0";
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
        int rc = 0;
        MongoBaseClient clientStreams(session->url_, session->database_, eventstreams_);
        MongoBaseClient clientEventsCounter(session->url_, session->database_, events_counter_);
        
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

      shared_ptr<document> addTimeFields(shared_ptr<document> instream) {
        *instream << "dateCreated" << bsoncxx::types::b_date(std::chrono::system_clock::now())
          << "dateLocal" << getCurrentTimeString()
          << "dateTimezoneOffset" << getTimezoneOffsetSeconds();
        return instream;
      }

      shared_ptr<bsoncxx::document::value> makeBsonFromJson(const json& o) {
        auto builder = make_shared<bsoncxx::builder::stream::document>();

        for (auto& x : json::iterator_wrapper(o)) {
          auto k = x.key();
          auto v = x.value();
          auto p = o[k];
          if (!p.is_primitive()) {
            *builder << k << bsoncxx::types::b_document{ bsoncxx::from_json(v.dump()) };
          } else if (p.is_number_integer()) {
            *builder << k << bsoncxx::types::b_int64{v};
          } else if (p.is_number_float()) {
            *builder << k << bsoncxx::types::b_double{v};
          } else {
            auto str = o[k].get<string>();
            *builder << k << str;
            if (k == "id") {
              *builder << "_id" << str;
            }
          }          
        }
  
        builder = addTimeFields(builder);
        
        auto doc = *builder << finalize;
        
        return make_shared<bsoncxx::document::value>(doc);
      }

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
        auto b = makeBsonFromJson(j);
        MongoBaseClient clientEvents(session->url_, session->database_, events_); 
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
        auto b1 = makeBsonFromJson(j1);
        MongoBaseClient clientStreams(session->url_, session->database_, eventstreams_);
        return clientStreams.upsertOne(b1->view(), streamId);
      }
    };
    
    explicit MongoClient(const string& url, const string& database, const string& collection) : 
      MongoBaseClient(url, database, collection) {}

    MongoEventStore events{ this };

  };
}
