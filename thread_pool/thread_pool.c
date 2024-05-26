#include "thread_pool.h"
#include <pthread.h>
#include <time.h>

pthread_mutex_t thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
struct thread_queue* thread_pool = NULL;

pthread_mutex_t arena_id_mutex = PTHREAD_MUTEX_INITIALIZER;
arena_t arena_id; // arena responsible for holding all the heap allocated values

// Finds the first idle thread from the thread pool and returns it.
// This should be called within a critical region for "thread_pool_mutex".
static struct thread *find_idle_thread(void) {
    struct thread *idle_thread = thread_pool->first;
    struct thread *prev_thread = NULL;

    if (idle_thread == NULL) {
        // this a fatal error and should never happen, as the thread pool can't be initiated with no threads
        fprintf(stderr, "ERROR: the thread pool seems to be empty\n");
        return NULL;
    }

    while (idle_thread != NULL) {
        if (idle_thread->state == IDLE) {
            break;
        }
        prev_thread = idle_thread;
        idle_thread = idle_thread->next;
    }

    if (idle_thread == NULL) {
#ifdef DEBUG
        printf("No idle thread was found\n");
#endif
        // pthread_mutex_unlock(&(thread_pool_mutex));
        return NULL; 
    }

    printf("idle: %p, prev: %p\n", idle_thread, prev_thread);

    // move thread to end of queue
    idle_thread->state = EXECUTING;
    if (prev_thread == NULL) { // first thread in queue, only change if not only node
        if (idle_thread != thread_pool->last) {
            thread_pool->first = idle_thread->next;
            idle_thread->next = NULL;
            thread_pool->last->next = idle_thread;
            thread_pool->last = idle_thread;
        }
    } else if (idle_thread != thread_pool->last) { 
        // move to back of queue, quicken search for idle, but only if not already there
        prev_thread->next = idle_thread->next;
        idle_thread->next = NULL;
        thread_pool->last->next = idle_thread;
        thread_pool->last = idle_thread;
    }

    // pthread_mutex_unlock(&(thread_pool_mutex));

    return idle_thread;
}

static void show_thread_pool(char* msg) {
#ifdef DEBUG
    printf("Starting show threads %s ----------\n", msg);
    for (struct thread* thread = thread_pool->first; thread != NULL; thread = thread->next) {
        printf("Showing thread %ld, state: %d\n", thread->id, thread->state);
    }
    printf("Finishing show threads ----------\n");
#endif
}

static void return_thread_to_idle(pthread_t id) {
    pthread_mutex_lock(&(thread_pool_mutex));

#ifdef DEBUG
    show_thread_pool("rt bf");
#endif

#ifdef DEBUG
    printf("Thread %ld has been made idle\n", id);
#endif

    for (struct thread* thread = thread_pool->first; thread != NULL; thread = thread->next) {
        pthread_mutex_lock(&(thread->thread_mutex));
        if (thread->id == id) {
            thread->state = IDLE;
            thread->arg = NULL;
            thread->task = NULL;
            thread->prelude = NULL;
            thread->cleanup = NULL;
            thread->mode = NONE;
            thread->mode_counter = 0;
            thread->arena_allocated_arg = false;
            thread->arena_id_arg = -1;
            pthread_mutex_unlock(&(thread->thread_mutex));
            break;
        }
        pthread_mutex_unlock(&(thread->thread_mutex));
    }

#ifdef DEBUG
    show_thread_pool("rt af");
#endif

    pthread_mutex_unlock(&(thread_pool_mutex));
};

// this should be called within a critical region for "thread_pool_mutex"
static void insert_thread_in_pool(struct thread* thread) {
    if (thread == NULL) {
        fprintf(stderr, "ERROR: inserted thread cannot be NULL\n");
        return;
    }

    if (thread_pool == NULL) {
        fprintf(stderr, "ERROR: thread pool hasn't been initialized yet\n");
        return;
    }

    // thread pool empty
    if (thread_pool->first == NULL) {
        thread_pool->first = thread_pool->last = thread;
    } else { 
        thread_pool->last->next = thread;
        thread_pool->last = thread;
    }
}

