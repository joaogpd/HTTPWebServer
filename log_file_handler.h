#ifndef LOG_FILE_HANDLER_H
#define LOG_FILE_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

typedef struct log_message {
    char *timestamped_message;
    size_t message_len;
    struct log_message *next;
} LogMessage;

extern pthread_mutex_t log_buffer_mutex;
extern pthread_cond_t new_log_data;
extern struct log_message *log_buffer;
extern pthread_t log_file_writer_id;

extern FILE *log_file;

void* log_file_writer(void* log_filename);
void* log_message_producer(void* msg);

#endif