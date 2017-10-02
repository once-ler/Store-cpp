#include "models.hpp";
#include "interfaces.hpp"
#include "ioc/simple_container.hpp"
#include "ioc/service_provider.hpp"

using namespace std;
using namespace store::models;
using namespace store::interfaces;

namespace ioc = store::ioc;

namespace test {
  template<typename T>
  bool AreEqual(const T& a, const T& b) {
    return &a == &b;
  }
}

int main() {
  struct Droid : Model {
    string name;
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
}