static void* new_thread_wait(void* arg) {
    struct thread* thread = (struct thread*)arg;

    while (1) {
        pthread_mutex_lock(&(thread->thread_mutex));
        pthread_cond_wait(&(thread->task_ready_cond), &(thread->thread_mutex));
        pthread_mutex_lock(&(thread->terminated_mutex));
        if (thread->terminated) {
            pthread_mutex_unlock(&(thread->terminated_mutex));
            pthread_mutex_unlock(&(thread->thread_mutex));
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&(thread->terminated_mutex));
        pthread_mutex_unlock(&(thread->thread_mutex));

        if (thread->prelude != NULL) {
            thread->prelude(thread->arg);
        }

        if (thread->task != NULL) {
            if (thread->mode == ONCE) {
                thread->task(thread->arg);
            } else if (thread->mode == FOREVER) {
                while (1) {
                    if (thread->task(thread->arg) == (void*)-1) {
                        // this means that program wants to free up thread 
                        if (thread->arena_allocated_arg) {
                            arena_free_memory(thread->arena_id_arg, arg);
                        }
                        break;
                    }

                    pthread_mutex_lock(&(thread->terminated_mutex));
                    if (thread->terminated) {
#ifdef DEBUG
                        printf("Will perform cleanup\n");
#endif
                        if (thread->cleanup != NULL) {
                            thread->cleanup(thread->arg);
                        }
                        pthread_mutex_unlock(&(thread->terminated_mutex));
                        pthread_exit(NULL);
                    }
                    pthread_mutex_unlock(&(thread->terminated_mutex));
                }
            } else if (thread->mode == N_TIMES) {
                for (int counter = 0; counter < thread->mode_counter; counter++) {
                    if (thread->task(thread->arg) == (void*)-1) {
                        // this means that program wants to free up thread 
                        if (thread->arena_allocated_arg) {
                            arena_free_memory(thread->arena_id_arg, arg);
                        }
                        break;
                    }

                    pthread_mutex_lock(&(thread->terminated_mutex));
                    if (thread->terminated) {
#ifdef DEBUG
                        printf("Will perform cleanup\n");
#endif
                        if (thread->cleanup != NULL) {
                            thread->cleanup(thread->arg);
                        }
                        pthread_mutex_unlock(&(thread->terminated_mutex));
                        pthread_exit(NULL);
                    }
                    pthread_mutex_unlock(&(thread->terminated_mutex));
                }
            }
        }

        return_thread_to_idle(thread->id);
    }

    return NULL;
}

void teardown_thread_pool(void) {
#ifdef DEBUG
    printf("teardown 0\n");
#endif
    pthread_mutex_lock(&thread_pool_mutex);

    if (thread_pool == NULL) {
        fprintf(stderr, "ERROR: thread pool hasn't been initialized yet\n");
        return;
    }

#ifdef DEBUG
    printf("teardown 1\n");
#endif

    struct thread* thread = thread_pool->first;

    while (thread != NULL) {
#ifdef DEBUG
        printf("Thread %ld\n", thread->id);
#endif
        pthread_mutex_lock(&(thread->terminated_mutex));
        thread->terminated = true;
        pthread_mutex_unlock(&(thread->terminated_mutex));
        pthread_mutex_lock(&(thread->thread_mutex));
        pthread_cond_signal(&(thread->task_ready_cond));
        pthread_mutex_unlock(&(thread->thread_mutex));
        int val = 0;
        if ((val = pthread_join(thread->id, NULL)) != 0) { 
            fprintf(stderr, "ERROR: couldn't join %ld. Error: %d\n", thread->id, val);
        }

        thread = thread->next;
    }

#ifdef DEBUG
    printf("teardown 2\n");
#endif

    thread_pool = NULL;

    pthread_mutex_unlock(&thread_pool_mutex);

    pthread_mutex_lock(&arena_id_mutex);
    arena_deallocate(arena_id);
    pthread_mutex_unlock(&arena_id_mutex);
}

