#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include "application_context.h"

#define TIMESTAMP_MSG_SIZE 30
#define MAX_BACKLOG 100
#define MAX_MSG_SIZE 1000
#define SHOW_PROPER_USAGE(program) \
    printf( \
        "Usage: %s -p --port <port> [-l --log <filename>] [-s --statistics <filename>] [-b --background] [-r --root <path>]\n", \
        program)

typedef enum {
    IMAGE_JPEG = 0, 
    IMAGE_PNG = 1, 
    TEXT_HTML = 2, 
    TEXT_CSS = 3, 
    TEXT_PLAIN = 4
} FileType;

char content_type_array[][20] = {
    "image/jpeg", 
    "image/png", 
    "text/html", 
    "text/css", 
    "text/plain"
};

typedef struct log_message {
    char *timestamped_message;
    size_t message_len;
    struct log_message *next;
} LogMessage;

typedef struct stats_message {
    FileType type;
    struct stats_message *next;
} StatsMessage;

typedef struct client {
    int sockfd;
    pthread_t thread_id;
    struct client *next;
} Client;

typedef struct file_response {
    char *file_content;
    size_t file_size;
} FileResponse;

pthread_mutex_t connected_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
struct client *connected_clients = NULL;

pthread_mutex_t log_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_log_data = PTHREAD_COND_INITIALIZER;
struct log_message *log_buffer = NULL;
pthread_t log_file_writer_id = -1;

pthread_mutex_t stats_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct stats_message *stats_buffer = NULL;

FILE *log_file = NULL;

int server_sockfd = -1;

char http_404_response[] = "HTTP/1.1 404 Not Found\r\n \
    Content-Type: text/html; charset=UTF-8\r\n \
    Content-Length: 174\r\n \
    Connection: close\r\n \
    \r\n\r\n \
    <!doctype html> \
    <html> \
    <head> \
        <meta http-equiv='content-type' content='text/html;charset=utf-8'> \
        <title>404 Not Found</title> \
    </head> \
    <body> \
        Arquivo não foi encontrado \
    </body> \
    </html>";

char http_ok_response_pt1[] = "HTTP/1.1 200 OK\r\n \
    Content-Type: ";
char http_ok_response_pt2[] = "; charset=UTF-8\r\n \
    Content-Length: ";
char http_ok_response_pt3[] = "\r\nConnection: close\r\n\\r\n\r\n";

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

#ifdef DEBUG
            printf("Wrote message %s to file\n", message->timestamped_message);
#endif        
    
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

    pthread_mutex_lock(&log_buffer_mutex);

    message->next = log_buffer;
    log_buffer = message;

#ifdef DEBUG
    printf("Produced message: %s\n", message->timestamped_message);
#endif

    pthread_cond_signal(&new_log_data);
    pthread_mutex_unlock(&log_buffer_mutex);

    return NULL;
}

void produce_stats_message(FileType type) {
    struct stats_message *message = (struct stats_message*)malloc(sizeof(struct stats_message));
    if (message == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for stats message\n");
        return;
    }

    message->type = type;
    
    pthread_mutex_lock(&stats_buffer_mutex);

    message->next = stats_buffer;
    stats_buffer = message;

    pthread_mutex_unlock(&stats_buffer_mutex);
}

