#include "response_file_handler.h"
#include <stdio.h>

struct file_response *get_file_content(char *path) {
    // call stat to get file attributes
    char *full_path = (char*)malloc(sizeof(char) * (strlen(path) + strlen(application_context->root_path) + 1));
    if (full_path == NULL) {
        fprintf(stderr,"ERROR: couldn't allocate memory for full file path");
        return NULL;
    }
    
    strcpy(full_path, "");
    strcat(full_path, application_context->root_path);
    strcat(full_path, path);

    struct stat statbuf;
    memset(&statbuf, 0, sizeof(struct stat));

    if (stat(full_path, &statbuf) != 0) {
        fprintf(stderr, "ERROR: couldn't get information on file.\n");
        free(full_path);
        return NULL;
    }

    char *file_content = (char*)malloc(sizeof(char) * (statbuf.st_size + 1));
    if (file_content == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for file content.\n");
        
        free(full_path);
       
        return NULL;
    }

    memset(file_content, 0, statbuf.st_size + 1);

    FILE* file = fopen(full_path, "r");
    if (file == NULL) {
        fprintf(stderr, "ERROR: couldn't open requested file for reading. Error: %s\n", strerror(errno));
        
        free(full_path);
        
        return NULL;
    }

    if (fread(file_content, sizeof(char), statbuf.st_size, file) != statbuf.st_size) {
        fprintf(stderr, "ERROR: couldn't read the entire file contents.\n");
       
        free(full_path);
        fclose(file);
       
        return NULL;
    }

    struct file_response *response = (struct file_response*)malloc(sizeof(struct file_response));
    if (response == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for file response.\n");
        
        free(full_path);
        free(file_content);
        
        return NULL;
    }

    response->file_content = file_content; 
    response->file_size = statbuf.st_size;

    // file_content[statbuf.st_size] = '\0';

    free(full_path);
    fclose(file);

    return response;
}