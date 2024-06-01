#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include "args.h"
#include "log_file_handler.h"
#include "stats_file_handler.h"
#include "clients.h"
#include "server.h"

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

int main(int argc, char *argv[]) {
    signal(SIGINT, terminate);

    // parse command line arguments
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't parse args\n");
        return 1;
    }

    if (application_context->log_filename != NULL) {
        pthread_create(&log_file_writer_id, NULL, log_file_writer, application_context->log_filename);
    }

    printf("Server started. Exit with 'kill -SIGUSR1 <pid>'.\n");

    start_server(application_context->port, AF_INET);

    terminate(0);

    return 0;
}
