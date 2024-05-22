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
#ifdef DEBUG
	    fprintf(stderr, "ERROR: domain not supported\n");
#endif
	    return -1;
    }

    // create TCP socket
    int sock_fd = socket(domain, SOCK_STREAM, IPPROTO_TCP);

    if (sock_fd == -1) {
#ifdef DEBUG
	    fprintf(stderr, 
            "ERROR: socket could not be opened. Error: %s\n",
            strerror(errno));
#endif
    }

    return sock_fd;
}

// need to handle SIGPIPE

int connect_socket(int sockfd, struct sockaddr_storage *addr) {
    if (sockfd == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
#endif
        return -1;
    }

    // TODO

    return 0;
}

int bind_socket(int sockfd, struct sockaddr_storage *addr) {
    if (addr == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: addr argument was NULL\n");
#endif
        return -1;
    }

    socklen_t sockaddr_len = (addr->ss_family == AF_INET) 
        ? sizeof(struct sockaddr_in) 
        : sizeof(struct sockaddr_in6);

    int error = bind(sockfd, (struct sockaddr*)addr, sockaddr_len);

    if (error == -1) {
#ifdef DEBUG
        fprintf(stderr, 
            "ERROR: couldn't bind socket to host. Error: %s\n",
             strerror(errno));
#endif
        return -1;
    }

    return 0;
}

struct addrinfo* getsockaddr_from_host(struct host* host) {
    if (host == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: host argument was NULL.\n");
#endif
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
#ifdef DEBUG
        if (error == EAI_SYSTEM) {
            fprintf(stderr, 
            "ERROR: couldn't get socket address from host. 'gai' error: %s. Error: %s\n", 
                    gai_strerror(error),
                    strerror(errno));
        } else {
            fprintf(stderr, 
            "ERROR: couldn't get socket address from host. 'getaddrinfo' error: %s.", 
                    gai_strerror(error));
        }
#endif
        return NULL;
    }

    return response;
}

int listen_on_socket(int sockfd) {
    if (sockfd == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
#endif
        return -1;
    }

    int error = listen(sockfd, LISTEN_BACKLOG);

    if (error != 0) {
#ifdef DEBUG
        fprintf(stderr, 
            "ERROR: couldn't start listening on socket. Error: %s\n",
            strerror(errno));
#endif
        return -1;
    }

    return 0;
}

// static void* thread_accept_conn_socket(void* sockfd) {
//     if (sockfd == NULL) {
// #ifdef DEBUG
//         fprintf(stderr, "ERROR: sockfd argument is NULL\n");
// #endif
//         return NULL;
//     }

//     accept_conn_socket(*((int*)sockfd));

//     if (close_socket(*((int*)sockfd), 10) != 0) {
// #ifdef DEBUG
//         fprintf(stderr, "ERROR: couldn't close socket fd\n");
// #endif
//     }

//     return NULL;
// }

int close_socket(int sockfd, int maxtries) {
    int error = 0;
    int counter = 0;
    while ((error = close(sockfd)) != 0) {
#ifdef DEBUG
        fprintf(stderr,
         "ERROR: couldn't close socket. Error: %s\n", 
         strerror(errno));
#endif
        counter++;
        if (counter >= maxtries) {
            return -1;
        }
    }

    return 0;
}

// int select_read_socket(int nreadfds, int *readfds) {
//     fd_set fd_set_readfds;

//     FD_ZERO(&fd_set_readfds);

//     for (int i = 0; i < nreadfds; i++) {
//         FD_SET(readfds[i], &fd_set_readfds);
//     }

//     int max_fds = nreadfds + 1;

//     printf("I am waiting on select\n");
//     int error = select(max_fds, &fd_set_readfds, NULL, NULL, NULL);
//     printf("I got past select\n");

//     if (error == -1) {
// #ifdef DEBUG
//         fprintf(stderr, "ERROR: couldn't wait on select on sockets. Error: %s\n", strerror(errno));
// #endif
//         return -1;
//     }

//     if (!is_thread_pool_spawned()) {
//         spawn_thread_pool(MAX_SOCK_THREADS);
//     }

