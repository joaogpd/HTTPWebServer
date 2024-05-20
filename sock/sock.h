#ifndef SOCK_H
#define SOCK_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "../thread_pool/thread_pool.h"

#define DEBUG
#define LISTEN_BACKLOG 100
#define MAX_MSG_SIZE 1024
#define MAX_SOCK_THREADS 50
#define THREAD_TIMEOUT_COUNTER 10
#define THREAD_TIMEOUT_TIMER 2000 // microseconds

struct host {
    char address[NI_MAXHOST];
    char port[NI_MAXSERV];
    int domain;
};

/** 
  * Creates a TCP socket and handles errors. If the call succeeds, 
  * the socket_fd will be returned. Otherwise, it returns -1. 
  *
  * @param domain selects protocol family, can be AF_INET or AF_INET6
  * options are: AF_INET and AF_INET6. 
  */
int create_TCP_socket(int domain);

/**
  * Connects a valid open TCP socket to a given address.
  *
  * @param sockfd socket file descriptor, should be a valid and open socket.
  * @param addr address to which the socket should connect. Is able to handle
  * both IPv4 and IPv6 sockets.
  */
int connect_socket(int sockfd, struct sockaddr_storage *addr);

int bind_socket(int sockfd, struct sockaddr_storage *addr);

struct sockaddr* getsockaddr_from_host(struct host* host);

int listen_on_socket(int sockfd);

int select_read_socket(int nreadfds, int *readfds);

int accept_conn_socket(int sockfd);

#endif