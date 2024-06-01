#include "terminate.h"

void terminate(int sig) {
    close_clients();
    free_log_buffer();

    if (server_sockfd != -1) {
        if (close(server_sockfd) != 0) {
            fprintf(stderr, "FATAL ERROR: couldn't close server socket. Error: %s\n", strerror(errno));
        }
    }

    if (application_context->stats_filename != NULL) {
        show_stats(application_context->stats_filename);
    }   

    free_application_context();

    terminate_log_file_writer();
    if (log_file != NULL) {
        fclose(log_file);
    }

    _exit(EXIT_SUCCESS);
}
