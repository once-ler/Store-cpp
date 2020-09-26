#pragma once

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "store.storage.connection-pools/src/pgsql.hpp"

using namespace std;

namespace store::storage::pgsql {
  // Reference: https://www.postgresql.org/docs/9.5/lo-examplesect.html

  class LargeObjectHandler {
  public:
    LargeObjectHandler(PGconn* conn_): conn(conn_) {}

    Oid createOid() {
      Oid oid = lo_creat(conn, INV_READ | INV_WRITE);
      return oid;
    }

    void deleteOid(Oid oid) {
      lo_unlink(conn, oid);
    }

    Oid upload(const string& data, Oid u_oid = 0) {
      auto oid = u_oid == 0 ? createOid() : u_oid;

      auto fd = lo_open(conn, oid, INV_READ|INV_WRITE);
      if (fd < 0)
        return -1;

      size_t len = data.size(),
      len_read = 0;      

      while (len > len_read) {
        size_t remainder = len - len_read;
        size_t len_end = remainder < buf_size ? remainder : buf_size;
        auto next = data.substr(len_read, len_end);

        if (lo_write(conn, fd, next.c_str(), next.length()) == -1)
          break;

        len_read += len_end;
      }

      lo_close(conn, fd);

      return oid;
    }

    Oid uploadFile(const char* filename, Oid oid = 0) {
      if (oid == 0)
        return lo_import(conn, filename);
      else
        lo_import_with_oid(conn, filename, oid);
      return oid;  
    }

    string downloadFile(Oid oid) {
      auto fd = lo_open(conn, oid, INV_READ);
      if (fd < 0)
        return "";

      int nb;
      char buf[buf_size];

      lo_lseek64(conn, fd, 0, SEEK_END);
      auto sz = lo_tell64(conn, fd);
      lo_lseek64(conn, fd, 0, SEEK_SET);
      string retval;
      retval.reserve(sz);

      while ((nb = lo_read(conn, fd, buf, buf_size)) > 0)
        retval.append(buf, nb);
      
      return retval;
    }

  private:
    PGconn* conn = NULL;
    size_t buf_size = 32768;
  };
}
