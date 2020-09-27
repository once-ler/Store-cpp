#pragma once

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "postgres-exceptions.h"
#include "store.storage.connection-pools/src/pgsql.hpp"
#include "store.common/src/logger.hpp"

using namespace std;
namespace Postgres = db::postgres;

namespace store::storage::pgsql {
  template<typename> class Client;
  
  // Reference: https://www.postgresql.org/docs/9.5/lo-examplesect.html

  template<typename A>
  class LargeObjectHandler {
  public:
    explicit LargeObjectHandler(Client<A>* session_): session(session_) {}

    void deleteOid(Oid oid) {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      
      try {
        conn = session->pool->borrow();
        lo_unlink(conn->sql_connection->getPGconn(), oid);
      } catch (exception e) {
        session->logger->error(e.what());
      }

      if (conn)
        session->pool->unborrow(conn);
    }

    Oid upload(const string& data) {
      Oid oid;
      PGresult* res;
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      
      try {
        conn = session->pool->borrow();
        auto pg_conn = conn->sql_connection->getPGconn();
        
        res = PQexec(pg_conn, "BEGIN");
        oid = lo_creat(pg_conn, INV_READ | INV_WRITE);
        auto fd = lo_open(pg_conn, oid, INV_READ | INV_WRITE);
       
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

        res = PQexec(pg_conn, "COMMIT");
      } catch (exception e) {
        session->logger->error(e.what());
      }
      
      if (conn)
        session->pool->unborrow(conn);

      return oid;
    }

    Oid uploadFile(const char* filename) {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      PGresult* res;
      Oid oid;
      try {
        conn = session->pool->borrow();
        auto pg_conn = conn->sql_connection->getPGconn();
        res = PQexec(pg_conn, "BEGIN");
        oid = lo_import(pg_conn, filename);
        res = PQexec(pg_conn, "COMMIT");  
      } catch (exception e) {
        session->logger->error(e.what());
      }
      
      if (conn)
        session->pool->unborrow(conn);

      return oid;  
    }

    string download(Oid oid) {
      string retval;
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      PGresult* res;

      try {
        conn = session->pool->borrow();
        auto pg_conn = conn->sql_connection->getPGconn();
        res = PQexec(pg_conn, "BEGIN");

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

        lo_close(pg_conn, fd);
        res = PQexec(pg_conn, "COMMIT");       
      } catch (exception e) {
        session->logger->error(e.what());
      }
      
      if (conn)
        session->pool->unborrow(conn);

      return retval;
    }

  private:
    Client<A>* session;
    size_t buf_size = 32768;
    string fd_open_failed_error = "Failed operation: lo_open().";
  };
}