void show_stats(void) {
    if (application_context->stats_filename == NULL) {
        fprintf(stderr, "ERROR: no statistics filename was provided (this function should not have been called)\n");
        return;
    }

    FILE* stats_file = fopen(application_context->stats_filename, "w");
    if (stats_file == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't open statistics file. Error: %s\n", strerror(errno));
        return;
    }

    int html_counter = 0, css_counter = 0, jpg_counter = 0, png_counter = 0, plain_counter = 0;

    pthread_mutex_lock(&stats_buffer_mutex);

    struct stats_message *message = stats_buffer;
    while (message != NULL) {
        struct stats_message *temp = message->next;

        switch (message->type) {
            case TEXT_HTML:
                html_counter++;
                break;
            case TEXT_CSS:
                css_counter++;
                break;
            case TEXT_PLAIN:
                plain_counter++;
                break;
            case IMAGE_JPEG:
                jpg_counter++;
                break;
            case IMAGE_PNG:
                png_counter++;
                break;
            default:
                fprintf(stderr, "ERROR: invalid option for statistics\n");
                break;
        }

        free(message);

        message = temp;
        stats_buffer = message;
    }

    pthread_mutex_unlock(&stats_buffer_mutex);

    fprintf(stats_file, 
        "HTML files: %d\nCSS files: %d\nPlain files: %d\nJPG/JPEG files: %d\nPNG files: %d\n",
         html_counter, css_counter, plain_counter, jpg_counter, png_counter);

    fclose(stats_file);
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
    close_clients();
    free_log_buffer();

    if (server_sockfd != -1) {
        if (close(server_sockfd) != 0) {
            fprintf(stderr, "FATAL ERROR: couldn't close server socket. Error: %s\n", strerror(errno));
        }
    }

    if (application_context->stats_filename != NULL) {
        show_stats();
    }   

    free_application_context();

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

char* get_file_path(char *request) {
    if (request == NULL) {
        fprintf(stderr, "ERROR: file path was NULL\n");
        return NULL;
    }

    char current_char;
    current_char = *request;

    char *expected_request = "GET";

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

struct file_response *get_file_content(char *path) {
    // call stat to get file attributes
    char *full_path = (char*)malloc(sizeof(char) * (strlen(path) + strlen(application_context->root_path) + 1));
    if (full_path == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for full file path\n");
        return NULL;
    }
    
    strcpy(full_path, "");
    strcat(full_path, application_context->root_path);
    strcat(full_path, path);

    struct stat statbuf;
    memset(&statbuf, 0, sizeof(struct stat));

    if (stat(full_path, &statbuf) != 0) {
        fprintf(stderr, "ERROR: couldn't get information on file. Error: %s\n", strerror(errno));
        free(full_path);
        return NULL;
    }

    char *file_content = (char*)malloc(sizeof(char) * (statbuf.st_size + 1));
    if (file_content == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for file content\n");
        
        free(full_path);
       
        return NULL;
    }

    memset(file_content, 0, statbuf.st_size + 1);

    FILE* file = fopen(full_path, "r");
    if (file == NULL) {
        fprintf(stderr, "ERROR: couldn't open requested file for reading. Error: %s\n", strerror(errno));
        
        free(full_path);
        
        return NULL;
    }

    if (fread(file_content, sizeof(char), statbuf.st_size, file) != statbuf.st_size) {
        fprintf(stderr, "ERROR: couldn't read the entire file contents.\n");
       
        free(full_path);
        fclose(file);
       
        return NULL;
    }

    struct file_response *response = (struct file_response*)malloc(sizeof(struct file_response));
    if (response == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for file response\n");
        
        free(full_path);
        free(file_content);
        
        return NULL;
    }

    response->file_content = file_content; 
    response->file_size = statbuf.st_size;

    // file_content[statbuf.st_size] = '\0';

    free(full_path);
    fclose(file);

    return response;
}

FileType get_file_type(char *extension) {
    if (strcmp("jpg", extension) == 0 || strcmp("jpeg", extension) == 0) {
        return IMAGE_JPEG;
    } else if (strcmp("png", extension) == 0) {
        return IMAGE_PNG;
    } else if (strcmp("html", extension) == 0) {
        return TEXT_HTML;
    } else if (strcmp("css", extension) == 0) {
        return TEXT_CSS;
    } else { // unknown extension
        return TEXT_PLAIN;
    }
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

            time_t timestamp = time(NULL);
            struct tm *timestamp_tm = localtime(&timestamp);

            char *timestamp_str = (char*)malloc(sizeof(char) * TIMESTAMP_MSG_SIZE);
            if (timestamp_str ==  NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for timestamp.\n");
                continue;
            }

            strftime(timestamp_str, TIMESTAMP_MSG_SIZE, "%a, %d %b %Y %T %Z", timestamp_tm);
            if (timestamp_str == NULL) {
                free(timestamp_str);
                log_message_producer((void*)"LOG MESSAGE: Missing timestamp");
                continue;
            }

            char* file_path = get_file_path(data);
            if (file_path == NULL) {
                free(timestamp_str);
                log_message_producer((void*)"LOG MESSAGE: Malformed query");
                continue;
            }

            char* message = (char*)malloc(sizeof(char) * 
                (strlen("LOG MESSAGE: ") + strlen(timestamp_str) + strlen(file_path) + strlen(" - ") + 1));
            if (message == NULL) {
                free(timestamp_str);
                free(file_path);
                fprintf(stderr, "ERROR: couldn't allocate memory for message\n");
                continue;
            }

            strcpy(message, "LOG MESSAGE: ");
            strcat(message, file_path);
            strcat(message, " - ");
            strcat(message, timestamp_str);

            free(timestamp_str);

            log_message_producer((void*)message);
        
            free(message);

            if (strcmp("/", file_path) == 0) {
                free(file_path);

                file_path = (char*)malloc(sizeof(char) * (strlen("/index.html") + 1));
                if (file_path == NULL) {
                    fprintf(stderr, "ERROR: couldn't allocate memory for index.html file path\n");
                    continue;
                }
                
                strcpy(file_path, "/index.html");
            }

            // get file extension
            char *file_extension = strrchr(file_path, (int)'.');
            if (file_extension == NULL) {
                free(file_path);

                if (send(client_sockfd, http_404_response, sizeof(char) * strlen(http_404_response), 0) != sizeof(char) * strlen(http_404_response)) {
                    fprintf(stderr, "ERROR: couldn't send all data. Error: %s\n", strerror(errno));
                    continue;
                }
                
                log_message_producer("LOG MESSAGE: 404 Not Found");

                remove_client(client_sockfd);
                
                return NULL;    
            }
            file_extension++; // get past dot

            FileType type = get_file_type(file_extension);

            struct file_response* file_response = get_file_content(file_path);
            if (file_response == NULL) {
                free(file_path);

                if (send(client_sockfd, http_404_response, sizeof(char) * strlen(http_404_response), 0) != sizeof(char) * strlen(http_404_response)) {
                    fprintf(stderr, "ERROR: couldn't send all data. Error: %s\n", strerror(errno));
                    continue;
                }

                log_message_producer("LOG MESSAGE: 404 Not Found");

                remove_client(client_sockfd);
                
                return NULL; 
            }

            free(file_path);

            char *content_type = content_type_array[type]; // this needs to be used to produce the response

            char content_length_str[10] = {0};

            sprintf(content_length_str, "%ld", file_response->file_size);

            size_t http_response_len = strlen(http_ok_response_pt1) + strlen(http_ok_response_pt2) + strlen(http_ok_response_pt3)
                + strlen(content_length_str) + file_response->file_size + strlen(content_type) + 1;

            char *http_response = (char*)malloc(sizeof(char) * (http_response_len));
            if (http_response == NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for http response\n");
               
                free(file_response->file_content);
                free(file_response);
                
                return NULL;
            }

            memset(http_response, 0, http_response_len);

            strcpy(http_response, "");
            strcat(http_response, http_ok_response_pt1);
            strcat(http_response, content_type);
            strcat(http_response, http_ok_response_pt2);
            strcat(http_response, content_length_str);
            strcat(http_response, http_ok_response_pt3);
            memcpy(http_response + (http_response_len - file_response->file_size - 1),
                 file_response->file_content, file_response->file_size);

            if (application_context->stats_filename != NULL) {
                produce_stats_message(type);
            }

            if (send(client_sockfd, http_response, sizeof(char) * http_response_len, 0) != sizeof(char) * http_response_len) {
                fprintf(stderr, "ERROR: couldn't send all data. Error: %s\n", strerror(errno));
                continue;
            }

            free(http_response);
            free(file_response->file_content);
            free(file_response);

            remove_client(client_sockfd);

            return NULL;
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

    printf("Server started. Exit with 'kill -SIGUSR1 <pid>'.\n");

    start_server(application_context->port, AF_INET);

    terminate(0);

    return 0;
}
