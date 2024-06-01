#ifndef RESPONSE_FILE_HANDLER_H
#define RESPONSE_FILE_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "args.h"

typedef enum {
    IMAGE_JPEG = 0, 
    IMAGE_PNG = 1, 
    TEXT_HTML = 2, 
    TEXT_CSS = 3, 
    TEXT_PLAIN = 4
} FileType;

typedef struct file_response {
    char *file_content;
    size_t file_size;
} FileResponse;

extern char content_type_array[][20];

extern char http_404_response[];

extern char http_ok_response_pt1[];
extern char http_ok_response_pt2[];
extern char http_ok_response_pt3[];

char* get_file_path(char *request);
struct file_response *get_file_content(char *path);
FileType get_file_type(char *extension);

#endif