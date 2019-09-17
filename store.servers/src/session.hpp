#pragma once

#include "json.hpp"

using nlohmann::json;
using namespace std;

namespace store::servers {

  struct session_t {
    string sid;
    string exp_ts;
    string user;
    bool is_valid = false;
  };

  void to_json(json& j, const session_t& p) {
    j = json{{"sid", p.sid}, {"exp_ts", p.exp_ts}, {"user", p.user}, {"is_valid", p.is_valid}};
  }

  void from_json(const json& j, session_t& p) {
    if (!j["header"].is_object() || !j["payload"].is_object())
      return;

    p.sid = j["header"].value("sid", "");
    p.user = j["payload"].value("user", "");
    p.exp_ts = j["payload"].value("exp_ts", "");
    p.is_valid = true;
  }

}