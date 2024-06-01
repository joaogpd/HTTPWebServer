#include "stats_file_handler.h"

void produce_stats_message(FileType type) {
    struct stats_message *message = (struct stats_message*)malloc(sizeof(struct stats_message));
    if (message == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for stats message\n");
        return;
    }

    message->type = type;
    
    pthread_mutex_lock(&stats_buffer_mutex);

    message->next = stats_buffer;
    stats_buffer = message;

    pthread_mutex_unlock(&stats_buffer_mutex);
}