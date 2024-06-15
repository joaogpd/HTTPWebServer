#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include "file_handler.h"
#include "server.h"

int main(int argc, char *argv[]) {
    signal(SIGUSR1, terminate);

    // parse command line arguments
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't parse args.\n");

        terminate(EXIT_FAILURE);
    }

    if (application_context->background) {
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "FATAL ERROR: couldn't fork process. Error: %s\n", strerror(errno));
            
            terminate(EXIT_FAILURE);
        } else if (pid != 0) {
            terminate(SIGUSR1); // will exit gracefully
        }
    }

    if (application_context->log_filename != NULL) {
        pthread_create(&log_file_writer_id, NULL, log_file_writer, application_context->log_filename);
    }

    printf("Server started. Exit with 'kill -SIGUSR1 %d'.\n", getpid());

    int error = start_server(application_context->port, AF_INET);

    if (error == -1) {
        fprintf(stderr, "FATAL ERROR: start server should never return.\n");
    }

    terminate(EXIT_FAILURE);

    return 0;
}
