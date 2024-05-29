#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define TIMESTAMP_MSG_SIZE 30
#define MAX_BACKLOG 100
#define MAX_MSG_SIZE 1000
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

typedef struct log_buffer {
    char *message;
    size_t message_len;
    char timestamp[TIMESTAMP_MSG_SIZE];
} LogBuffer;

typedef struct client {
    int sockfd;
    pthread_t thread_id;
    struct client* next;
} Client;

pthread_mutex_t connected_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
struct client* connected_clients = NULL;

struct context* application_context = NULL; // this may be unnecessary
int server_sockfd = -1;

void free_application_context(void) {
    if (application_context != NULL) {
        free(application_context->port);
        free(application_context->log_filename);
        free(application_context->stats_filename);
        free(application_context->root_path);
    }
    free(application_context);
    application_context = NULL;
}

/**
 * This function parses command line arguments into a global structure
 * that contains the application context.
 */
int parse_args(int argc, char *argv[]) {
    char *optstring = "p:l:s:br:";

    struct option long_options[] = {
        {"port", required_argument, NULL, 'p'},
        {"log", required_argument, NULL, 'l'},
        {"statistics", required_argument, NULL, 's'},
        {"background", no_argument, NULL, 'b'},
        {"root", required_argument, NULL, 'r'}
    };

    application_context = (struct context*)malloc(sizeof(struct context));
    if (application_context == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for application context\n");
        return -1;
    }

    application_context->background = false;
    application_context->port = NULL;
    application_context->log_filename = NULL;
    application_context->stats_filename = NULL;
    application_context->root_path = NULL;

    char option = (char)-1;
    while ((option = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (option) {
            case 'p':
                application_context->port = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->port, optarg);
                break;
            case 'l':
                application_context->log_filename = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->log_filename, optarg);
                break;
            case 's':
                application_context->stats_filename = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->stats_filename, optarg);
                break;
            case 'b':
                application_context->background = true;
                break;
            case 'r':
                application_context->root_path = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->root_path, optarg);
                break;
            case '?':
                fprintf(stderr, "ERROR: unknown or missing argument value\n"); 
                SHOW_PROPER_USAGE(argv[0]);

                free_application_context();

                return -1;
            default:
                fprintf(stderr, "ERROR: unknown or missing argument value\n");
                SHOW_PROPER_USAGE(argv[0]);

                free_application_context();

                return -1;
        }
    }

    if (application_context->port == NULL) {
        fprintf(stderr, "ERROR: port argument is mandatory\n");
        SHOW_PROPER_USAGE(argv[0]);
        free_application_context();
        return -1;
    }

    if (application_context->root_path == NULL) {
        fprintf(stderr, "ERROR: path argument is mandatory\n");
        SHOW_PROPER_USAGE(argv[0]);
        free_application_context();
        return -1;
    }

    return 0;
}

// Consumes data from log buffer
void* log_file_writer(void* arg) {


    return NULL;
}

void remove_client(int sockfd) {
    pthread_mutex_lock(&connected_clients_mutex);

    struct client* prev = NULL;
    struct client* client = connected_clients;

    while (client != NULL) {
        if (client->sockfd == sockfd) {
            break;
        }
        prev = client;
        client = client->next;
    }    

    if (client != NULL) {
        if (prev == NULL) {
            connected_clients = client->next;
        } else {
            prev->next = client->next;
        }

        close(client->sockfd);
        free(client);
    }

    pthread_mutex_unlock(&connected_clients_mutex);
}

int insert_client(int sockfd) {
    struct client* client = (struct client*)malloc(sizeof(struct client));
    if (client == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for client structure\n");
        return -1;
    }

    client->sockfd = sockfd;
    client->thread_id = pthread_self();
    pthread_mutex_lock(&connected_clients_mutex);
    client->next = connected_clients;

    connected_clients = client;

    pthread_mutex_unlock(&connected_clients_mutex);

    return 0;
}

void close_clients(void) {
    pthread_mutex_lock(&connected_clients_mutex);
    struct client* client = connected_clients;
    while (client != NULL) {
        struct client* temp = client->next;
        pthread_cancel(client->thread_id);
        int error = 0;
        if ((error = pthread_join(client->thread_id, NULL)) != 0) {
            fprintf(stderr, "ERROR: couldn't join thread. Error: %d\n", error);
        }
        close(client->sockfd);
        free(client);
        client = temp;
    }
    pthread_mutex_unlock(&connected_clients_mutex);
}

