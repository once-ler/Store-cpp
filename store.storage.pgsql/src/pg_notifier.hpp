#pragma once

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <functional>
#include <libpq-fe.h>
#include "socket.hpp"

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  116444736000000000Ui64 // CORRECT
#else
#define DELTA_EPOCH_IN_MICROSECS  116444736000000000ULL // CORRECT
#endif

using namespace std;

static void exit_nicely(PGconn *conn) {
  PQfinish(conn);
  exit(1);
}

struct PGNotifyResult {
  string channel;
  string data;
  int pid;
};

using Callback = function<void(PGNotifyResult)>;

class ClientHandler {
public:
  void operator()(const string& channel) {
    const char *conninfo = "dbname = pccrms";
    PGconn     *conn;
    PGresult   *res;
    PGnotify   *notify;
    int         nnotifies;

    /* Make a connection to the database */
    conn = PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK) {
      fprintf(stderr, "Connection to database failed: %s",
        PQerrorMessage(conn));
      exit_nicely(conn);
    }

    /*
    * Issue LISTEN command to enable notifications from the rule's NOTIFY.
    */
    string cmd = "LISTEN " + channel;
    res = PQexec(conn, cmd.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(conn));
      PQclear(res);
      exit_nicely(conn);
    }

    /*
    * should PQclear PGresult whenever it is no longer needed to avoid memory
    * leaks
    */
    PQclear(res);

    /* Quit after four notifies are received. */
    nnotifies = 0;
    // while (nnotifies < 4) {
    while (true) {
      /*
      * Sleep until something happens on the connection.  We use select(2)
      * to wait for input, but you could also use poll() or similar
      * facilities.
      */
      int         sock;
      fd_set      input_mask;

      sock = PQsocket(conn);

      if (sock < 0)
        break;              /* shouldn't happen */

      FD_ZERO(&input_mask);
      FD_SET(sock, &input_mask);

      if (select(sock + 1, &input_mask, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "select() failed: %s\n", strerror(errno));
        exit_nicely(conn);
      }

      /* Now check for input */
      PQconsumeInput(conn);
      while ((notify = PQnotifies(conn)) != NULL) {
        fprintf(stderr,
          "ASYNC NOTIFY of '%s' received %s from backend PID %d\n",
          notify->relname, notify->extra, notify->be_pid);

        PGNotifyResult result{ notify->relname, notify->extra, notify->be_pid };
        callback(result);

        PQfreemem(notify);
        nnotifies++;
      }
    }

    fprintf(stderr, "Done.\n");

    /* close the connection to the database and cleanup */
    PQfinish(conn);
  }
};
