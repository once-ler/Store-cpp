#pragma once

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "store.storage.connection-pools/src/pgsql.hpp"
#include "store.common/src/logger.hpp"

using namespace std;
namespace Postgres = db::postgres;

// Reference: https://www.postgresql.org/docs/9.5/lo-examplesect.html
namespace store::storage::pgsql {
  template<typename> class Client;
  
  template<typename A>
  class LargeObjectHandler {
  public:
    explicit LargeObjectHandler(Client<A>* session_): session(session_) {}

    void deleteOid(Oid oid) {
      // std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      PGconn* pg_conn = NULL;
      try {
        // conn = session->pool->borrow();
        // lo_unlink(conn->sql_connection->getPGconn(), oid);
        connInfo = getConnInfo();
        pg_conn = PQconnectdb(connInfo.c_str());
        lo_unlink(pg_conn, oid);
      } catch (exception e) {
        session->logger->error(e.what());
      }

      // if (conn)
      //   session->pool->unborrow(conn);

      if (pg_conn)
        PQfinish(pg_conn);
    }

    Oid upload(const string& data) {
      Oid oid;
      PGresult* res;
      PGconn* pg_conn = NULL;
      // std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      
      try {
        // conn = session->pool->borrow();
        // auto pg_conn = conn->sql_connection->getPGconn();
        connInfo = getConnInfo();
        pg_conn = PQconnectdb(connInfo.c_str());
        
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
          // Replace single quotes with 2 single quotes.
          next = regex_replace(next, regex("'"), "''");

          if (lo_write(pg_conn, fd, next.c_str(), next.length()) == -1)
            break;

          len_read += len_end;
        }

        lo_close(pg_conn, fd);

        res = PQexec(pg_conn, "COMMIT");
        PQclear(res);
      } catch (exception e) {
        session->logger->error(e.what());
      }
      
      // if (conn)
      //   session->pool->unborrow(conn);

      if (pg_conn)
        PQfinish(pg_conn);

      return oid;
    }

    Oid uploadFile(const char* filename) {
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      PGresult* res;
      PGconn* pg_conn = NULL;
      Oid oid;
      try {
        // conn = session->pool->borrow();
        // auto pg_conn = conn->sql_connection->getPGconn();
        connInfo = getConnInfo();
        pg_conn = PQconnectdb(connInfo.c_str());
        res = PQexec(pg_conn, "BEGIN");
        oid = lo_import(pg_conn, filename);
        res = PQexec(pg_conn, "COMMIT");
        PQclear(res);
      } catch (exception e) {
        session->logger->error(e.what());
      }
      
      // if (conn)
      //   session->pool->unborrow(conn);
      if (pg_conn)
        PQfinish(pg_conn);

      return oid;  
    }

    string download(Oid oid) {
      string retval;
      std::shared_ptr<PostgreSQLConnection> conn = nullptr;
      PGresult* res;
      PGconn* pg_conn = NULL;

      try {
        // conn = session->pool->borrow();
        // auto pg_conn = conn->sql_connection->getPGconn();
        connInfo = getConnInfo();
        pg_conn = PQconnectdb(connInfo.c_str());
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
        PQclear(res);    
      } catch (exception e) {
        session->logger->error(e.what());
      }
      
      // if (conn)
      //   session->pool->unborrow(conn);
      if (pg_conn)
        PQfinish(pg_conn);

      return retval;
    }

  private:
    Client<A>* session;
    size_t buf_size = 32768;
    string fd_open_failed_error = "Failed operation: lo_open().";
    
    string connInfo;

    string getConnInfo() {
      stringstream ss;
			std::shared_ptr<PostgreSQLConnection> conn = nullptr;
          
      try {
        conn = session->pool->borrow();
        PGconn* pg_conn = conn->sql_connection->getPGconn();
        char* username = PQuser(pg_conn);
        char* password = PQpass(pg_conn);
        char* server = PQhost(pg_conn);
        char* port = PQport(pg_conn);
        char* database = PQdb(pg_conn);
        // postgresql://[user[:password]@][netloc][:port][/dbname][?param1=value1&...]
			  ss << "postgresql://" << username << ":" << password << "@" << server << ":" << port << "/" << database;
			} catch (exception e) {
        session->logger->error(e.what());
      }

      if (conn)
        session->pool->unborrow(conn);

      return ss.str();	
    }

  };
}
