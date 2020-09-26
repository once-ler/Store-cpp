#pragma once

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "store.storage.connection-pools/src/pgsql.hpp"

using namespace std;

namespace store::storage::pgsql {
  class LargeObjectHandler {
  public:
    LargeObjectHandler(PGconn* conn_): conn(conn_) {}

    Oid createOid() {
      Oid oid = lo_creat(conn, INV_READ | INV_WRITE);
      return oid;
    }

  private:
    PGconn* conn = NULL;
  };
}
