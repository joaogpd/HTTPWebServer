#include "log_file_handler.h"

void free_log_buffer(void) {
    pthread_mutex_lock(&log_buffer_mutex);

    struct log_message *message = log_buffer;
    while (message != NULL) {
        struct log_message *temp = message->next;

        free(message->timestamped_message);
        free(message);

        message = temp; 
    }

    pthread_mutex_unlock(&log_buffer_mutex);
}