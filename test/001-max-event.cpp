/*
g++ -std=c++14 -I ../../ -I /usr/local/include -I ../../../Store-cpp -I ../../../json/single_include/nlohmann ../index-max-event.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib
*/
#include <cassert> 
#include <numeric>
#include "store.common/src/group_by.hpp"
#include "store.events/src/event.hpp"

using namespace std;
using namespace store::events;

vector<IEvent> listOfEvents(int howMany = 10) {
  vector<string> types = {"Type A", "Type B", "Type C", "Type D"};
  vector<IEvent> events;
  
  for(auto name : types) {
    for(int i = 0; i < howMany; i++) {
      IEvent ev;
      ev.seqId = i;
      ev.data = {
        {"id", name}
      };
      events.emplace_back(ev);  
    }
  }
  return events;
};

vector<IEvent> getMaxEvent(vector<IEvent>& events) {
  vector<IEvent> latest;

  const auto& streams = groupBy(events.begin(), events.end(), [](IEvent& e) { return e.data.value("id", ""); });

  // Get the member with max sequence
  for(const auto& o : streams) {
    auto streamId = o.first;
    auto stream = o.second;
    IEvent base;
    base.seqId = -1; 

    IEvent maxEv = std::accumulate(stream.begin(), stream.end(),
      base,
      [](auto m, IEvent& ev) {
        return ev.seqId > m.seqId ? ev : m;
      }
    );

    latest.emplace_back(maxEv);
  }

  return latest;
}

int main(int argc, char *argv[]) {
  auto l = listOfEvents();

  cout << "Count of original list " << l.size() << endl;

  auto v = getMaxEvent(l);

  cout << "Count of modified list " << v.size() << endl;
  
  for(auto& e : v) {
    cout << e.data["id"] << ": " << e.seqId << endl;

    assert(e.seqId == 9);
  }
}
