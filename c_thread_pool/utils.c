// Utility functions for socket servers in C.
//
// Inspired By Eli Bendersky [http://eli.thegreenplace.net]
// This code is in the public domain.
#include "utils.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <netdb.h>

#define N_BACKLOG 64

void perror_die(char* msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

void report_peer_connected(const struct sockaddr_in* sa, socklen_t salen) {
  char hostbuf[NI_MAXHOST];
  char portbuf[NI_MAXSERV];
  if (getnameinfo((struct sockaddr*)sa, salen, hostbuf, NI_MAXHOST, portbuf,   //address-to-name;
                  NI_MAXSERV, 0) == 0) {
    printf("Client (%s, %s) connected\n", hostbuf, portbuf);
  } else {
    printf("Client (unknonwn) connected\n");
  }
}

int listen_inet_socket(int portnum) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror_die("ERROR: OPENING SOCKET: ");
  }

  int opt = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror_die("SETSOCKOPT");
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portnum);

  if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror_die("ERROR: ON BINDING : ");
  }

  if (listen(sockfd, N_BACKLOG) < 0) {
    perror_die("ERROR: ON LISTENING : ");
  }

  return sockfd;
}
