#include "clients.h"

void close_clients(void) {
    pthread_mutex_lock(&connected_clients_mutex);

    struct client* client = connected_clients;
    while (client != NULL) {
        struct client* temp = client->next;

        pthread_cancel(client->thread_id);

        int error = 0;
        if ((error = pthread_join(client->thread_id, NULL)) != 0) {
            fprintf(stderr, "ERROR: couldn't join thread. Error: %d.\n", error);
        }

        close(client->sockfd);
        free(client);

        client = temp;
    }

    pthread_mutex_unlock(&connected_clients_mutex);
}