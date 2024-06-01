#include "stats_file_handler.h"

void show_stats(char* stats_filename) {
    if (stats_filename == NULL) {
        fprintf(stderr, "ERROR: no statistics filename was provided (this function should not have been called)\n");
        return;
    }

    FILE* stats_file = fopen(stats_filename, "w");
    if (stats_file == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't open statistics file. Error: %s\n", strerror(errno));
        return;
    }

    int html_counter = 0, css_counter = 0, jpg_counter = 0, png_counter = 0, plain_counter = 0;

    pthread_mutex_lock(&stats_buffer_mutex);

    struct stats_message *message = stats_buffer;
    while (message != NULL) {
        struct stats_message *temp = message->next;

        switch (message->type) {
            case TEXT_HTML:
                html_counter++;
                break;
            case TEXT_CSS:
                css_counter++;
                break;
            case TEXT_PLAIN:
                plain_counter++;
                break;
            case IMAGE_JPEG:
                jpg_counter++;
                break;
            case IMAGE_PNG:
                png_counter++;
                break;
            default:
                fprintf(stderr, "ERROR: invalid option for statistics\n");
                break;
        }

        free(message);

        message = temp;
        stats_buffer = message;
    }

    pthread_mutex_unlock(&stats_buffer_mutex);

    fprintf(stats_file, 
        "HTML files: %d\nCSS files: %d\nPlain files: %d\nJPG/JPEG files: %d\nPNG files: %d\n",
         html_counter, css_counter, plain_counter, jpg_counter, png_counter);

    fclose(stats_file);
}