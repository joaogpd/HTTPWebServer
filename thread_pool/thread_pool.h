#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "../arena/arena.h"

#define DEBUG

typedef void*(*thread_task_t)(void*);

struct thread_queue_node {
    thread_task_t task;
    pthread_t id;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    void* arg;
    struct thread_queue_node* next;
};

struct thread_queue {
    struct thread_queue_node* first;
    struct thread_queue_node* last;
    int max_threads;
};

int spawn_thread_pool(int nthreads);
bool is_thread_pool_spawned(void);
int request_thread_from_pool(thread_task_t task, void* arg);
void teardown_thread_pool(void);

#endif