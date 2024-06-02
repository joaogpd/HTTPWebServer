#include "args.h"

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
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for application context.\n");
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
                fprintf(stderr, "ERROR: unknown or missing argument value.\n"); 
                SHOW_PROPER_USAGE(argv[0]);

                free_application_context();

                return -1;
            default:
                fprintf(stderr, "ERROR: unknown or missing argument value.\n");
                SHOW_PROPER_USAGE(argv[0]);

                free_application_context();

                return -1;
        }
    }

    if (application_context->port == NULL) {
        fprintf(stderr, "FATAL ERROR: port argument is mandatory.\n");
        SHOW_PROPER_USAGE(argv[0]);
        free_application_context();
        return -1;
    }

    if (application_context->root_path == NULL) {
        fprintf(stderr, "FATAL ERROR: path argument is mandatory.\n");
        SHOW_PROPER_USAGE(argv[0]);
        free_application_context();
        return -1;
    }

    return 0;
}