#include "response_file_handler.h"
#include "stats_file_handler.h"

void show_stats(char* stats_filename) {
    if (stats_filename == NULL) {
        fprintf(stderr, "FATAL ERROR: no statistics filename was provided.\n");
        return;
    }

    FILE* stats_file = fopen(stats_filename, "w");
    if (stats_file == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't open statistics file. Error: %s.\n", strerror(errno));
        return;
    }

    int html_counter = 0, css_counter = 0, js_counter = 0, csv_counter = 0, xml_counter = 0,
        jpg_counter = 0, png_counter = 0, gif_counter = 0, ico_counter = 0, tiff_counter = 0,
        svg_counter = 0, mp4_counter = 0, plain_counter = 0, pdf_counter = 0;

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
            case TEXT_JAVASCRIPT:
                js_counter++;
                break;
            case TEXT_CSV:
                csv_counter++;
                break;
            case TEXT_XML:
                xml_counter++;
                break;
            case IMAGE_JPEG:
                jpg_counter++;
                break;
            case IMAGE_PNG:
                png_counter++;
                break;
            case IMAGE_GIF:
                gif_counter++;
                break;
            case IMAGE_ICO:
                ico_counter++;
                break;
            case IMAGE_TIFF:
                tiff_counter++;
                break;
            case IMAGE_SVG:
                svg_counter++;
                break;
            case VIDEO_MP4:
                mp4_counter++;
                break;
            case APPLICATION_PDF:
                pdf_counter++;
                break;
            default:
                fprintf(stderr, "ERROR: invalid option for statistics.\n");
                break;
        }

        free(message);

        message = temp;
        stats_buffer = message;
    }

    pthread_mutex_unlock(&stats_buffer_mutex);

    fprintf(stats_file, 
        "HTML files: %d\nCSS files: %d\nJS files: %d\nPlain files: %d\n\
CSV files: %d\nXML files:%d\nJPG/JPEG files: %d\nPNG files: %d\nPDF files: %d\n\
TIFF files: %d\nGIF files: %d\nSVG files: %d\nICO files: %d\nMP4 files: %d\n",
         html_counter, css_counter, js_counter, plain_counter, csv_counter, 
         xml_counter, jpg_counter, png_counter, pdf_counter,
         tiff_counter, gif_counter, svg_counter, ico_counter, mp4_counter);

    fclose(stats_file);
}