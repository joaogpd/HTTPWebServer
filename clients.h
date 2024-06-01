#ifndef CLIENTS_H
#define CLIENTS_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

typedef struct client {
    int sockfd;
    pthread_t thread_id;
    struct client *next;
} Client;

extern pthread_mutex_t connected_clients_mutex;
extern struct client *connected_clients;

void remove_client(int sockfd);
int insert_client(int sockfd);
void close_clients(void);

#endif