#pragma once

#include "rxcpp/rx.hpp"
#include "json.hpp"
#include "store.events/src/event.hpp"
#include "store.events/src/eventstore.hpp"
#include "store.storage.pgsql/src/pg_client.hpp"

namespace Rx {
  using namespace rxcpp;
  using namespace rxcpp::sources;
  using namespace rxcpp::operators;
  using namespace rxcpp::util;
}
using namespace Rx;
using namespace std;

using namespace store::events;
using namespace store::storage;

using json = nlohmann::json;

namespace store::rx {

  decltype(auto) RxEventLoop = Rx::observe_on_event_loop();
  
  // Observable
  template<typename A>
  decltype(auto) generateSourceFromSql =
    [](const string& sqlstmt){
      return [&](shared_ptr<BaseClient<A>> client){
        auto a = client->runQueryJson(sqlstmt);

        return Rx::observable<>::iterate(a);
      };
    };

  // Observable
  template<typename A>
  decltype(auto) generateSourceFromSharedCollection =
    [](vector<shared_ptr<A>> a){
      return Rx::observable<>::iterate(a);
    };

  // Observable
  template<typename A>
  decltype(auto) generateSourceFromCollection =
    [](const vector<A>& a){
      return Rx::observable<>::iterate(a);
    };

  // Observable  
  template<typename A>
  decltype(auto) generateEvent =
    [](const string& streamType) {
      // e could be shared_ptr<json> or shared_ptr<A>
      return [&](auto& e) -> shared_ptr<Event<A>> {
        if (e == nullptr)
          return nullptr;

        auto ev = make_shared<Event<A>>();
        ev->streamId = generate_uuid_v3(streamType.c_str());
        ev->type = streamType;
        ev->data = *e;
        return ev;
      };
    };

  // Observer
  template<typename A>
  decltype(auto) onNextEvent =
    [](EventStore& publisher) {
      return [&](shared_ptr<Event<A>> ev){
        if (ev != nullptr)
          publisher.Append(*ev);
      };
    };

  // Postgresql Observer
  template<typename A, typename B>
  decltype(auto) onNextPgSqlModel =
    [](const string& schema) {
      return [&](shared_ptr<pgsql::Client<A>> publisher) {
        return [&](shared_ptr<B> obj){
          if (obj != nullptr) 
            publisher->save(schema, *obj);
        };
      };
    };

  // Observer
  template<typename B>
  decltype(auto) onNextIgnore =
    []() {
      return [](const B& a) {
        // No op.
      };
    };

  // Observer
  template<typename B>
  decltype(auto) onNextSharedIgnore =
    []() {
      return [](shared_ptr<B> a) {
        // No op.
      };
    };
 
  auto onAllEventsCompleted = 
    [](EventStore& publisher) {
      return [&](){
        publisher.Save();
      };
    };
}
