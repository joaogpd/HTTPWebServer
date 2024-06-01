#include "clients.h"

int insert_client(int sockfd) {
    struct client* client = (struct client*)malloc(sizeof(struct client));
    if (client == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for client structure\n");
        return -1;
    }

    client->sockfd = sockfd;
    client->thread_id = pthread_self();
    pthread_mutex_lock(&connected_clients_mutex);
    client->next = connected_clients;

    connected_clients = client;

    pthread_mutex_unlock(&connected_clients_mutex);

    return 0;
}