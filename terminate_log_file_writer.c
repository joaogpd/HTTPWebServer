#include "log_file_handler.h"

void terminate_log_file_writer(void) {
    if (log_file_writer_id != -1) {
        pthread_cancel(log_file_writer_id);
        int error = 0;
        if ((error = pthread_join(log_file_writer_id, NULL)) != 0) {
            fprintf(stderr, "ERROR: couldn't join thread. Error: %d.\n", error);
        }
    }
}