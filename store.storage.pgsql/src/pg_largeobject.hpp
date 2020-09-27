#pragma once

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "postgres-exceptions.h"
#include "store.storage.connection-pools/src/pgsql.hpp"
#include "store.common/src/logger.hpp"

using namespace std;
namespace Postgres = db::postgres;

namespace store::storage::pgsql {
  // Reference: https://www.postgresql.org/docs/9.5/lo-examplesect.html

  class LargeObjectHandler {
  public:
    LargeObjectHandler(shared_ptr<ConnectionPool<PostgreSQLConnection>> pool_, shared_ptr<ILogger> logger_): pool(pool_), logger(logger_) {}

    Oid createOid() {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      Oid oid = 0;
      
      try {
        conn = pool->borrow();
        oid = lo_creat(conn->sql_connection->getPGconn(), INV_READ | INV_WRITE);
      } catch (exception e) {
        logger->error(e.what());
      }

      if (conn)
        pool->unborrow(conn);

      return oid;
    }

    void deleteOid(Oid oid) {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      
      try {
        conn = pool->borrow();
        lo_unlink(conn->sql_connection->getPGconn(), oid);
      } catch (exception e) {
        logger->error(e.what());
      }

      if (conn)
        pool->unborrow(conn);
    }

    Oid upload(const string& data, Oid u_oid = 0) {
      auto oid = u_oid == 0 ? createOid() : u_oid;

      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      
      try {
        conn = pool->borrow();
        auto pg_conn = conn->sql_connection->getPGconn();
        auto fd = lo_open(pg_conn, oid, INV_READ|INV_WRITE);
        if (fd < 0)
          throw runtime_error(fd_open_failed_error);
        
        size_t len = data.size(),
        len_read = 0;      

        while (len > len_read) {
          size_t remainder = len - len_read;
          size_t len_end = remainder < buf_size ? remainder : buf_size;
          auto next = data.substr(len_read, len_end);

          if (lo_write(pg_conn, fd, next.c_str(), next.length()) == -1)
            break;

          len_read += len_end;
        }

        lo_close(pg_conn, fd);        
      } catch (exception e) {
        logger->error(e.what());
      }
      
      if (conn)
        pool->unborrow(conn);

      return oid;
    }

    Oid uploadFile(const char* filename, Oid oid = 0) {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;

      try {
        conn = pool->borrow();
        auto pg_conn = conn->sql_connection->getPGconn();

        if (oid == 0)
          return lo_import(pg_conn, filename);
        else
          lo_import_with_oid(pg_conn, filename, oid);
      } catch (exception e) {
        logger->error(e.what());
      }
      
      if (conn)
        pool->unborrow(conn);

      return oid;  
    }

    string downloadFile(Oid oid) {
      string retval;
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;

      try {
        conn = pool->borrow();
        auto pg_conn = conn->sql_connection->getPGconn();

        auto fd = lo_open(pg_conn, oid, INV_READ);
        if (fd < 0)
          throw runtime_error(fd_open_failed_error);

        int nb;
        char buf[buf_size];

        lo_lseek64(pg_conn, fd, 0, SEEK_END);
        auto sz = lo_tell64(pg_conn, fd);
        lo_lseek64(pg_conn, fd, 0, SEEK_SET);
        retval.reserve(sz);

        while ((nb = lo_read(pg_conn, fd, buf, buf_size)) > 0)
          retval.append(buf, nb);      
      } catch (exception e) {
        logger->error(e.what());
      }
      
      if (conn)
        pool->unborrow(conn);

      return retval;
    }

  private:
    shared_ptr<ConnectionPool<PostgreSQLConnection>> pool = nullptr;
    shared_ptr<ILogger> logger = nullptr;
    size_t buf_size = 32768;
    string fd_open_failed_error = "Failed operation: lo_open().";
  };
}
