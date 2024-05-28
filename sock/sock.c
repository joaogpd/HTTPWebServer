#include "sock.h"
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>

arena_t arena_sock = -1;
pthread_mutex_t arena_sock_mutex = PTHREAD_MUTEX_INITIALIZER;

int create_TCP_socket(int domain) {
    if (domain != AF_INET && domain != AF_INET6) {
	    fprintf(stderr, "ERROR: domain not supported\n");
	    return -1;
    }

    // create TCP socket
    int sock_fd = socket(domain, SOCK_STREAM, IPPROTO_TCP);

    if (sock_fd == -1) {
	    fprintf(stderr, 
            "ERROR: socket could not be opened. Error: %s\n",
            strerror(errno));
    }

    return sock_fd;
}

// need to handle SIGPIPE

int connect_socket(int sockfd, struct sockaddr_storage *addr) {
    if (sockfd == -1) {
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
        return -1;
    }

    // TODO

    return 0;
}

int bind_socket(int sockfd, struct sockaddr_storage *addr) {
    if (addr == NULL) {
        fprintf(stderr, "ERROR: addr argument was NULL\n");
        return -1;
    }

    socklen_t sockaddr_len = (addr->ss_family == AF_INET) 
        ? sizeof(struct sockaddr_in) 
        : sizeof(struct sockaddr_in6);

    int error = bind(sockfd, (struct sockaddr*)addr, sockaddr_len);

    if (error == -1) {
        fprintf(stderr, 
            "ERROR: couldn't bind socket to host. Error: %s\n",
             strerror(errno));
        return -1;
    }

    return 0;
}

struct addrinfo* getsockaddr_from_host(struct host* host) {
    if (host == NULL) {
        fprintf(stderr, "ERROR: host argument was NULL.\n");
        return NULL;
    }

    struct addrinfo hints = {0};
    struct addrinfo* response = NULL;
    hints.ai_family = host->domain;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    // AI_NUMERICHOST prevents any potentially lengthy network host address lookups
    // AI_NUMERICSERV does the same but inhibiting the invocation of a name resolution service
    // AI_PASSIVE will return a socket address suitable for binding if address is NULL
    // AI_ADDRCONFIG will only return IPv4 or IPv6 addresses if there's at least one configured
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE | AI_ADDRCONFIG; 

    int error = 0;
    if (strcmp(host->address, "") == 0) {
        error = getaddrinfo(NULL, host->port, &hints, &response);
    } else {
        error = getaddrinfo(host->address, host->port, &hints, &response);
    }

    if (error != 0) {
        if (error == EAI_SYSTEM) {
            fprintf(stderr, 
            "ERROR: couldn't get socket address from host. 'getaddrinfo' error: %s. Error: %s\n", 
                    gai_strerror(error),
                    strerror(errno));
        } else {
            fprintf(stderr, 
            "ERROR: couldn't get socket address from host. 'getaddrinfo' error: %s.", 
                    gai_strerror(error));
        }
        return NULL;
    }

    return response;
}

int listen_on_socket(int sockfd) {
    if (sockfd == -1) {
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
        return -1;
    }

    int error = listen(sockfd, LISTEN_BACKLOG);

    if (error != 0) {
        fprintf(stderr, 
            "ERROR: couldn't start listening on socket. Error: %s\n",
            strerror(errno));
        return -1;
    }

    return 0;
}

int close_socket(int sockfd, int maxtries) {
    int error = 0;
    int counter = 0;
    while ((error = close(sockfd)) != 0) {
        fprintf(stderr,
         "ERROR: couldn't close socket. Error: %s\n", 
         strerror(errno));
        counter++;
        if (counter >= maxtries) {
            return -1;
        }
    }

    return 0;
}

static void* free_buffer_entry(void* arg) {
    struct buffer* buffer_entry = (struct buffer*)arg;

    arena_free_memory(arena_sock, buffer_entry->value);
    arena_free_memory(arena_sock, buffer_entry);

    return NULL;
}

void* thread_read_socket(void* sockfd) {
    char hostname[NI_MAXHOST] = {0}, servname[NI_MAXSERV] = {0};
    char data[MAX_MSG_SIZE] = {0};

    int sockfd_int = *((int*)sockfd);

    struct sockaddr addr = {0};
    socklen_t addrlen = sizeof(struct sockaddr);

    // TODO: check for errors
    getpeername(sockfd_int, &addr, &addrlen); 

    // TODO: check for errors
    getnameinfo(&addr, addrlen, hostname, sizeof hostname, 
        servname, sizeof servname, NI_NUMERICHOST | NI_NUMERICSERV);

    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;

    FD_ZERO(&readfds);
    FD_SET(sockfd_int, &readfds);
    
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 2000; // wait for 2ms

    int error = select(sockfd_int + 1, &readfds, &writefds, &exceptfds, &timeout);

    if (error > 0) {
        if (FD_ISSET(sockfd_int, &readfds)) {
            int byte_count = 0;
            byte_count = read(sockfd_int, data, MAX_MSG_SIZE);

            // connection ended
            if (byte_count <= 0) { 
                return (void*)-1;
            }

// // #ifdef DEBUG
//             printf("Got message of size %d bytes from %s:%s: %s\n", byte_count, hostname, servname, data);
// // #endif
            printf("Got message of size %d bytes from %s:%s\n", byte_count, hostname, servname);
            char log_message[MAX_MSG_SIZE] = {0};
            strcat(log_message, "Requisition from ");
            strcat(log_message, servname);
            pthread_t id = pthread_self();
            char pthread_id[50] = {0};
            sprintf(pthread_id, " from thread %ld", id);
            strcat(log_message, pthread_id); 

            struct buffer* buffer_entry = arena_request_memory(arena_sock, sizeof(struct buffer));
            if (buffer_entry == NULL) {
                fprintf(stderr, 
                    "ERROR: couldn't allocate memory for buffer entry structure. Error: %s\n", 
                    strerror(errno));
                return NULL;
            }

            buffer_entry->next = NULL;

            gettimeofday(&(buffer_entry->time), NULL);

            buffer_entry->value = arena_request_memory(arena_sock, sizeof(char) * (strlen(log_message + 1)));
            if (buffer_entry->value == NULL) {
                fprintf(stderr, 
                    "ERROR: couldn't allocate memory for buffer entry structure. Error: %s\n", 
                    strerror(errno));
                return NULL;
            }
            
            strcpy(buffer_entry->value, log_message);
            buffer_entry->valuelen = strlen(log_message);

            // we are passing false, although arg is arena allocated because freeing it is more complicated, should be done elsewhere
            if (request_thread_from_pool(produce_buffer_entry, free_buffer_entry, 
                NULL, (void*)buffer_entry, ONCE, 0, false, -1) == -3) {
                    fprintf(stderr, 
                        "ERROR: no thread is available for log writer\n");
            } 

#ifdef DEBUG
            printf("requested to produce message\n");
#endif
        }
    }
    
    return NULL;
}

