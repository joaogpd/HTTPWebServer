#include "sock/sock.h"
#include <stdlib.h>
#include <signal.h>

#define PORT "8214"

void close_socket(int sockfd) {
    close(sockfd);
    exit(EXIT_SUCCESS);
}

int main(void) {
    signal(SIGINT, close_socket);

    int sockfd = create_TCP_socket(AF_INET);

    struct host host;
    strcpy(host.address, "");
    strcpy(host.port, "8214");

    struct sockaddr* sockaddr = getsockaddr_from_host(&host);

    bind_socket(sockfd, (struct sockaddr_storage*)sockaddr);

    listen_on_socket(sockfd);

    select_read_socket(1, &sockfd);

    while (1);

    return 0;
}