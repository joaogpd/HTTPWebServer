#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // getopt_long
#include <getopt.h> // struct option
#include <signal.h>

typedef struct execution_context {
    uint port;
    char log_file[100];
    char stats_file[100];
    bool background;
    char root_path[500];
} ExecutionContext;

void print_execution_context(ExecutionContext* context) {
    if (context == NULL) {
        printf("Error, 'context' was NULL.\n");
    }
    printf("Porta: %d\n", context->port);
    printf("Arquivo de log: %s\n", context->log_file);
    printf("Arquivo de estatísticas: %s\n", context->stats_file);
    printf("Background: %d\n", context->background);
    printf("Caminho da raiz: %s\n", context->root_path);
}

void terminate_service(int sig) {
    printf("Service is being terminated...\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // SIGUSR1 should terminate the service
    signal(SIGUSR1, terminate_service);

    // only the port is a required argument
    char *opstring = "p:l:s:br:";
    // structure for long options. all return the identifiers for the short option.
    struct option longopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "log", required_argument, NULL, 'l' },
        { "statistics", required_argument, NULL, 's' },
        { "background", no_argument, NULL, 'b' },
        { "root", required_argument, NULL, 'r' }
    };

    ExecutionContext context = {0};

    if (argc <= 1) {
        printf("Usage: %s -p <port> [-l <file>] [-s <file>] [-b] [-r <path>]\n", argv[0]);
    }
    
    char option = (char)-1;
    while ((option = getopt_long(argc, argv, opstring, longopts, NULL)) != -1) {
        switch (option) {
            case 'p':
                printf("Port\n");
                // handle non numeric argument
                context.port = atoi(optarg);
                if (context.port == 0) {
                    fprintf(stderr, "Error, 'port' argument was not numeric value\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                printf("Log\n");
                strcpy(context.log_file, optarg);
                break;
            case 's':
                printf("Statistics\n");
                strcpy(context.stats_file, optarg);
                break;
            case 'b':
                printf("Background\n");
                context.background = true;
                break;
            case 'r':
                printf("Root\n");
                strcpy(context.root_path, optarg);
                break;
            case '?':
                printf("Usage: %s -p <port> [-l <file>] [-s <file>] [-b] [-r <path>]\n", argv[0]);
                exit(EXIT_FAILURE);
            case ':':
                printf("Usage: %s -p <port> [-l <file>] [-s <file>] [-b] [-r <path>]\n", argv[0]);
                exit(EXIT_FAILURE);
            case (char)-1:
                // finished parsing
                printf("Finished parsing\n");
                break;
            default:
                printf("Usage: %s -p --port <port> [-l --log <filename>] [-s --statistics <filename>] [-b --background] [-r -root <path>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    print_execution_context(&context);

    while (1);
    
    return 0;
}