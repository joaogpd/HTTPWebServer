#ifndef STATS_FILE_HANDLER
#define STATS_FILE_HANDLER

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef enum {
    IMAGE_JPEG = 0, 
    IMAGE_PNG = 1, 
    TEXT_HTML = 2, 
    TEXT_CSS = 3, 
    TEXT_PLAIN = 4
} FileType;

typedef struct stats_message {
    FileType type;
    struct stats_message *next;
} StatsMessage;

extern pthread_mutex_t stats_buffer_mutex;
extern struct stats_message *stats_buffer;

void produce_stats_message(FileType type);
void show_stats(char *stats_filename);

#endif