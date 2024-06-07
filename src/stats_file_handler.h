#ifndef STATS_FILE_HANDLER
#define STATS_FILE_HANDLER

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "response_file_handler.h"

typedef struct stats_message {
    FileType type;
    struct stats_message *next;
} StatsMessage;

extern pthread_mutex_t stats_buffer_mutex;
extern struct stats_message *stats_buffer;

void produce_stats_message(FileType type);
void show_stats(char *stats_filename);

#endif