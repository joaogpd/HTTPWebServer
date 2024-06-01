#include "server.h"

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