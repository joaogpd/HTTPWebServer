#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

int create_tcp_socket(int domain) {
    int socket_fd = socket(domain, SOCK_STREAM, 0)

    if (socket_fd == -1) {
        fprintf(stderr, "Error, couldn't open socket\n");
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}

struct addrinfo* get_addr(char* host, char* port, int protocol) {
    // getaddrinfo is reentrant and allows programs to eliminate IPv4-versus IPv6 dependencies.
    struct addrinfo *response; // this will receive a linked list with all the found addresses
    // this is used to narrow down what will be returned, all fields are initialized with 0 explicitly
    struct addrinfo hints = {0};

    if (protocol != AF_INET && protocol != AF_INET6) {
        fprintf(stderr, "Error, protocol is not IPv4 or IPv6\n");
        exit(EXIT_FAILURE);
    }

    hints.ai_family = protocol; // can be either IPv4 or IPv6, AF_INET or AF_INET6
    hints.ai_socktype = SOCK_STREAM // TCP socket
    hints.ai_protocol = 0; // don't care

    getaddrinfo(host, port, &hints, &response);

    if (response == NULL) {
        fprintf(stderr, "Error, couldn't get any addrinfo response\n");
        exit(EXIT_FAILURE);
    }

    return response;
}

int main(void) {
    int socket_fd = create_socket(AF_INET);



    return 0;
}