#pragma once

#include "rxcpp/rx.hpp"
#include "json.hpp"
#include "store.events/src/event.hpp"
#include "store.events/src/eventstore.hpp"
#include "store.storage/src/base_client.hpp"

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
  auto scheduler = rxcpp::identity_current_thread();
  auto period = std::chrono::seconds(2);

  // Obervable
  template<typename A>
  decltype(auto) generateSource =
    [](const string& sqlstmt){
      return [&](shared_ptr<BaseClient<A>> client){
        auto a = client->runQueryJson(sqlstmt);

        return Rx::observable<>::iterate(a);
      };
    };

  // Observable  
  template<typename A>
  decltype(auto) generateEvent =
    [](const string& streamType) {
      return [&](auto e){
        auto ev = make_shared<Event<A>>();
        ev->streamId = generate_uuid_v3(streamType.c_str());
        ev->type = streamType;
        ev->data = *e;
        return ev;
      };
    };

  // Obervable
  template<typename A>
  decltype(auto) generateStoreSource =
    [](vector<shared_ptr<A>> a){
      return Rx::observable<>::iterate(a);
    };

  // Observer
  template<typename A>
  decltype(auto) onNextEvent =
    [](EventStore& publisher) {
      return [&](auto ev){
        publisher.Append<A>(*ev);
      };
    };

  template<typename A, typename B>
  decltype(auto) onNextStoreModel =
    [](shared_ptr<BaseClient<A>> publisher) {
      return [&](const string& schema) {
        return [&](B obj){
          publisher->save(schema, *obj);
        };
      };
    };

  auto onAllEventsCompleted = 
    [](EventStore& publisher) {
      return [&](){
        publisher.Save();
      };
    };

}
