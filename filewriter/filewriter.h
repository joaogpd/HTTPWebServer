#ifndef FILEWRITER_H
#define FILEWRITER_H

#undef DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "../arena/arena.h"

struct buffer {
    char* value;
    struct timeval time;
    size_t valuelen;
    struct buffer* next;
};

struct stats {
    char* filename;
    struct timeval time;
    struct stats* next;
};

FILE* open_file(char* name, char* mode);
// Initialize the filewriter module by opening the appropriate files.
int filewriter_init(char* log_filename, char* stats_filename);
// Cleanup module.
void filewriter_cleanup(void);
// Write all the statistics collected throughout execution to a file.
void write_stats_file(void);
// Produce a stats buffer entry.
void* produce_stats_entry(void* arg);
// Initialize id of thread responsible for writing to buffer.
void* set_write_log_buffer_id(void* arg);
// Write to log file from buffer.
void* write_log_buffer(void* arg);
// Produce a log buffer entry.
void* produce_buffer_entry(void* arg);

#endif