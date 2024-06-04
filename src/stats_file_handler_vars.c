#include "stats_file_handler.h"

pthread_mutex_t stats_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct stats_message *stats_buffer = NULL;
