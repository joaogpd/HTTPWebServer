#ifndef APPLICATION_CONTEXT_H
#define APPLICATION_CONTEXT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

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

extern struct context *application_context; 

void free_application_context(void);
int parse_args(int argc, char *argv[]);

#endif