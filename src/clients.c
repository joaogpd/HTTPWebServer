#include "clients.h"

pthread_mutex_t connected_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
struct client *connected_clients = NULL;


void remove_client(int sockfd) {
    pthread_mutex_lock(&connected_clients_mutex);

    struct client* client = connected_clients;

    while (client != NULL) {
        if (client->sockfd == sockfd) {
            break;
        }
        client = client->next;
    }    

    if (client != NULL) {
        close(client->sockfd);
    }

    pthread_mutex_unlock(&connected_clients_mutex);
}


int insert_client(int sockfd) {
    struct client* client = (struct client*)malloc(sizeof(struct client));
    if (client == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for client structure.\n");
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