/*
cp -r ../resources bin
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../websocket -I ../../../../cpp-jwt/include -I ../../../../json/single_include/nlohmann  ../004-ws-rs256-server.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L ../../../../websocket/libwebsocket/lib -lwebsocket -levent -lcrypto -lssl -lrt -luuid
*/

#include "store.servers/src/rs256-secure-ws-server.hpp"

using namespace store::servers;

auto main(int argc, char* argv[]) -> int {
  json config_j = json::parse(R"(
    {
      "port": 5555,
      "privateKeyFile": "resources/jwtRS256.sample.key",
      "publicKeyFile": "resources/jwtRS256.sample.key.pub"
    }
  )");
  auto rs256KeyPair = tryGetRS256Key(config_j);
  
  ioc::ServiceProvider->RegisterInstance<RS256KeyPair>(make_shared<RS256KeyPair>(rs256KeyPair));

  int port = config_j["port"].get<int>();

  // Dump a token for testing.
  json j{{"user", "mickeymouse"}};
  j["private_key"] = rs256KeyPair.privateKey;
  auto enc_str = createJwt(j);
  std::ofstream ofs("test.ws.token");
  ofs << "TOKEN=" << enc_str << "\n"
    << R"(websocat ws://localhost:5555/?x-access-token=${TOKEN})" << "\n";
  ofs.close();


  RS256SecureWsServer rs256WsServer;
  rs256WsServer.onMessageReceived = [](user_t* user, string message) {
    string resp = user->sess.user + "\n" + user->sess.sid + "\n" + user->sess.exp_ts; 

    frame_buffer_t *fb = frame_buffer_new(1, 1, 
      resp.size(), 
      resp.c_str()
    );
    if (fb)
      send_a_frame(user->wscon, fb);
    
  };
  cout << "Starting RS256WsServer Server...\n";
  rs256WsServer.serv(port);
}
