#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define MAX_BACKLOG 100
#define MAX_MSG_SIZE 1000
#define TIMESTAMP_MSG_SIZE 30

extern int server_sockfd;

int start_server(char *port, int domain);
void *client_thread(void *arg);
int create_tcp_socket(int domain);

#endif