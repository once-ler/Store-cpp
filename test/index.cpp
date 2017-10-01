#include "models.hpp";
#include "interfaces.hpp"
#include "ioc/singleton.hpp"

using namespace std;
using namespace store::models;
using namespace store::interfaces;

namespace ioc1 = store::ioc1;
namespace ioc0 = store::ioc0;

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

  {
    ioc0::IOCContainer container0;
    container0.RegisterClass<ParticipantExtended>();
    auto o = container0.GetInstance<ParticipantExtended>();
  }

  {
    ioc1::IOCContainer container1;
    container1.RegisterInstance<Model, Droid>();
    // container1.RegisterFactory<ParticipantExtended>();
    
    // auto name = container1.GetObject<ParticipantExtended>()->name;
    // auto ra = container1.GetObject<RebelAlliance>();
  }

}
