/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I /usr/local/include ../002-basic-graph.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lcpp_redis -ltacopie -lpthread -luuid
*/

#include <iostream>
#include <cpp_redis/cpp_redis>

using namespace std;

struct ResponseHandler {
  vector<string> results;
  vector<string> statistics;
  string response;

  void onResponse(cpp_redis::reply& resp) {
    if (resp.is_array()) {
      auto r = resp.as_array();
      if (r[0].is_array()) {
        for(auto e : r[0].as_array()) {
          results.emplace_back(e.as_string());
          cout << __func__ << ": " << e << endl;
        }
      }

      if (r[1].is_array()) {
        for(auto e : r[1].as_array()) {
          statistics.emplace_back(e.as_string());
          cout << __func__ << ": " << e << endl;
        }
      }
    } else {
      response = resp.as_string();
    }
  }
};

auto handleResponse(cpp_redis::reply& resp) -> void {
  if (resp.is_array()) {    
    for(auto e : resp.as_array()) {
      if (e.is_array())
        handleResponse(e);
      else
        cout << e << endl; 
    }
  } else {
    cout << resp << endl;
  }   
}

auto count() -> int {
  return 0;
}

// https://github.com/swilly22/redis-graph/blob/master/docs/commands.md
void createNode(shared_ptr<cpp_redis::client> client) {
  vector<string> command{"GRAPH.QUERY", "social", "CREATE (:person{name: 'John Doe',age: 32})"};
  ResponseHandler h, h1;
  auto cb = std::bind(&ResponseHandler::onResponse, &h, placeholders::_1);
  client->send(command, [&cb](cpp_redis::reply& resp){
    cb(resp);
  });

  command.clear();
  command = {"GRAPH.QUERY", "social", "CREATE (:country{name: 'Japan'})"};
  client->send(command, handleResponse);

  client->sync_commit();
}

void createEdge(shared_ptr<cpp_redis::client> client) {
  // # Edge
  // -[:relation{...props}]->
  /*
    CREATE (:Rider {name:'Valentino Rossi'})-[:rides]->(:Team {name:'Yamaha'}), (:Rider {name:'Dani Pedrosa'})-[:rides]->(:Team {name:'Honda'}), (:Rider {name:'Andrea Dovizioso'})-[:rides]->(:Team {name:'Ducati'})"
  */
  vector<string> command = {"GRAPH.QUERY", "social", "MATCH (a:person), (b:country) WHERE (a.name = 'John Doe' AND b.name='Japan') CREATE (a)-[:visited{purpose:'pleasure'}]->(b)"};
  ResponseHandler h;
  client->send(command, handleResponse);

  client->sync_commit();
}

void matchQuery(shared_ptr<cpp_redis::client> client) {
  vector<string> command = {"GRAPH.QUERY", "social", "MATCH (p:person)-[v:visited {purpose:\"pleasure\"}]->(c:country) RETURN p.name, p.age, v.purpose, c.name"};
  ResponseHandler h;
  client->send(command, handleResponse);

  client->sync_commit();
}

void deleteGraph(shared_ptr<cpp_redis::client> client) {
  vector<string> command = {"GRAPH.DELETE", "social"};
  ResponseHandler h;
  client->send(command, handleResponse);

  client->sync_commit();
}

auto main(int argc, char *argv[]) -> int {

  auto client = make_shared<cpp_redis::client>();

  client->connect("127.0.0.1", 6379, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
    if (status == cpp_redis::client::connect_state::dropped) {
      std::cerr << "client disconnected from " << host << ":" << port << std::endl;
    }
  });

  createNode(client);
  createEdge(client);
  matchQuery(client);

  deleteGraph(client);
  
}
