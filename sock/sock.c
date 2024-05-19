#include "sock.h"
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

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

    int error = getaddrinfo(host->address, host->port, &hints, &response);

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

    char data[MAX_MSG_SIZE];

    int byte_count = 0;
    while (!((byte_count = read(new_sockfd, data, MAX_MSG_SIZE)) == 0)) {
#ifdef DEBUG
        printf("Got message of size %d bytes from %s:%s: %s", byte_count, addr_in.sin_addr, addr_in.sin_port, data);
#endif
    }

    return 0;
}

