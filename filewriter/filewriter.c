#include "filewriter.h"

struct stats* stats_buffer = NULL;
struct buffer* log_buffer = NULL;

arena_t filewriter_arena = -1;

FILE* log_file = NULL;
char* stats_filename = NULL;

pthread_mutex_t log_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_buffer_cond = PTHREAD_COND_INITIALIZER;

pthread_t log_writer_id = -1;

pthread_mutex_t log_writer_id_mutex = PTHREAD_MUTEX_INITIALIZER;

bool log_writer_terminate = false;

pthread_mutex_t log_writer_terminate_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE* open_file(char* name, char* mode) {
    FILE* file = fopen(name, mode);
    if (file == NULL) {
        fprintf(stderr, "ERROR: Couldn't open file %s with mode %s. Error: %s\n", name, mode, strerror(errno));
        return NULL;
    }

    return file;
}

int filewriter_init(char* log_filename, char* stats_filename) {
    filewriter_arena = arena_allocate(4096);

    if (filewriter_arena == -1) {
        fprintf(stderr, "ERROR: couldn't initialize arena for filewriter\n");
        return -1;
    }

    if (log_filename != NULL) {
        log_file = open_file(log_filename, "a");
        if (log_file == NULL) {
            arena_deallocate(filewriter_arena);
            return -1;
        }
    }

    if (stats_filename != NULL) {
        stats_filename = (char*)arena_request_memory(filewriter_arena, sizeof(char) * (strlen(stats_filename) + 1));
        if (stats_filename == NULL) {
            fprintf(stderr, "ERROR: couldn't allocate memory for stats_filename\n");
            fclose(log_file);
            arena_deallocate(filewriter_arena);
            return -1;
        }
    }

    return 0;
}

void filewriter_cleanup(void) {
    pthread_mutex_lock(&log_writer_terminate_mutex);
    log_writer_terminate = true;
    pthread_mutex_unlock(&log_writer_terminate_mutex);

    pthread_mutex_lock(&log_buffer_mutex);
    pthread_cond_signal(&log_buffer_cond);
    pthread_mutex_unlock(&log_buffer_mutex);

    if (log_file != NULL) {
        fclose(log_file);
    }
}

void* set_write_log_buffer_id(void* arg) {
    pthread_mutex_lock(&log_writer_id_mutex);
    log_writer_id = pthread_self();
    pthread_mutex_unlock(&log_writer_id_mutex);
    
    return NULL;
}

void* write_log_buffer(void* arg) {
    if (log_file == NULL) {
        fprintf(stderr, "ERROR: log file hasn't been opened\n");
        return NULL;
    }

    pthread_mutex_lock(&log_buffer_mutex);
    pthread_cond_wait(&log_buffer_cond, &log_buffer_mutex);
    pthread_mutex_unlock(&log_buffer_mutex);
    pthread_mutex_lock(&log_writer_terminate_mutex);
    if (log_writer_terminate == true) {
        pthread_mutex_unlock(&log_writer_terminate_mutex);
        return (void*)-1;
    }
    pthread_mutex_unlock(&log_writer_terminate_mutex);
    pthread_mutex_lock(&log_buffer_mutex);
    while (log_buffer != NULL) {
        struct buffer* temp = log_buffer->next;
        fwrite(log_buffer->value, sizeof(char), log_buffer->valuelen, log_file);
        arena_free_memory(filewriter_arena, log_buffer->value);
        arena_free_memory(filewriter_arena, log_buffer);
        log_buffer = temp;
    }
    if (fflush(log_file) != 0) {
        fprintf(stderr, "ERROR: couldn't flush log file. Error: %s\n", strerror(errno));
    }
    pthread_mutex_unlock(&log_buffer_mutex);

    return NULL;
}

void* produce_buffer_entry(void* arg) {
    struct buffer* buffer_entry_arg = (struct buffer*)arg;
    size_t buffer_value_len = strlen(buffer_entry_arg->value);

    struct buffer* buffer_entry = (struct buffer*)arena_request_memory(filewriter_arena, sizeof(struct buffer));
    if (buffer_entry == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for buffer entry\n");
        return NULL;
    }

    buffer_entry->value = (char*)arena_request_memory(filewriter_arena,
         sizeof(char) * (buffer_value_len + 1));

    if (buffer_entry->value == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for buffer entry value\n");
        arena_free_memory(filewriter_arena, buffer_entry);
        return NULL;
    }
    buffer_entry->valuelen = buffer_value_len;
    strcpy(buffer_entry->value, buffer_entry_arg->value);
    buffer_entry->time = buffer_entry_arg->time;
    buffer_entry->next = NULL;

    pthread_mutex_lock(&log_buffer_mutex);
    buffer_entry->next = log_buffer;
    log_buffer = buffer_entry;
    pthread_cond_signal(&log_buffer_cond);
    pthread_mutex_unlock(&log_buffer_mutex);

    return NULL;
}