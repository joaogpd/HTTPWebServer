#include "response_file_handler.h"

char* get_file_path(char *request) {
    if (request == NULL) {
        fprintf(stderr, "ERROR: file path was NULL\n");
        return NULL;
    }

    char current_char;
    current_char = *request;

    char *expected_request = "GET";

    while (current_char != ' ') {
        if (current_char != *expected_request) {
            fprintf(stderr, "ERROR: malformed request\n");
            return NULL;
        }
        request++;
        expected_request++;
        current_char = *request;
    }

    request++;
    char *file_path_start = request;
    int counter = 0;

    current_char = *request;

    while (current_char !=  ' ') {
        request++;
        expected_request++;
        current_char = *request;
        counter++;
    }

    char *file_path = (char*)malloc(sizeof(char) * (counter + 1));
    if (file_path == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for file path\n");
        return NULL;
    }

    int up_counter = 0;

    while (counter--) {
        file_path[up_counter] = file_path_start[up_counter];
        up_counter++;
    }

    file_path[up_counter] = '\0';

    return file_path;
}