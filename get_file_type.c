#include "response_file_handler.h"

FileType get_file_type(char *extension) {
    if (strcmp("jpg", extension) == 0 || strcmp("jpeg", extension) == 0) {
        return IMAGE_JPEG;
    } else if (strcmp("png", extension) == 0) {
        return IMAGE_PNG;
    } else if (strcmp("html", extension) == 0) {
        return TEXT_HTML;
    } else if (strcmp("css", extension) == 0) {
        return TEXT_CSS;
    } else if (strcmp("pdf", extension) == 0) { // unknown extension
        return APPLICATION_PDF;
    } else {
        return TEXT_PLAIN;
    }
}