void terminate(int sig) {
    free_application_context();
    close_clients();
    if (server_sockfd != -1) {
        if (close(server_sockfd) != 0) {
            fprintf(stderr, "FATAL ERROR: couldn't close server socket. Error: %s\n", strerror(errno));
        }
    }
    _exit(EXIT_SUCCESS);
}

// Create a socket. Important: this socket should be closed. On error, -1 is returned.
// 'domain' can be AF_INET or AF_INET6
// 'type' should always be SOCK_STREAM
int create_tcp_socket(int domain) {
    int sockfd = socket(domain, SOCK_STREAM, 0); // 0 on protocol as only choice is TCP

    if (sockfd == -1) {
        fprintf(stderr, "FATAL ERROR: couldn't create socket. Error: %s\n", strerror(errno));
        return -1;
    }

    return sockfd;
}

void* client_thread(void* arg) {
    int client_sockfd = *((int*)arg);
    free(arg); // arg is heap allocated

    if (insert_client(client_sockfd) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't insert client in list, will close\n");
        close(client_sockfd);
        return NULL;
    }

    while (1) {
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_SET(client_sockfd, &readfds);

        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);

        struct timeval timeout;
        timeout.tv_sec = 0; 
        timeout.tv_usec = 2000; // 2ms timeout

        int error = select(client_sockfd + 1, &readfds, &writefds, &exceptfds, &timeout);

        if (error == -1) {
            fprintf(stderr, "FATAL ERROR: select call failed. Error: %s\n", strerror(errno));
            return (void*)-1;
        } 

        if (FD_ISSET(client_sockfd, &readfds) != 0) {
            char data[MAX_MSG_SIZE] = {0};

            if (read(client_sockfd, data, sizeof(data)) == 0) {
                break;
            }
        }

        pthread_testcancel();
    }

    remove_client(client_sockfd);

    return NULL;
}

// This function starts a server. On error, -1 is returned. On success,
// it should never return. Port needs to be a string containing a numeric value,
// while domain can be AF_INET or AF_INET6.
int start_server(char *port, int domain) {
    if (port == NULL) {
        fprintf(stderr, "FATAL ERROR: 'port' argument is NULL\n");
        return -1;
    }

    if ((server_sockfd = create_tcp_socket(domain)) == -1) {
        return -1;
    }

    int enable_reuseaddr = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&enable_reuseaddr, sizeof(int)) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't set socket to reuse address. Error: %s\n", strerror(errno));
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    // if domain is IPv6, accept IPv4 addresses aswell
    hints.ai_family = (domain == AF_INET6) ? AF_UNSPEC : domain; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV | AI_NUMERICHOST | AI_PASSIVE;
    struct addrinfo* res;

    int gai_errcode = getaddrinfo(NULL, port, &hints, &res);
    if (gai_errcode != 0) {
        fprintf(stderr, 
            "FATAL ERRROR: couldn't get address structure. getaddrinfo error: %s\n", gai_strerror(gai_errcode));
        return -1;
    }

    if (res == NULL) {
        fprintf(stderr, "FATAL ERROR: no address was found\n");
        return -1;
    }

    if (bind(server_sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        fprintf(stderr, "FATAL ERROR: could not bind to socket. Error: %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(res);

    if (listen(server_sockfd, MAX_BACKLOG) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't listen on socket. Error: %s\n", strerror(errno));
    }

    while (1) {
        // use accept with select
        struct sockaddr client_addr;
        socklen_t client_addrlen = sizeof(client_addr);

        int client_sockfd = accept(server_sockfd, &client_addr, &client_addrlen);

        if (client_sockfd == -1) {
            fprintf(stderr, "LOG MESSAGE: couldn't accept connection. Error: %s\n", strerror(errno));
        }

        int *client_sockfd_alloc = (int*)malloc(sizeof(int));
        if (client_sockfd_alloc == NULL) {
            fprintf(stderr, "LOG MESSAGE: couldn't allocate memory for client socket file descriptor\n");
            continue;
        }

        *client_sockfd_alloc = client_sockfd;

        pthread_t id;
        pthread_create(&id, NULL, client_thread, (void*)client_sockfd_alloc);
    }

    return 0; // never reached
}

int main(int argc, char *argv[]) {
    signal(SIGINT, terminate);

    // parse command line arguments
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't parse args\n");
        return 1;
    }
    
    start_server(application_context->port, AF_INET);

    // need to keep path name and stats file name throughout execution, but log 
    // can be freed after opening the file

    // create one thread to periodically check the log buffer structure
    // create one thread to block on select -> this can be done (and probably should be done) in the main thread
    
    // one should keep track of all active threads, which should be terminate-able 

    return 0;
}