#pragma once

#include <iostream>
#include <memory>
#include <regex>

#ifdef _WIN32
#include <winsock2.h>
#endif
#ifndef _WIN32
#include <sys/socket.h>
#endif
#ifndef _WIN32
#include <netinet/in.h>
#endif
#ifndef _WIN32
#include <sys/select.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef _WIN32
#include <arpa/inet.h>
#endif
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

namespace store {
  namespace common {

    static const std::regex ipAddressRegEx("(\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b)");

    class socketImpl {

    public:
      int hasSocket = 0;
      socketImpl(const char* _hostname, int _port) : hostname(_hostname), port(_port) {
        hasSocket = createSocket();
      }
      ~socketImpl() {
#ifdef _WIN32
        closesocket(*sock);
#else
        close(*sock);
#endif
      }
      int connectImpl() {
        if (connect(*sock, (struct sockaddr*)(&sin),
          sizeof(struct sockaddr_in)) != 0) {
          fprintf(stderr, "failed to connect!\n");
          return -1;
        }
        return 0;
      }
      std::shared_ptr<int> getSocket() {
        return sock;
      }
    private:
      const char* hostname;
      int port;
      unsigned long hostaddr;
      std::shared_ptr<int> sock;
      struct sockaddr_in sin;
      struct hostent *hp;

      int createSocket() {
#ifdef WIN32
        WSADATA wsadata;
        int err;

        err = WSAStartup(MAKEWORD(2, 0), &wsadata);
        if (err != 0) {
          fprintf(stderr, "WSAStartup failed with error: %d\n", err);
          return 0;
        }
#endif
        // if ip address was passed in
        if (std::regex_match(hostname, ipAddressRegEx)) {
          hostaddr = inet_addr(hostname);
          sin.sin_addr.s_addr = hostaddr;
        } else {
          if (!(hp = gethostbyname(hostname))) {
            return 0;
          }
          sin.sin_addr = *(struct in_addr*) hp->h_addr_list[0];
        }

        /* Ultra basic "connect to port 443 on host"
        * Your code is responsible for creating the socket establishing the
        * connection
        */
        int sockImpl = socket(AF_INET, SOCK_STREAM, 0);
        sock = std::make_shared<int>();
        if (sock < 0) {
          return 0;
        }
        *sock = sockImpl;

        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);

        return 1;
      }

    };
  }
}
