#include "clients.h"

pthread_mutex_t connected_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
struct client *connected_clients = NULL;