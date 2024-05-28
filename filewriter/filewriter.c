#include "filewriter.h"
#include <linux/limits.h>

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

void* filewriter_cleanup(void* arg) {
    if (log_file == NULL) {
        reset_write_log_buffer_id();
        return NULL; // shouldn't wait for thread to terminate
    }

    pthread_mutex_lock(&log_writer_terminate_mutex);
    log_writer_terminate = true;
    pthread_mutex_unlock(&log_writer_terminate_mutex);

#ifdef DEBUG
    printf("cleanup 1\n");
#endif

    pthread_mutex_lock(&log_buffer_mutex);
    pthread_cond_signal(&log_buffer_cond);
    pthread_mutex_unlock(&log_buffer_mutex);

    int val = 0;
    pthread_mutex_lock(&log_writer_id_mutex);
    if (log_writer_id != -1) {
        if ((val = pthread_join(log_writer_id, NULL) != 0)) {
            fprintf(stderr, "ERROR: couldn't join log writer thread. Error: %d", val);
        }
    }
    pthread_mutex_unlock(&log_writer_id_mutex);

#ifdef DEBUG
    printf("cleanup 2\n");
#endif

    if (log_file != NULL) {
        if (fclose(log_file) != 0) {
            fprintf(stderr, "ERROR: couldn't close log file. Error: %s\n", strerror(errno));
        }
    }

#ifdef DEBUG
    printf("cleanup 3\n");
#endif

    return NULL;
}

void* set_write_log_buffer_id(void* arg) {
    pthread_mutex_lock(&log_writer_id_mutex);
    log_writer_id = pthread_self();
    pthread_mutex_unlock(&log_writer_id_mutex);

    printf("My id is: %ld\n", pthread_self());
    
    return NULL;
}

void* write_log_buffer(void* arg) {
    if (log_file == NULL) {
        fprintf(stderr, "ERROR: log file hasn't been opened\n");
        return (void*)-1; // since no log file has been opened, we can free up thread
    }

    pthread_mutex_lock(&log_buffer_mutex);
#ifdef DEBUG
    printf("write wait\n");
#endif
    pthread_cond_wait(&log_buffer_cond, &log_buffer_mutex);
#ifdef DEBUG
    printf("past wait\n");
#endif
    pthread_mutex_unlock(&log_buffer_mutex);
    pthread_mutex_lock(&log_writer_terminate_mutex);
    if (log_writer_terminate == true) {
        pthread_mutex_unlock(&log_writer_terminate_mutex);
#ifdef DEBUG
        printf("should terminate\n");
#endif
        return (void*)-2; // request immediate termination
    }
    pthread_mutex_unlock(&log_writer_terminate_mutex);
    pthread_mutex_lock(&log_buffer_mutex);
    while (log_buffer != NULL) {
        struct buffer* temp = log_buffer->next;

        if (fwrite(log_buffer->value, sizeof(char), log_buffer->valuelen, log_file) != log_buffer->valuelen) {
            fprintf(stderr, "ERROR: couldn't write the entire %ld bytes to log file. Error: %s", log_buffer->valuelen, strerror(errno));
            break;
        }

        char time[TIME_MAX_SIZE] = {0};
        sprintf(time, ". At time: %ld\n", log_buffer->time.tv_sec);

        if (fwrite(time, sizeof(char), strlen(time), log_file) != strlen(time)) {
            fprintf(stderr, "ERROR: couldn't write time to log file. Error: %s", strerror(errno));
            break;
        }

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

void reset_write_log_buffer_id(void) {
    pthread_mutex_lock(&log_writer_id_mutex);
    log_writer_id = -1;
    pthread_mutex_unlock(&log_writer_id_mutex);
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