#include "sock/sock.h"
#include "thread_pool/thread_pool.h"
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>

#define PORT "8224"

int sockfd = -1;

void terminate(int sig) {
    close_socket(sockfd, 10);
    teardown_thread_pool();
    _exit(EXIT_SUCCESS); // exit isn't async-signal safe
}

int main(void) {
    signal(SIGINT, terminate);

    sockfd = create_TCP_socket(AF_INET);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

    struct host host;
    strcpy(host.address, "");
    strcpy(host.port, PORT);
    host.domain = AF_INET;

    struct addrinfo* addrinfo = getsockaddr_from_host(&host);
    struct sockaddr* sockaddr = addrinfo->ai_addr;

    int error = bind_socket(sockfd, (struct sockaddr_storage*)sockaddr);

    freeaddrinfo(addrinfo);

    if (error != 0) {
        close_socket(sockfd, 10);
        return EXIT_FAILURE;
    }

    listen_on_socket(sockfd);

    thread_pool_accept_conn_socket(sockfd);

    while (1);

    return 0;
}