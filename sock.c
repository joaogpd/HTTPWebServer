#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int socket_fd_temp;

typedef union {
    struct sockaddr_in client_ipv4;
    struct sockaddr_in6 client_ipv6;
} sockaddr_unspec;

int create_tcp_socket(struct addrinfo* addr) {
    int socket_fd = socket(addr->ai_family, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        fprintf(stderr, "Error, couldn't open socket\n");
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}

struct addrinfo* get_addr(char* port, int family) {
    // getaddrinfo is reentrant and allows programs to eliminate IPv4-versus IPv6 dependencies.
    struct addrinfo *response; // this will receive a linked list with all the found addresses
    // this is used to narrow down what will be returned, all fields are initialized with 0 explicitly
    struct addrinfo hints = {0};

    if (family != AF_INET && family != AF_INET6) {
        fprintf(stderr, "Error, protocol is not IPv4 or IPv6\n");
        exit(EXIT_FAILURE);
    }

    // AI_PASSIVE is used to have a socket that is suitable for binding and will accept connections
    // AI_ADDRCONFIG is used to return an IPv4 or IPv6 address only if they have at least one address/add
    hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE; 
    hints.ai_family = family; // can be either AF_INET or AF_INET6
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_protocol = 0; // don't care

    getaddrinfo(NULL, port, &hints, &response);

    if (response == NULL) {
        fprintf(stderr, "Error, couldn't get any addrinfo response\n");
        exit(EXIT_FAILURE);
    }

    return response;
}

void assign_name_socket(int sockfd, struct addrinfo* addr) {
    // an operation of binding is traditionally called "assigning a name to a socket"
    if (bind(sockfd, addr->ai_addr, addr->ai_addrlen) != 0) {
        fprintf(stderr, "Error, couldn't bind socket to an address\n");
        exit(EXIT_FAILURE);
    }
}

void listen_on_socket(int sockfd) {
    if (listen(sockfd, 10) != 0) {
        fprintf(stderr, "Error, couldn't listen on the port\n");
        exit(EXIT_FAILURE);
    }
    puts("Starting the service, will be listening to connections on port 8080\n");
}

int accept_connection(int sockfd) {
    sockaddr_unspec accept_sockaddr;
    unsigned int accept_sockaddrlen;
    char hostname[NI_MAXHOST], hostaddr[NI_MAXHOST];

    accept_sockaddrlen = sizeof(sockaddr_unspec);
    int accept_fd = -1;
    if ((accept_fd = accept(sockfd, (struct sockaddr*)&accept_sockaddr, &accept_sockaddrlen)) == -1) {
        fprintf(stderr, "Error, couldn't accept connection\n");
    }

    getnameinfo((struct sockaddr*)&accept_sockaddr, accept_sockaddrlen, 
            hostname, sizeof hostname, NULL, 0, 0);
    getnameinfo((struct sockaddr*)&accept_sockaddr, accept_sockaddrlen, 
            hostaddr, sizeof hostaddr, NULL, 0, NI_NUMERICHOST);

    printf("Server connected to %s (%s)\n", hostname, hostaddr);

    return accept_fd;
}

void terminate_server(int signal) {
    if (close(socket_fd_temp) != 0) {
        printf("Error, couldn't close socket\n");
        exit(EXIT_SUCCESS);
    }
    exit(EXIT_SUCCESS);
}

int main(void) {
    signal(SIGINT, terminate_server);
    struct addrinfo* addr = get_addr("8080", AF_INET);

    int socket_fd = create_tcp_socket(addr);
    socket_fd_temp = socket_fd;
    assign_name_socket(socket_fd, addr);
    listen_on_socket(socket_fd);

    freeaddrinfo(addr);

    while (true) {
        int new_socket_fd = accept_connection(socket_fd);
        if (new_socket_fd != -1) {
            int byte_count = 0;
            char data[100];
            while ((byte_count = read(new_socket_fd, data, 100)) != 0) {
                printf("%s\n", data);
                memset(data, 0, 100);
            } 
        }
    }

    if (close(socket_fd) != 0) {
        printf("Error, couldn't close socket\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}