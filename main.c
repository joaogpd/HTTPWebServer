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
#include <time.h>
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

typedef struct log_message {
    char *timestamped_message;
    size_t message_len;
    struct log_message *next;
} LogMessage;

typedef struct stats_message {
    char *file_type;
    char *file_name;
    struct stats_message *next;
} StatsMessage;

typedef struct client {
    int sockfd;
    pthread_t thread_id;
    struct client *next;
} Client;

pthread_mutex_t connected_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
struct client *connected_clients = NULL;

pthread_mutex_t log_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_log_data = PTHREAD_COND_INITIALIZER;
struct log_message *log_buffer = NULL;
pthread_t log_file_writer_id = -1;

pthread_mutex_t stats_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct stats_message *stats_buffer = NULL;

FILE *log_file = NULL;

struct context *application_context = NULL; // this may be unnecessary
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
void* log_file_writer(void* log_filename) {
    log_file_writer_id = pthread_self();

    log_file = fopen((char*)log_filename, "a");
    if (log_file == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't open log file. Error: %s\n", strerror(errno));
        log_file_writer_id = -1;
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&log_buffer_mutex);
        pthread_cond_wait(&new_log_data, &log_buffer_mutex);
        pthread_testcancel();

        struct log_message *message = log_buffer;
        while (message != NULL) {
            struct log_message *temp = message->next;
            
            fwrite(message->timestamped_message, sizeof(char), message->message_len, log_file);
            fwrite("\n", sizeof(char), 1, log_file);
            
            free(message->timestamped_message);
            free(message);
            message = temp;

            log_buffer = message;
        }

        fflush(log_file);

        pthread_mutex_unlock(&log_buffer_mutex);
    }

    return NULL;
}

void* log_message_producer(void* msg) {
    struct log_message *message = (struct log_message*)malloc(sizeof(struct log_message));
    if (message == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for log message. Error: %s\n", strerror(errno));    
        return NULL;
    }

    message->timestamped_message = (char*)malloc(sizeof(char) * (strlen(msg) + 1));
    if (message->timestamped_message == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for timestamped_message. Error: %s\n", strerror(errno));   
        free(message); 
        return NULL;
    }

    strcpy(message->timestamped_message, (char*)msg);

    message->message_len = strlen((char*)msg); 

    free(msg);

    pthread_mutex_lock(&log_buffer_mutex);

    message->next = log_buffer;
    log_buffer = message;

    pthread_cond_signal(&new_log_data);
    pthread_mutex_unlock(&log_buffer_mutex);

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

void free_log_buffer(void) {
    pthread_mutex_lock(&log_buffer_mutex);

    struct log_message *message = log_buffer;
    while (message != NULL) {
        struct log_message *temp = message->next;

        free(message->timestamped_message);
        free(message);

        message = temp; 
    }

    pthread_mutex_unlock(&log_buffer_mutex);
}

void terminate_log_file_writer(void) {
    if (log_file_writer_id != -1) {
        pthread_cancel(log_file_writer_id);
        int error = 0;
        if ((error = pthread_join(log_file_writer_id, NULL)) != 0) {
            fprintf(stderr, "ERROR: couldn't join thread. Error: %d\n", error);
        }
    }
}

void terminate(int sig) {
    free_application_context();
    close_clients();
    free_log_buffer();

    if (server_sockfd != -1) {
        if (close(server_sockfd) != 0) {
            fprintf(stderr, "FATAL ERROR: couldn't close server socket. Error: %s\n", strerror(errno));
        }
    }

    terminate_log_file_writer();
    if (log_file != NULL) {
        fclose(log_file);
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

char* get_file_path(char* request) {
    if (request == NULL) {
        fprintf(stderr, "ERROR: file path was NULL\n");
        return NULL;
    }

    char current_char;
    current_char = *request;

    char* expected_request = "GET";

    while (current_char != ' ') {
        if (current_char != *expected_request) {
            fprintf(stderr, "ERROR: malformed request\n");
            return NULL;
        }
        request++;
        expected_request++;
        current_char = *request;
    }

    request++;
    char *file_path_start = request;
    int counter = 0;

    current_char = *request;

    while (current_char !=  ' ') {
        request++;
        expected_request++;
        current_char = *request;
        counter++;
    }

    char *file_path = (char*)malloc(sizeof(char) * (counter + 1));
    if (file_path == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for file path\n");
        return NULL;
    }

    int up_counter = 0;

    while (counter--) {
        file_path[up_counter] = file_path_start[up_counter];
        up_counter++;
    }

    file_path[up_counter] = '\0';

    return file_path;
}

void *client_thread(void *arg) {
    int client_sockfd = *((int*)arg);
    free(arg); // arg is heap allocated

    if (insert_client(client_sockfd) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't insert client in list, will close\n");
        close(client_sockfd);
        return NULL;
    }

    while (1) {
        pthread_t id;

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
            
            // need to get timestamp

            time_t timestamp = time(NULL);
            char* timestamp_str = asctime(localtime(&timestamp));
            if (timestamp_str == NULL) {
                pthread_create(&id, NULL, log_message_producer, (void*)"Missing timestamp");
                continue;
            }

            timestamp_str[strlen(timestamp_str) - 1] = '\0';

            char* file_path = get_file_path(data);
            if (file_path == NULL) {
                pthread_create(&id, NULL, log_message_producer, (void*)"Malformed query");
                continue;
            }

            char* message = (char*)malloc(sizeof(char) * (strlen(timestamp_str) + strlen(file_path) + strlen(" - ") + 1));
            if (message == NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for message\n");
                continue;
            }

            strcpy(message, "");
            strcat(message, file_path);
            strcat(message, " - ");
            strcat(message, timestamp_str);

            free(file_path);

            pthread_create(&id, NULL, log_message_producer, message);
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

    if (application_context->log_filename != NULL) {
        pthread_create(&log_file_writer_id, NULL, log_file_writer, application_context->log_filename);
    }

    start_server(application_context->port, AF_INET);

    // need to keep path name and stats file name throughout execution, but log 
    // can be freed after opening the file

    // create one thread to periodically check the log buffer structure
    // create one thread to block on select -> this can be done (and probably should be done) in the main thread
    
    // one should keep track of all active threads, which should be terminate-able 

    return 0;
}