#include "response_file_handler.h"

FileType get_file_type(char *extension) {
    if (strcmp("jpg", extension) == 0 || strcmp("jpeg", extension) == 0) {
        return IMAGE_JPEG;
    } else if (strcmp("webp", extension) == 0) {
        return IMAGE_WEBP;
    } else if (strcmp("png", extension) == 0) {
        return IMAGE_PNG;
    }  else if (strcmp("tiff", extension) == 0) {
        return IMAGE_TIFF;
    }  else if (strcmp("gif", extension) == 0) {
        return IMAGE_GIF;
    } else if (strcmp("ico", extension) == 0) {
        return IMAGE_ICO;
    } else if (strcmp("svg", extension) == 0) {
        return IMAGE_SVG;
    }  else if (strcmp("mp4", extension) == 0) {
        return VIDEO_MP4;
    } else if (strcmp("html", extension) == 0) {
        return TEXT_HTML;
    } else if (strcmp("css", extension) == 0) {
        return TEXT_CSS;
    } else if (strcmp("js", extension) == 0) {
        return TEXT_JAVASCRIPT;
    } else if (strcmp("csv", extension) == 0) {
        return TEXT_CSV;
    } else if (strcmp("xml", extension) == 0) {
        return TEXT_XML;
    } else if (strcmp("pdf", extension) == 0) { // unknown extension
        return APPLICATION_PDF;
    } else {
        return TEXT_PLAIN;
    }
}