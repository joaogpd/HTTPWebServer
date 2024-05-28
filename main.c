#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#define SHOW_PROPER_USAGE(program) \
    printf( \
        "Usage: %s -p --port <port> [-l --log <filename>] [-s --statistics <filename>] [-b --background] [-r --root <path>]\n", \
        program)

typedef struct context {
    char *port;
    char *log_filename;
    char *stats_filename;
    char *root_path;
    bool background;
} Context;

struct context* application_context = NULL;

void free_application_context(void) {
    if (application_context != NULL) {
        free(application_context->port);
        free(application_context->log_filename);
        free(application_context->stats_filename);
        free(application_context->root_path);
    }
    free(application_context);
}

/**
 * This function parses command line arguments into a global structure
 * that contains the application context.
 */
int parse_args(int argc, char *argv[]) {
    char *optstring = "p:l:s:br:";

    struct option long_options[] = {
        {"port", required_argument, NULL, 'p'},
        {"log", required_argument, NULL, 'l'},
        {"statistics", required_argument, NULL, 's'},
        {"background", no_argument, NULL, 'b'},
        {"root", required_argument, NULL, 'r'}
    };

    application_context = (struct context*)malloc(sizeof(struct context));
    if (application_context == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for application context\n");
        return -1;
    }

    application_context->background = false;
    application_context->port = NULL;
    application_context->log_filename = NULL;
    application_context->stats_filename = NULL;
    application_context->root_path = NULL;

    char option = (char)-1;
    while ((option = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (option) {
            case 'p':
                application_context->port = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->port, optarg);
                break;
            case 'l':
                application_context->log_filename = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->log_filename, optarg);
                break;
            case 's':
                application_context->stats_filename = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->stats_filename, optarg);
                break;
            case 'b':
                application_context->background = true;
                break;
            case 'r':
                application_context->root_path = (char*)malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(application_context->root_path, optarg);
                break;
            case '?':
                fprintf(stderr, "ERROR: unknown or missing argument value\n"); 
                SHOW_PROPER_USAGE(argv[0]);

                free_application_context();

                return -1;
            default:
                fprintf(stderr, "ERROR: unknown or missing argument value\n");
                SHOW_PROPER_USAGE(argv[0]);

                free_application_context();

                return -1;
        }
    }

    if (application_context->port == NULL) {
        fprintf(stderr, "ERROR: port argument is mandatory\n");
        free_application_context();
        return -1;
    }

    if (application_context->root_path == NULL) {
        fprintf(stderr, "ERROR: path argument is mandatory\n");
        free_application_context();
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    // parse command line arguments
    if (parse_args(argc, argv) != 0) {
        fprintf(stderr, "ERROR: couldn't parse args\n");
        return 1;
    }

    // need to keep path name and stats file name throughout execution, but log 
    // can be freed after opening the file



    return 0;
}