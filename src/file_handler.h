#ifndef LOG_FILE_HANDLER_H
#define LOG_FILE_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include "args.h"

typedef enum {
    IMAGE_JPEG = 0, 
    IMAGE_PNG = 1, 
    IMAGE_WEBP,
    IMAGE_TIFF,
    IMAGE_GIF,
    IMAGE_SVG,
    IMAGE_ICO,
    IMAGE_AVIF,
    VIDEO_MP4,
    TEXT_HTML, 
    TEXT_CSS, 
    TEXT_PLAIN,
    TEXT_JAVASCRIPT,
    TEXT_CSV,
    TEXT_XML,
    APPLICATION_PDF
} FileType;

typedef struct log_message {
    char *timestamped_message;
    size_t message_len;
    struct log_message *next;
} LogMessage;

typedef struct stats_message {
    FileType type;
    struct stats_message *next;
} StatsMessage;

typedef struct file_response {
    char *file_content;
    size_t file_size;
} FileResponse;

extern pthread_mutex_t log_buffer_mutex;
extern pthread_cond_t new_log_data;
extern struct log_message *log_buffer;
extern pthread_t log_file_writer_id;

extern pthread_mutex_t stats_buffer_mutex;
extern struct stats_message *stats_buffer;

extern FILE *log_file;

extern char content_type_array[][30];

extern char http_404_response_pt1[];
extern char http_404_response_pt2[];

extern char http_ok_response_pt1[];
extern char http_ok_response_pt2[];
extern char http_ok_response_pt3[];
extern char http_ok_response_pt4[];

void* log_file_writer(void* log_filename);
void* log_message_producer(void* msg);
void free_log_buffer(void);
void terminate_log_file_writer(void);
void produce_stats_message(FileType type);
void show_stats(char *stats_filename);
char* get_file_path(char *request);
struct file_response *get_file_content(char *path);
FileType get_file_type(char *extension);

#endif