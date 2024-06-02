#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include "args.h"
#include "log_file_handler.h"
#include "server.h"
#include "terminate.h"

#define KEEP_FILE_DESCRIPTORS 1
#define KEEP_WORKING_DIRECTORY 1

int main(int argc, char *argv[]) {
    signal(SIGUSR1, terminate);

    // parse command line arguments
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't parse args.\n");
        terminate(0);
        return 1;
    }

    // send process to background (make it a daemon)
    if (application_context->background) {
        if (daemon(KEEP_FILE_DESCRIPTORS, KEEP_WORKING_DIRECTORY) != 0) {
            fprintf(stderr, 
                "FATAL ERROR: couldn't daemonize server. Error: %s\n", 
                strerror(errno));
            terminate(0);
            return 1;
        }
    }

    if (application_context->log_filename != NULL) {
        pthread_create(&log_file_writer_id, NULL, log_file_writer, application_context->log_filename);
    }

    printf("Server started. Exit with 'kill -SIGUSR1 <pid>'.\n");

    start_server(application_context->port, AF_INET);

    terminate(0);

    return 0;
}
