#include "clients.h"

void remove_client(int sockfd) {
    pthread_mutex_lock(&connected_clients_mutex);

    struct client* prev = NULL;
    struct client* client = connected_clients;

    while (client != NULL) {
        if (client->sockfd == sockfd) {
            break;
        }
        prev = client;
        client = client->next;
    }    

    if (client != NULL) {
        close(client->sockfd);
    }

    pthread_mutex_unlock(&connected_clients_mutex);
}
