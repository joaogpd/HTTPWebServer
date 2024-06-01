#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include "args.h"
#include "log_file_handler.h"
#include "stats_file_handler.h"
#include "clients.h"

#define TIMESTAMP_MSG_SIZE 30
#define MAX_BACKLOG 100
#define MAX_MSG_SIZE 1000

char content_type_array[][20] = {
    "image/jpeg", 
    "image/png", 
    "text/html", 
    "text/css", 
    "text/plain"
};

typedef struct file_response {
    char *file_content;
    size_t file_size;
} FileResponse;

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
        show_stats(application_context->stats_filename);
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
