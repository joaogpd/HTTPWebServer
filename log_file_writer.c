#include "log_file_handler.h"

void* log_file_writer(void* log_filename) {
    log_file_writer_id = pthread_self();

    log_file = fopen((char*)log_filename, "a");
    if (log_file == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't open log file. Error: %s.\n", strerror(errno));
        log_file_writer_id = -1;
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&log_buffer_mutex);
        pthread_cond_wait(&new_log_data, &log_buffer_mutex);
        pthread_testcancel();

        struct log_message *message = log_buffer;
        while (message != NULL) {
            struct log_message *temp = message->next;
            
            fwrite(message->timestamped_message, sizeof(char), message->message_len, log_file);
            fwrite("\n", sizeof(char), 1, log_file);   
    
            free(message->timestamped_message);
            free(message);
            message = temp;

            log_buffer = message;
        }

        fflush(log_file);

        pthread_mutex_unlock(&log_buffer_mutex);
    }

    return NULL;
}