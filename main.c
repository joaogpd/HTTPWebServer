#include "sock/sock.h"
#include "thread_pool/thread_pool.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define PORT "8222"
#define DEBUG
#define SHOW_PROPER_USAGE(program) printf("Usage: %s -p --port <port> [-l --log <filename>] [-s --statistics <filename>] [-b --background] [-r --root <path>]\n", program)

struct context {
    char* port;
    char* log_filename;
    char* stats_filename;
    char* root_path;
    bool background;
};

int sockfd = -1;

struct context* global_context = NULL;

FILE* open_file(char* name, char* mode) {
    FILE* file = fopen(name, mode);
    if (file == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: Couldn't open file %s with mode %s. Error: %s\n", name, mode, strerror(errno));
#endif  
        return NULL;
    }

    return file;
}

void free_global_context(void) {
    if (global_context != NULL) {
        free(global_context->root_path);
        free(global_context->stats_filename);
        free(global_context->log_filename);
        free(global_context->port);
    }
    free(global_context);
}

int parse_args_into_global_context(int argc, char* argv[]) {
    if (argc <= 1) { 
        SHOW_PROPER_USAGE(argv[0]);
        return -1;
    }

    char* optstring = "p:l:s:br:";
    struct option options[] = {
        {"port", required_argument, NULL, 'p'},
        {"log", required_argument, NULL, 'l'},
        {"statistics", required_argument, NULL, 's'},
        {"background", no_argument, NULL, 'b'},
        {"root", required_argument, NULL, 'r'}
    };

    global_context = (struct context*)malloc(sizeof(struct context));
    if (global_context == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't allocate memory for global context\n");
#endif
        return -1;
    }

    global_context->background = false;
    global_context->port = NULL;
    global_context->log_filename = NULL;
    global_context->stats_filename = NULL;
    global_context->root_path = NULL;
    
    char option = (char)-1;
    while ((option = getopt_long(argc, argv, optstring, options, NULL)) != -1) {
        switch (option) {
            case 'p':
                global_context->port = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(global_context->port, optarg);
                break;
            case 'l':
                global_context->log_filename = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(global_context->log_filename, optarg);
                break;
            case 's':
                global_context->stats_filename = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(global_context->stats_filename, optarg);
                break;
            case 'b':
                global_context->background = true;
                break;
            case 'r':
                global_context->root_path = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(global_context->root_path, optarg);
                break;
            case '?':
#ifdef DEBUG
                fprintf(stderr, "ERROR: unknown or missing argument value\n");
#endif 
                SHOW_PROPER_USAGE(argv[0]);

                free_global_context();

                return -1;
            default:
#ifdef DEBUG
                fprintf(stderr, "ERROR: unknown or missing argument value\n");
#endif
                SHOW_PROPER_USAGE(argv[0]);

                free_global_context();

                return -1;
        }
    }

    if (strcmp("", global_context->port) == 0) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: port argument is mandatory\n");
#endif
        free(global_context);
        return -1;
    }

    return 0;
}

void show_global_context(void) {
    if (global_context != NULL) {
        printf("Global context: \nPort: %s\nLog filename: %s\nStats filename: %s\nBackground: %d\nRoot path: %s\n",
            global_context->port, global_context->log_filename, global_context->stats_filename,
            global_context->background, global_context->root_path);
    }
}

void terminate_failure(void) {
    if (sockfd != -1) {
        close_socket(sockfd, 10);
    }
    teardown_thread_pool();
    arena_cleanup();
    free_global_context();
    _exit(EXIT_FAILURE); // exit isn't async-signal safe
}

void terminate(int sig) {
    close_socket(sockfd, 10);
    teardown_thread_pool();
    arena_cleanup();
    free_global_context();
    _exit(EXIT_SUCCESS); // exit isn't async-signal safe
}

int main(int argc, char *argv[]) {
    int error = parse_args_into_global_context(argc, argv);
    if (error == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't parse command line arguments into global context\n");
#endif
        terminate_failure();
        return EXIT_FAILURE;
    }

    signal(SIGINT, terminate);

    if (global_context->background) {
        if (daemon(1, 0) != 0) {
            printf("Couldn't daemonize\n");
            terminate_failure();
            return EXIT_FAILURE;
        } 
    }

    // change working directory
    // redirect input, output, error to opened files

    sockfd = create_TCP_socket(AF_INET);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

    struct host host;
    strcpy(host.address, "");
    strcpy(host.port, global_context->port);
    host.domain = AF_INET;

    struct addrinfo* addrinfo = getsockaddr_from_host(&host);
    struct sockaddr* sockaddr = addrinfo->ai_addr;

    error = bind_socket(sockfd, (struct sockaddr_storage*)sockaddr);

    freeaddrinfo(addrinfo);

    if (error != 0) {
        close_socket(sockfd, 10);
        printf("Couldn't bind socket\n");
        terminate_failure();
        return EXIT_FAILURE;
    }

    listen_on_socket(sockfd);

    if (thread_pool_accept_conn_socket(sockfd) != 0) {
        printf("Something went bad, this should never return\n");
    }

    return 0;
}