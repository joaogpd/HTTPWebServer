#ifndef SOCK_H
#define SOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "graceful_exit.h"

extern int global_socket_fd;

int create_tcp_socket(struct addrinfo* addr);
struct addrinfo* get_addr(char* port, int family);
void assign_name_socket(int sockfd, struct addrinfo* addr);
void listen_on_socket(int sockfd);
int accept_connection(int sockfd);
void close_server_socket(void);

#endif