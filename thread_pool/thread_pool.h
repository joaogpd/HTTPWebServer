#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "../arena/arena.h"

#define DEBUG

typedef void*(*thread_task_t)(void*);

typedef enum {
    IDLE = 0, EXECUTING = 1
} thread_state_t;

typedef enum {
    NONE, ONCE, N_TIMES, FOREVER
} execution_mode_t;

struct thread {
    thread_task_t task;
    thread_task_t cleanup;
    thread_task_t prelude;
    thread_state_t state;
    pthread_t id;
    pthread_cond_t task_ready_cond;
    pthread_mutex_t thread_mutex;
    pthread_mutex_t terminated_mutex;
    bool terminated; // signals the thread it should finish
    execution_mode_t mode;
    int mode_counter;
    void* arg;
    bool arena_allocated_arg;
    arena_t arena_id_arg;
    struct thread* next;
};

struct thread_queue {
    struct thread* first;
    struct thread* last;
};

int spawn_thread_pool(int nthreads);
bool is_thread_pool_spawned(void);
int request_thread_from_pool(thread_task_t task, thread_task_t cleanup, thread_task_t prelude, 
                            void* arg, execution_mode_t mode, int mode_counter, 
                            bool arena_allocated_arg, arena_t arena_id_arg);
void teardown_thread_pool(void);

#endif