//     for (int i = 0; i < nreadfds; i++) {
//         if (FD_ISSET(readfds[i], &fd_set_readfds) != 0) {
//             int error = 0;
//             int timeout_counter = 0;
//             while ((error = request_thread_from_pool(thread_accept_conn_socket, (void*)(readfds + i))) != 0) {
// #ifdef DEBUG
//                 printf("ERROR: no threads available, will try again in %dms\n", THREAD_TIMEOUT_TIMER);
// #endif
//                 usleep(THREAD_TIMEOUT_TIMER);
//                 timeout_counter++;
//                 if (timeout_counter > THREAD_TIMEOUT_COUNTER) {
// #ifdef DEBUG
//                     printf(
//                         "ERROR: couldn't find a thread for the available file descriptor after trying %d times\n", 
//                          THREAD_TIMEOUT_COUNTER);
// #endif
//                     break;
//                 }
//             }
//         }
//     }

//     return 0;
// }

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
    timeout.tv_usec = 200000; // wait for 200ms

    int error = select(sockfd_int + 1, &readfds, &writefds, &exceptfds, &timeout);

    if (error > 0) {
        if (FD_ISSET(sockfd_int, &readfds)) {
            int byte_count = 0;
            byte_count = read(sockfd_int, data, MAX_MSG_SIZE);

#ifdef DEBUG
            printf("past read\n");
#endif

            // connection ended
            if (byte_count <= 0) { 
                return (void*)-1;
            }

#ifdef DEBUG
            printf("Got message of size %d bytes from %s:%s: %s\n", byte_count, hostname, servname, data);
#endif
        }
    }
    
    return NULL;
}

void* thread_close_socket(void* sockfd) {
    if (sockfd == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: pointer to socket descriptor is NULL\n");
#endif
        return NULL;
    }

    int error = close_socket(*((int*)sockfd), 10);

    if (error != 0) {
#ifdef DEBUG
        printf("Couldn't close (thread) socket file descriptor\n");
#endif
    }

    pthread_mutex_lock(&(arena_sock_mutex));
    arena_free_memory(arena_sock, sockfd);
    pthread_mutex_unlock(&(arena_sock_mutex));

    return NULL;
}

int thread_pool_accept_conn_socket(int sockfd) {
    if (sockfd == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
#endif
        return -1;
    }

    struct sockaddr addr = {0};
    socklen_t addrlen = 0;

    if (!is_thread_pool_spawned()) {
        int error = spawn_thread_pool(MAX_SOCK_THREADS);
        if (error != 0) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't spawn thread pool\n");
#endif
            return -1;
        }
    }

    arena_sock = arena_allocate(1000);
    if (arena_sock == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't create new arena\n");
#endif
        return -1;
    }

    while (1) {
        // printf("I am on accept\n");
        // accept4 to atomically make sockfd non blocking
        int new_sockfd = accept(sockfd, &addr, &addrlen);
        // printf("I got past fucking accept\n");

        if (new_sockfd == -1) {
#ifdef DEBUG
            fprintf(stderr, 
                "ERROR: invalid socket descriptor from accept. Error %s\n", 
                strerror(errno));
#endif
            if (errno == EBADF) { // connection was probably torn down
                return -1;
            }
            continue;
        }

//         if (fcntl(new_sockfd, F_SETFL, O_NONBLOCK) != 0) {
// #ifdef DEBUG
//             fprintf(stderr,
//                  "ERROR: couldn't set socket fd to non blocking mode: Error: %s\n", 
//                  strerror(errno));
// #endif
//             return -1;
//         }

        int error = 0;
        int timeout_counter = 0;
        pthread_mutex_lock(&(arena_sock_mutex));
        int *new_sockfd_heap = (int*)arena_request_memory(arena_sock, sizeof(int));
        pthread_mutex_unlock(&(arena_sock_mutex));
        
        if (new_sockfd_heap == NULL) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't allocate memory for socket fd\n");
#endif
            return -1;
        }

        *new_sockfd_heap = new_sockfd;

#ifdef DEBUG
        printf("I'm not here\n");
#endif

        while ((error = request_thread_from_pool(thread_read_socket, thread_close_socket, NULL, 
                                                (void*)new_sockfd_heap, FOREVER, 0, true, arena_sock) != 0)) {
#ifdef DEBUG
            printf("ERROR: no threads available, will try again in %dms\n", THREAD_TIMEOUT_TIMER);
#endif
            usleep(THREAD_TIMEOUT_TIMER);
            timeout_counter++;
            if (timeout_counter > THREAD_TIMEOUT_COUNTER) {
#ifdef DEBUG
                printf(
                    "ERROR: couldn't find a thread for the available file descriptor after trying %d times\n", 
                    THREAD_TIMEOUT_COUNTER);
#endif
                break;
            }
        }
    }

    return 0;
}

int accept_conn_socket(int sockfd) {
    if (sockfd == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: socket descriptor is invalid\n");
#endif
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