int spawn_thread_pool(int nthreads) {
    pthread_mutex_lock(&thread_pool_mutex);
    if (thread_pool != NULL) {
        // NON FATAL ERROR -> user can request thread instead
        pthread_mutex_unlock(&thread_pool_mutex);
#ifdef DEBUG
        printf("Thread pool has alread been spawned\n");
#endif
        return 1;
    }

    pthread_mutex_lock(&arena_id_mutex);
    // create enough bytes to hold linked list of nthreads nodes and one queue head, times 2
    long alloc_size = 2 * (nthreads * sizeof(struct thread) + sizeof(struct thread_queue));
    arena_id = arena_allocate(alloc_size);
    pthread_mutex_unlock(&arena_id_mutex);

    if (arena_id == -1) {
        fprintf(stderr, "ERROR: couldn't allocate arena for thread pool\n");
        pthread_mutex_unlock(&thread_pool_mutex);
        return -1;
    }

    thread_pool = arena_request_memory(arena_id, sizeof(struct thread_queue));

    if (thread_pool == NULL) {
        fprintf(stderr, "ERROR: couldn't allocate memory for thread queue structure\n");
        pthread_mutex_unlock(&thread_pool_mutex);
        arena_deallocate(arena_id);
        return -1;
    }

    thread_pool->first = thread_pool->last = NULL;

    for (int i = 0; i < nthreads; i++) {
        struct thread* new_thread = 
            (struct thread*)arena_request_memory(arena_id, sizeof(struct thread));

        if (new_thread == NULL) {
            fprintf(stderr, "ERROR: couldn't allocate memory for thread queue node structure\n");
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        int error = pthread_cond_init(&(new_thread->task_ready_cond), NULL);
        if (error != 0) {
            fprintf(stderr, "ERROR: couldn't initialize pthread_cond. Error: %s\n", strerror(errno));
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        error = pthread_mutex_init(&(new_thread->thread_mutex), NULL);
        if (error != 0) {
            fprintf(stderr, "ERROR: couldn't initialize thread mutex. Error: %s\n", strerror(errno));
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        error = pthread_mutex_init(&(new_thread->terminated_mutex), NULL);
        if (error != 0) {
            fprintf(stderr, "ERROR: couldn't initialize thread terminate mutex. Error: %s\n", strerror(errno));
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        new_thread->task = NULL;
        new_thread->cleanup = NULL;
        new_thread->prelude = NULL;
        new_thread->state = IDLE;
        new_thread->terminated = false;
        new_thread->mode = NONE;
        new_thread->mode_counter = -1;
        new_thread->arg = NULL;
        new_thread->arena_allocated_arg = false;
        new_thread->arena_id_arg = -1;
        new_thread->next = NULL;

        error = pthread_create(&(new_thread->id), NULL, new_thread_wait, (void*)new_thread);
        if (error != 0) {
            fprintf(stderr, "ERROR: couldn't create new thread. Error: %s\n", strerror(errno));
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        insert_thread_in_pool(new_thread);
    }

    pthread_mutex_unlock(&thread_pool_mutex);

    return 0;
}

int request_thread_from_pool(thread_task_t task, thread_task_t cleanup, thread_task_t prelude, 
                            void* arg, execution_mode_t mode, int mode_counter,
                            bool arena_allocated_arg, arena_t arena_id_arg) {
    pthread_mutex_lock(&thread_pool_mutex);
    if (thread_pool == NULL) {
        // NON FATAL ERROR -> user can spawn the thread pool instead -> can't be sent to buffer because no thread is available yet
        fprintf(stderr, "ERROR: thread pool hasn't been spawned yet\n"); 
        pthread_mutex_unlock(&thread_pool_mutex);
        return -2;
    }

#ifdef DEBUG
    show_thread_pool("fp bf");
#endif

    struct thread* thread = find_idle_thread();

#ifdef DEBUG
    show_thread_pool("fp af");
#endif

#ifdef DEBUG
    if (thread != NULL) {
        printf("Thread %p id: %ld\n", thread, thread->id);
    }
#endif

    if (thread == NULL) {
        // NON FATAL ERROR -> user can wait until a thread is available -> can't be sent to buffer because no thread is available yet
        fprintf(stderr, "ERROR: no thread is currently available\n");
        pthread_mutex_unlock(&thread_pool_mutex);
        return -3;
    }

    thread->arg = arg;
    thread->task = task;
    thread->cleanup = cleanup;
    thread->prelude = prelude;
    thread->mode = mode;
    thread->mode_counter = mode_counter;
    thread->arena_allocated_arg = arena_allocated_arg;
    thread->arena_id_arg = arena_id_arg;

    pthread_mutex_lock(&(thread->thread_mutex));
    pthread_cond_signal(&(thread->task_ready_cond));
    pthread_mutex_unlock(&(thread->thread_mutex));

    pthread_mutex_unlock(&thread_pool_mutex);

    return 0;
}

bool is_thread_pool_spawned(void) {
    pthread_mutex_lock(&thread_pool_mutex);
    bool status = (thread_pool != NULL);
    pthread_mutex_unlock(&thread_pool_mutex);

    return status;
}

