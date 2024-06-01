#ifndef APPLICATION_CONTEXT_H
#define APPLICATION_CONTEXT_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct context {
    char *port;
    char *log_filename;
    char *stats_filename;
    char *root_path;
    bool background;
} Context;

extern struct context *application_context; 

void free_application_context(void);

#endif