#include "args.h"

struct context *application_context = NULL; 

void free_application_context(void) {
    if (application_context != NULL) {
        free(application_context->port);
        free(application_context->log_filename);
        free(application_context->stats_filename);
        free(application_context->root_path);
    }

    free(application_context);
    
    application_context = NULL;
}