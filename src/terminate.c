#include "terminate.h"
#include "args.h"

void terminate(int sig) {
    close_clients();
    free_log_buffer();

    if (server_sockfd != -1) {
        if (close(server_sockfd) != 0) {
            fprintf(stderr, "ERROR: couldn't close server socket. Error: %s.\n", strerror(errno));
        }
    }

    if (application_context != NULL) {
        if (application_context->stats_filename != NULL) {
            show_stats(application_context->stats_filename);
        }   
    }

    free_application_context();

    terminate_log_file_writer();
    if (log_file != NULL) {
        fclose(log_file);
    }

    if (sig == EXIT_FAILURE) {
        _exit(EXIT_FAILURE);
    }
    _exit(EXIT_SUCCESS);
}
