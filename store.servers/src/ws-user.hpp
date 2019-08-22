#pragma once

#include "libwebsocket/include/connection.h"

namespace store::servers {

  struct user_t {
    uint32_t id;
    ws_conn_t *wscon;
    string msg;
  };

  user_t* user_create() {
    user_t* user = new (nothrow) user_t;
    if (user) {
      user->id = 0;
      user->wscon = ws_conn_new();
      user->msg = "";
    }
    return user;
  }


  void user_destroy(user_t* user) {
    if (user) {
      if (user->wscon) {
        ws_conn_free(user->wscon);
      }
      delete user;
    }
  }

}