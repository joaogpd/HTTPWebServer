#include "file_handler.h"

pthread_mutex_t log_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_log_data = PTHREAD_COND_INITIALIZER;
struct log_message *log_buffer = NULL;
pthread_t log_file_writer_id = -1;

FILE *log_file = NULL;

pthread_mutex_t stats_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct stats_message *stats_buffer = NULL;


char content_type_array[][30] = {
    "image/jpeg", 
    "image/png", 
    "image/webp",
    "image/tiff",
    "image/gif",
    "image/svg+xml",
    "image/x-icon",
    "image/avif",
    "video/mp4",
    "text/html", 
    "text/css", 
    "text/plain",
    "text/javascript",
    "text/csv",
    "text/xml",
    "application/pdf"
};

char http_404_response_pt1[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: 174\r\nExpires: -1\r\nDate: ";
char http_404_response_pt2[] = "\r\nConnection: close\r\n\r\n<!doctype html><html><head><meta http-equiv='content-type' content='text/html;charset=utf-8'><title>404 Not Found</title></head><body>Arquivo nao foi encontrado</body></html>";

char http_ok_response_pt1[] = "HTTP/1.1 200 OK\r\nContent-Type: ";
char http_ok_response_pt2[] = "; charset=UTF-8\r\nContent-Length: ";
char http_ok_response_pt3[] = "\r\nExpires: -1\r\nDate: ";
char http_ok_response_pt4[] = "\r\nConnection: keep-alive\r\n\r\n";

void* log_file_writer(void* log_filename) {
    log_file_writer_id = pthread_self();

    log_file = fopen((char*)log_filename, "a");
    if (log_file == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't open log file. Error: %s.\n", strerror(errno));
        log_file_writer_id = -1;
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&log_buffer_mutex);
        pthread_cond_wait(&new_log_data, &log_buffer_mutex);
        pthread_testcancel();

        struct log_message *message = log_buffer;
        while (message != NULL) {
            struct log_message *temp = message->next;
            
            fwrite(message->timestamped_message, sizeof(char), message->message_len, log_file);
            fwrite("\n", sizeof(char), 1, log_file);   
    
            free(message->timestamped_message);
            free(message);
            message = temp;

            log_buffer = message;
        }

        fflush(log_file);

        pthread_mutex_unlock(&log_buffer_mutex);
    }

    return NULL;
}

void* log_message_producer(void* msg) {
    struct log_message *message = (struct log_message*)malloc(sizeof(struct log_message));
    if (message == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for log message. Error: %s.\n", strerror(errno));    
        return NULL;
    }

    message->timestamped_message = (char*)malloc(sizeof(char) * (strlen(msg) + 1));
    if (message->timestamped_message == NULL) {
        fprintf(stderr, "FATAL ERROR: couldn't allocate memory for timestamped_message. Error: %s.\n", strerror(errno));   
        free(message); 
        return NULL;
    }

    strcpy(message->timestamped_message, (char*)msg);

    message->message_len = strlen((char*)msg); 

    pthread_mutex_lock(&log_buffer_mutex);

    message->next = log_buffer;
    log_buffer = message;

    pthread_cond_signal(&new_log_data);
    pthread_mutex_unlock(&log_buffer_mutex);

    return NULL;
}

void free_log_buffer(void) {
    pthread_mutex_lock(&log_buffer_mutex);

    struct log_message *message = log_buffer;
    while (message != NULL) {
        struct log_message *temp = message->next;

        free(message->timestamped_message);
        free(message);

        message = temp; 
    }

    pthread_mutex_unlock(&log_buffer_mutex);
}

void terminate_log_file_writer(void) {
    if (log_file_writer_id != -1) {
        pthread_cancel(log_file_writer_id);
        int error = 0;
        if ((error = pthread_join(log_file_writer_id, NULL)) != 0) {
            fprintf(stderr, "ERROR: couldn't join thread. Error: %d.\n", error);
        }
    }
}

void produce_stats_message(FileType type) {
    struct stats_message *message = (struct stats_message*)malloc(sizeof(struct stats_message));
    if (message == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for stats message\n");
        return;
    }

    message->type = type;
    
    pthread_mutex_lock(&stats_buffer_mutex);

    message->next = stats_buffer;
    stats_buffer = message;

    pthread_mutex_unlock(&stats_buffer_mutex);
}

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
        svg_counter = 0, mp4_counter = 0, plain_counter = 0, pdf_counter = 0, avif_counter = 0;

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
            case IMAGE_AVIF:
                avif_counter++;
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
CSV files: %d\nXML files: %d\nJPG/JPEG files: %d\nPNG files: %d\nAVIF files: %d\nPDF files: %d\n\
TIFF files: %d\nGIF files: %d\nSVG files: %d\nICO files: %d\nMP4 files: %d\n",
         html_counter, css_counter, js_counter, plain_counter, csv_counter, 
         xml_counter, jpg_counter, png_counter, avif_counter, pdf_counter,
         tiff_counter, gif_counter, svg_counter, ico_counter, mp4_counter);

    fclose(stats_file);
}

char* get_file_path(char *request) {
    if (request == NULL) {
        fprintf(stderr, "ERROR: file path was NULL.\n");
        return NULL;
    }

    char current_char;
    current_char = *request;

    char *expected_request = "GET";

    while (current_char != ' ') {
        if (current_char != *expected_request) {
            fprintf(stderr, "ERROR: malformed request.\n");
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
        fprintf(stderr, "ERROR: couldn't allocate memory for file path.\n");
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

struct file_response *get_file_content(char *path, char *root_path) {
    // call stat to get file attributes
    char *full_path = (char*)malloc(sizeof(char) * (strlen(path) + strlen(root_path) + 1));
    if (full_path == NULL) {
        fprintf(stderr,"ERROR: couldn't allocate memory for full file path");
        return NULL;
    }
    
    strcpy(full_path, "");
    strcat(full_path, root_path);
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
    } else if (strcmp("avif", extension) == 0) {
        return IMAGE_AVIF;
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