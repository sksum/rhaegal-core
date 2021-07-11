#ifndef UTILS_H
#define UTILS_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

// Creates a bound and listening INET socket on the given port number. Returns
// the socket fd when successful; dies in case of errors.
int listen_inet_socket(int portnum);

void die(char *fmt, ...);

void *xmalloc(size_t size);

void perror_die(char *msg);

void report_peer_connected(const struct sockaddr_in *sa, socklen_t salen);

void make_socket_non_blocking(int sockfd);

#endif