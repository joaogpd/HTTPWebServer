#include "log_file_handler.h"

void* log_message_producer(void* msg) {
    struct log_message *message = (struct log_message*)malloc(sizeof(struct log_message));
    if (message == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for log message. Error: %s.\n", strerror(errno));    
        return NULL;
    }

    message->timestamped_message = (char*)malloc(sizeof(char) * (strlen(msg) + 1));
    if (message->timestamped_message == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for timestamped_message. Error: %s.\n", strerror(errno));   
        free(message); 
        return NULL;
    }

    strcpy(message->timestamped_message, (char*)msg);

    message->message_len = strlen((char*)msg); 

    pthread_mutex_lock(&log_buffer_mutex);

    message->next = log_buffer;
    log_buffer = message;

    pthread_cond_signal(&new_log_data);
    pthread_mutex_unlock(&log_buffer_mutex);

    return NULL;
}