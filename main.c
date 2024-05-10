#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // getopt_long
#include <getopt.h> // struct option
#include <signal.h>
#include <sys/types.h>

int exit_status = EXIT_SUCCESS;

#define LOG_FILE_ACCESS_MODE "a"
#define STATS_FILE_ACCESS_MODE "w"
#define SHOW_PROPER_PROGRAM_USAGE() printf("Usage: %s -p --port <port> [-l --log <filename>] [-s --statistics <filename>] [-b --background] [-r --root <path>]\n", argv[0])
#define GRACEFUL_EXIT(status) exit_status = status; kill(getpid(), SIGUSR1);

typedef struct execution_context {
    char* port_number;
    FILE* log_file;
    FILE* stats_file;
    bool is_background;
    char* root_path;
} ExecutionContext;

ExecutionContext* global_context = NULL;

void free_execution_context(void) {
    if (global_context != NULL) {
        // can free NULL pointers with no side effects
        free(global_context->port_number);
        free(global_context->root_path);

        // should not close NULL file pointers, causes undefined behaviour (at best)
        if (global_context->log_file != NULL) fclose(global_context->log_file);
        if (global_context->stats_file != NULL) fclose(global_context->stats_file);

        free(global_context);
    }
}

void terminate_service(int sig) {
    printf("Service is being terminated...\n");

    free_execution_context();
    
    exit(exit_status);
}

FILE* open_file(char* filename, char* mode) {
    if (filename == NULL || mode == NULL) {
        fprintf(stderr, "Fatal error, cannot open file with NULL arguments\n");
        GRACEFUL_EXIT(EXIT_FAILURE);
    }
    FILE* file = fopen(filename, mode);
    if (file == NULL) {
        fprintf(stderr, "Fatal error, FILE* opened was NULL\n");
        GRACEFUL_EXIT(EXIT_FAILURE);
    }
    return file;
}

void parse_arguments_into_global_context(int argc, char *argv[]) {
    if (argc <= 1) {
        SHOW_PROPER_PROGRAM_USAGE();
        GRACEFUL_EXIT(EXIT_FAILURE);
    }

    // using 'calloc' so everything starts zeroed
    global_context = (ExecutionContext*)calloc(1, sizeof(ExecutionContext)); 

    char* optstring = "p:l:s:br:"; // only background has no argument (and others are all required)

    // structure for long options. all return the identifiers for the short option.
    struct option longopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "log", required_argument, NULL, 'l' },
        { "statistics", required_argument, NULL, 's' },
        { "background", no_argument, NULL, 'b' },
        { "root", required_argument, NULL, 'r' }
    };

    char option = (char)-1;
    while ((option = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (option) {
            case 'p':
                global_context->port_number = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(global_context->port_number, optarg);
                printf("Port: %s\n", global_context->port_number);
                if (atoi(optarg) == 0) { // check whether port is a number
                    fprintf(stderr, "Fatal error, 'port' argument is not a numeric value\n");
                    GRACEFUL_EXIT(EXIT_FAILURE);
                }
                break;
            case 'l':
                global_context->log_file = open_file(optarg, LOG_FILE_ACCESS_MODE);
                printf("Log file: %s\n", optarg);
                break;
            case 's':
                global_context->stats_file = open_file(optarg, STATS_FILE_ACCESS_MODE);
                printf("Stats file: %s\n", optarg);
                break;
            case 'b':
                printf("Background\n");
                global_context->is_background = true;
                break;
            case 'r':
                global_context->root_path = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(global_context->root_path, optarg);
                printf("Root path: %s\n", global_context->root_path);
                break;
            case '?':
                SHOW_PROPER_PROGRAM_USAGE();
                GRACEFUL_EXIT(EXIT_FAILURE);
            case ':':
                SHOW_PROPER_PROGRAM_USAGE();
                GRACEFUL_EXIT(EXIT_FAILURE);
            default:
                SHOW_PROPER_PROGRAM_USAGE();
                GRACEFUL_EXIT(EXIT_FAILURE);
        }
    }

    if (optind <= 1) {
        SHOW_PROPER_PROGRAM_USAGE();
        GRACEFUL_EXIT(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // SIGUSR1 should terminate the service
    signal(SIGUSR1, terminate_service);
    signal(SIGINT, terminate_service);

    parse_arguments_into_global_context(argc, argv);

    while (1);

    return 0;
}