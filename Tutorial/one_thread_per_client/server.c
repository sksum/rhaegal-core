// Sequential socket server - accepting one client at a time.
//
// Inspired By Eli Bendersky [http://eli.thegreenplace.net]
// This code is in the public domain.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "utils.h"

typedef enum { WAIT_FOR_MSG, IN_MSG } ProcessingState;

void serve_connection(int sockfd) {
  // Clients attempting to connect and send data will succeed even before the
  // connection is accept()-ed by the server. Therefore, to better simulate
  // blocking of other clients while one is being served, do this "ack" from the
  // server which the client expects to see before proceeding.
  if (send(sockfd, "*", 1, 0) < 1) {
    perror_die("send");
  }

  ProcessingState state = WAIT_FOR_MSG;

  while (1) {
    uint8_t buf[1024];
    int len = recv(sockfd, buf, sizeof buf, 0);
    if (len < 0) {
      perror_die("recv");
    } else if (len == 0) {
      break;
    }

    for (int i = 0; i < len; ++i) {
      switch (state) {
      case WAIT_FOR_MSG:
        if (buf[i] == '^') {
          state = IN_MSG;
        }
        break;
      case IN_MSG:
        if (buf[i] == '$') {
          state = WAIT_FOR_MSG;
        } else {
          buf[i] += 1;
          if (send(sockfd, &buf[i], 1, 0) < 1) {
            perror("send error");
            close(sockfd);
            return;
          }
        }
        break;
      }
    }
  }

  close(sockfd);
}

void serve_connection_threaded (int sockfd) {
  unsigned long id = (unsigned long)pthread_self();
  printf("Thread %lu , Socket %d\n", id, sockfd);
  serve_connection(sockfd);
}

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("RUNNING  %ld \n", (long)getpid());

  char thread_option[100] = "FALSE";
  int portnum = 9090;

  if (argc == 2) {
    portnum = atoi(argv[1]);
  } else if (argc == 3) {
    strcpy(thread_option,argv[2]);
  }

  printf("Rheagal serving running on port %d\n", portnum);
  int sockfd = listen_inet_socket(portnum);


  while (1) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    int newsockfd =
        accept(sockfd, (struct sockaddr*)&peer_addr, &peer_addr_len);

    if (newsockfd < 0) {
      perror_die("ERROR on accept");
    }

    report_peer_connected(&peer_addr, peer_addr_len);

    if (!strcmp(thread_option ,"TRUE")) {
      pthread_t client_thread;
      
      int err = pthread_create(&client_thread, NULL, serve_connection_threaded, newsockfd);
      if (err != 0)
        perror("ERROR: THREAD CREATION: ");
      pthread_detach(client_thread);
    } else {
      serve_connection(newsockfd);
    }
    printf("Client satisfied 👍\n");
  }

  return 0;
}