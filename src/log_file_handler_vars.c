#include "log_file_handler.h"

pthread_mutex_t log_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_log_data = PTHREAD_COND_INITIALIZER;
struct log_message *log_buffer = NULL;
pthread_t log_file_writer_id = -1;

FILE *log_file = NULL;