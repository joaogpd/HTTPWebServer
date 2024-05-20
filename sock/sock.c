#include "sock.h"
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/select.h>

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

struct sockaddr* getsockaddr_from_host(struct host* host) {
    if (host == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: host argument was NULL.\n");
#endif
        return NULL;
    }

    struct addrinfo hints = {0};
    struct addrinfo* response;
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

    return response->ai_addr;
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
            "ERROR: couldn't start listening on socket. Erorr: %s\n",
            strerror(errno));
#endif
        return -1;
    }

    return 0;
}

static void* thread_accept_conn_socket(void* sockfd) {
    if (sockfd == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: sockfd argument is NULL\n");
#endif
        return NULL;
    }

    accept_conn_socket(*((int*)sockfd));

    return NULL;
}

int select_read_socket(int nreadfds, int *readfds) {
    fd_set fd_set_readfds;

    FD_ZERO(&fd_set_readfds);

    for (int i = 0; i < nreadfds; i++) {
        FD_SET(readfds[i], &fd_set_readfds);
    }

    int max_fds = nreadfds + 1;

    int error = select(max_fds, &fd_set_readfds, NULL, NULL, NULL);

    if (error == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't wait on select on sockets. Error: %s\n", strerror(errno));
#endif
        return -1;
    }

    if (!is_thread_pool_spawned()) {
        spawn_thread_pool(MAX_SOCK_THREADS);
    }

    for (int i = 0; i < nreadfds; i++) {
        if (FD_ISSET(readfds[i], &fd_set_readfds) != 0) {
            int error = 0;
            int timeout_counter = 0;
            while ((error = request_thread_from_pool(thread_accept_conn_socket, (void*)(readfds + i))) != 0) {
#ifdef DEBUG
                printf("ERROR: no threads available, will try again in %dms\n", THREAD_TIMEOUT_TIMER);
#endif
                usleep(THREAD_TIMEOUT_TIMER);
                timeout_counter++;
                if (timeout_counter > THREAD_TIMEOUT_COUNTER) {
#ifdef DEBUG
                    printf("ERROR: couldn't find a thread for the available file descriptor after trying %d times\n", THREAD_TIMEOUT_COUNTER);
#endif
                    break;
                }
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

    int new_sockfd = accept(sockfd, &addr, &addrlen);

    char hostname[NI_MAXHOST], servname[NI_MAXSERV];
    char data[MAX_MSG_SIZE];

    getnameinfo(&addr, addrlen, hostname, sizeof hostname, 
        servname, sizeof servname, NI_NUMERICHOST | NI_NUMERICSERV);

    int byte_count = 0;
    while (!((byte_count = read(new_sockfd, data, MAX_MSG_SIZE)) == 0)) {
#ifdef DEBUG
        printf("Got message of size %d bytes from %s:%s: %s", byte_count, hostname, servname, data);
#endif
    }

    return 0;
}

