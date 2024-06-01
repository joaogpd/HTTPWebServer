#include "server.h"

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