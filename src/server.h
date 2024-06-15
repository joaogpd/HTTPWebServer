#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include "file_handler.h"

#define MAX_BACKLOG 100
#define MAX_MSG_SIZE 1000
#define TIMESTAMP_MSG_SIZE 30
#define SHOW_PROPER_USAGE(program) \
    printf( \
        "Usage: %s -p --port <port> [-l --log <filename>] [-s --statistics <filename>] [-b --background] [-r --root <path>]\n", \
        program)

typedef struct context {
    char *port;
    char *log_filename;
    char *stats_filename;
    char *root_path;
    bool background;
} Context;

typedef struct client {
    int sockfd;
    pthread_t thread_id;
    struct client *next;
} Client;

// Socket file descriptor for server.
extern int server_sockfd;

// Global structure to contain values of command line arguments.
extern struct context *application_context; 

// Linked list of connected clients.
extern pthread_mutex_t connected_clients_mutex;
extern struct client *connected_clients;

/** Main server and socket functionality. */
// Starts the webserver.
int start_server(char *port, int domain);
// Thread for a client, a new one is created for a new connection.
void *client_thread(void *arg);
// Creates a TCP socket, IPv4 or IPv6.
int create_tcp_socket(int domain);
// Terminates the server by performing all necessary cleanup tasks.
void terminate(int sig);

/** Utility functions for argument handling. */
// Frees the global application_context to prevent memory leaks.
void free_application_context(void);
// Parses command line arguments into the application_context structure.
int parse_args(int argc, char *argv[]);

/** Utility functions for client linked list. */
// Marks client as ended, though entry is not removed, due to the necessity
// of waiting to join on the client thread to assure no resources leak.
void remove_client(int sockfd);
// Insers a new client into the linked list.
int insert_client(int sockfd);
// Traverses the clients linked list to close all clients and join
// on their threads, preventing resource leaks.
void close_clients(void);

#endif