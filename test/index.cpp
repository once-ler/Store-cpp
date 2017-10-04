#include "models.hpp";
#include "interfaces.hpp"
#include "ioc/simple_container.hpp"
#include "ioc/service_provider.hpp"
#include "base_client.hpp"

using namespace std;
using namespace store::models;
using namespace store::interfaces;
using namespace store::storage;

namespace ioc = store::ioc;

namespace test {
  template<typename T>
  bool AreEqual(const T& a, const T& b) {
    return &a == &b;
  }

  struct Widget : Model {
    template<typename U>
    U ONE(U o) { return o; };
  };

  template<typename T>
  struct Widget2 : public Widget {
    T ONE(T o) override { return o; };
  };

  template<typename T = Widget>
  class WidgetClient : public BaseClient<T> {
    
  };
}

template<typename F, typename ... Args>
void aFunction(F f, const Args... args) {
  f(args...);
}

int main() {
  struct Droid : Model {
    string model;
  };

  Droid c3po, k2so;
  
  struct RebelAlliance : Affiliation<Participant> {};

  Participant p0;
  p0.id = "1234";
  p0.name = "c3po";
  p0.party = c3po;

  vector<Participant> goodRoster = { p0 };
  
  RebelAlliance rebels;

  rebels.id = "rebels";
  rebels.roster = goodRoster;

  History<Droid> h0;
  h0.id = "0";
  h0.source = c3po;

  Record<Droid> r0;
  r0.current = c3po;
  r0.history = { h0 };

  struct ParticipantExtended : public Participant {
    ParticipantExtended() = default;
    ~ParticipantExtended() = default;

    ParticipantExtended(string id, string name, store::primitive::dateTime ts, boost::any party) : Participant{ id, name, ts, party } {}
  };

  struct RogueOne : Affiliation<ParticipantExtended> {};

  RogueOne rogue1;
  
  ParticipantExtended p1 = { string("5678"), string("k2so"), store::primitive::dateTime(""), k2so };
  
  vector<ParticipantExtended> goodRoster2 = { p1 };

  rogue1.roster = goodRoster2;

  // Should be the same object.
  {
    ioc::SimpleContainer container1;
    container1.RegisterSingletonClass<RogueOne>();

    auto a = container1.GetInstance<RogueOne>();

    auto b = container1.GetInstance<RogueOne>();

    auto same = test::AreEqual(*(a.get()), *(b.get()));
    
    cout << same << endl;

    auto sameShared = test::AreEqual(*a, *b);

    cout << sameShared << endl;
  }

  // Should not be the same object.
  {
    ioc::SimpleContainer container1;
    container1.RegisterClass<RogueOne>();

    auto a = container1.GetInstance<RogueOne>();

    auto b = container1.GetInstance<RogueOne>();

    auto same = test::AreEqual(*(a.get()), *(b.get()));

    cout << same << endl;

    auto sameShared = test::AreEqual(*a, *b);

    cout << sameShared << endl;
  }

  // Use singleton provider.
  {
    ioc::ServiceProvider->RegisterSingletonClass<RogueOne>();

    auto a = ioc::ServiceProvider->GetInstance<RogueOne>();

    auto b = ioc::ServiceProvider->GetInstance<RogueOne>();

    auto sameShared = test::AreEqual(*a, *b);

    cout << sameShared << endl;    
  }

  {
    IStore<RogueOne> istore0;

    auto f = istore0.list<Record<RogueOne>>([&rogue1](string version, int offset = 0, int limit = 10, string sortKey = "id", SortDirection sortDirection = SortDirection::Asc) {
      Record<RogueOne> o = {
        "0", "test", "2017-10-04", rogue1, { rogue1 }
      };

      auto o1 = o;
      return vector<Record<RogueOne>>{ o, o1 };
    });
    
    auto v = f("master", 0, 10, "id", SortDirection::Desc);
  
    cout << v.size() << " " << v.at(0).name  << endl;
    
    auto v1 = istore0.list<Record<RogueOne>>([&rogue1](string version, string typeOfStore, int offset = 0, int limit = 10, string sortKey = "id", string sortDirection = "asc") {
      Record<RogueOne> o = {
        "0", "test", "2017-10-04", rogue1,{ rogue1 }
      };

      auto o1 = o;
      o1.name = "test2";
      return vector<Record<RogueOne>>{ o, o1 };
    }, "master", "RogueOne", 0, 10, "id", "Asc");

    cout << v1.size() << " " << v1.at(1).name << endl;
  }

  {
    using RcdOfRogueOne = Record<RogueOne>;

    IStore<RogueOne> istore0;
    Droid droid2;
    auto r = istore0.makeRecord([&rogue1, &droid2](Droid d) {
      RcdOfRogueOne o = {
        "1234", "0", "2017-10-04", rogue1,{ rogue1 }
      };

      return o;
    }, droid2);

    cout << r.id << endl;
  }

  {
    using namespace test;

    ioc::ServiceProvider->RegisterSingletonClass<WidgetClient<Widget>>();

    auto widgetClient = ioc::ServiceProvider->GetInstance<WidgetClient<Widget>>();

    
  }

}