void* thread_close_socket(void* sockfd) {
    if (sockfd == NULL) {
        fprintf(stderr, "ERROR: pointer to socket descriptor is NULL\n");
        return NULL;
    }

    int error = close_socket(*((int*)sockfd), SOCKET_CLOSE_MAXTRIES);

    if (error != 0) {
        printf("Couldn't close (thread) socket file descriptor\n");
    }

    pthread_mutex_lock(&(arena_sock_mutex));
    arena_free_memory(arena_sock, sockfd);
    pthread_mutex_unlock(&(arena_sock_mutex));

    return NULL;
}

int thread_pool_accept_conn_socket(int sockfd) {
    if (sockfd == -1) {
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
        return -1;
    }

    struct sockaddr addr = {0};
    socklen_t addrlen = 0;

    if (!is_thread_pool_spawned()) {
        int error = spawn_thread_pool(MAX_SOCK_THREADS);
        if (error != 0) {
            fprintf(stderr, "ERROR: couldn't spawn thread pool\n");
            return -1;
        }
    }

    arena_sock = arena_allocate(1000);
    if (arena_sock == -1) {
        fprintf(stderr, "ERROR: couldn't create new arena\n");
        return -1;
    }

    while (1) {
        int new_sockfd = accept(sockfd, &addr, &addrlen);

#ifdef DEBUG
        printf("Got new socket descriptor: %d\n", new_sockfd);
#endif

        if (new_sockfd == -1) {
            fprintf(stderr, 
                "ERROR: invalid socket descriptor from accept. Error %s\n", 
                strerror(errno));
            if (errno == EBADF) { // connection was probably torn down
                return -1;
            }
            continue;
        }

        int error = 0;
        int timeout_counter = 0;
        pthread_mutex_lock(&(arena_sock_mutex));
        int *new_sockfd_heap = (int*)arena_request_memory(arena_sock, sizeof(int));
        pthread_mutex_unlock(&(arena_sock_mutex));
        
        if (new_sockfd_heap == NULL) {
            fprintf(stderr, "ERROR: couldn't allocate memory for socket fd\n");
            return -1;
        }

        *new_sockfd_heap = new_sockfd;

#ifdef DEBUG
        printf("I'm not here\n");
#endif

        while ((error = request_thread_from_pool(thread_read_socket, thread_close_socket, NULL, 
                                                (void*)new_sockfd_heap, FOREVER, 0, true, arena_sock)) != 0) {
// #ifdef DEBUG
            printf("ERROR: no threads available, will try again in %dms\n", THREAD_TIMEOUT_TIMER);
// #endif
            usleep(THREAD_TIMEOUT_TIMER);
            timeout_counter++;
            if (timeout_counter > THREAD_TIMEOUT_COUNTER) {
                fprintf(stderr,
                    "ERROR: couldn't find a thread for the available file descriptor after trying %d times\n", 
                    THREAD_TIMEOUT_COUNTER);
                write(*new_sockfd_heap, BUSY_MSG, sizeof(BUSY_MSG));
                if (close(*new_sockfd_heap) != 0) {
                    fprintf(stderr, 
                        "ERROR: couldn't close accepted socket fd. Error: %s\n", 
                        strerror(errno));
                } else {
                    printf("Closed socket %d\n", *new_sockfd_heap);
                }
                arena_free_memory(arena_sock, new_sockfd_heap);
                break;
            }
        }

        printf("Why am I here\n");
    }

    return 0;
}

int accept_conn_socket(int sockfd) {
    if (sockfd == -1) {
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
        return -1;
    }

    struct sockaddr addr = {0};
    socklen_t addrlen = 0;

    printf("I am on accept\n");
    int new_sockfd = accept(sockfd, &addr, &addrlen);

    char hostname[NI_MAXHOST], servname[NI_MAXSERV];
    char data[MAX_MSG_SIZE];

    getnameinfo(&addr, addrlen, hostname, sizeof hostname, 
        servname, sizeof servname, NI_NUMERICHOST | NI_NUMERICSERV);

    int byte_count = 0;
    while (!((byte_count = read(new_sockfd, data, MAX_MSG_SIZE)) == 0)) {
#ifdef DEBUG
        printf("Got message of size %d bytes from %s:%s: %s\n", byte_count, hostname, servname, data);
#endif
    }

    return 0;
}

