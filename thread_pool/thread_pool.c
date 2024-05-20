#include "thread_pool.h"
#include <pthread.h>
#include <time.h>

pthread_mutex_t thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
struct thread_queue* thread_pool = NULL;

pthread_mutex_t arena_id_mutex = PTHREAD_MUTEX_INITIALIZER;
arena_t arena_id;

static void insert_new_thread(struct thread_queue_node* node) {
    if (!node->removed) {
        return;
    }
    node->removed = false;
    if (thread_pool->first == NULL) {
        thread_pool->first = thread_pool->last = node;
    } else {
        thread_pool->last->next = node;
        thread_pool->last = node;
    }
}

static struct thread_queue_node* remove_first_thread(void) {
    struct thread_queue_node* thread = thread_pool->first;

    if (thread_pool->first == thread_pool->last) {
        thread_pool->first = thread_pool->last = NULL;
    } else {
        thread_pool->first = thread->next;
    }

    thread->next = NULL;
    thread->removed = true;

    return thread;
}

static void return_thread_to_pool(struct thread_queue_node* thread) {
    insert_new_thread(thread);
}

static void* new_thread_wait(void* arg) {
    struct thread_queue_node* thread = (struct thread_queue_node*)arg;

    while (1) {
        pthread_mutex_lock(&(thread->mutex));
        pthread_cond_wait(&(thread->cond), &(thread->mutex));
        pthread_mutex_lock(&(thread->terminate_mutex));
        if (thread->terminate) {
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&(thread->terminate_mutex));
        pthread_mutex_unlock(&(thread->mutex));

        if (thread->task != NULL) {
            thread->task(thread->arg);
        }

        thread->task = NULL;
        thread->arg = NULL;

        return_thread_to_pool(thread);
    }

    return NULL;
}

void teardown_thread_pool(void) {
    pthread_mutex_lock(&thread_pool_mutex);

    if (thread_pool == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: thread pool hasn't been initialized yet\n");
#endif
        return;
    }

    struct thread_queue_node* thread = thread_pool->first;

    while (thread != NULL) {
        pthread_cond_signal(&(thread->cond));
        pthread_mutex_lock(&(thread->terminate_mutex));
        thread->terminate = true;
        pthread_mutex_unlock(&(thread->terminate_mutex));
        pthread_join(thread->id, NULL);

        thread = thread->next;
    }

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
        printf("Thread pool has alread been spawned\n");
        return 1;
    }

    pthread_mutex_lock(&arena_id_mutex);
    arena_id = arena_allocate(2 * (nthreads * sizeof(struct thread_queue_node) + sizeof(struct thread_queue)));
    pthread_mutex_unlock(&arena_id_mutex);

    if (arena_id == -1) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't allocate arena for thread pool\n");
#endif
        pthread_mutex_unlock(&thread_pool_mutex);
        return -1;
    }

    thread_pool = arena_request_memory(arena_id, sizeof(struct thread_queue));

    if (thread_pool == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't allocate memory for thread_queue structure\n");
#endif
        pthread_mutex_unlock(&thread_pool_mutex);
        arena_deallocate(arena_id);
        return -1;
    }

    thread_pool->first = thread_pool->last = NULL;
    thread_pool->max_threads = nthreads;

    for (int i = 0; i < nthreads; i++) {
        struct thread_queue_node* new_node = 
            (struct thread_queue_node*)arena_request_memory(arena_id, sizeof(struct thread_queue_node));

        if (new_node == NULL) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't allocate memory for thread_queue_node structure\n");
#endif
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        int error = pthread_cond_init(&(new_node->cond), NULL);
        if (error != 0) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't initialize pthread_cond. Error: %s\n", strerror(errno));
#endif
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        error = pthread_mutex_init(&(new_node->mutex), NULL);
        if (error != 0) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't initialize thread mutex. Error: %s\n", strerror(errno));
#endif
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        error = pthread_mutex_init(&(new_node->terminate_mutex), NULL);
        if (error != 0) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't initialize thread terminate mutex. Error: %s\n", strerror(errno));
#endif
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        new_node->next = NULL;
        new_node->task = NULL;
        new_node->terminate = false;
        new_node->removed = true;

        error = pthread_create(&(new_node->id), NULL, new_thread_wait, (void*)new_node);
        if (error != 0) {
#ifdef DEBUG
            fprintf(stderr, "ERROR: couldn't create new thread. Error: %s\n", strerror(errno));
#endif  
            pthread_mutex_unlock(&thread_pool_mutex);
            teardown_thread_pool(); // teardown any already created threads
            return -1;
        }

        insert_new_thread(new_node);
    }

    pthread_mutex_unlock(&thread_pool_mutex);

    return 0;
}

int request_thread_from_pool(thread_task_t task, void* arg) {
    pthread_mutex_lock(&thread_pool_mutex);
    if (thread_pool == NULL) {
#ifdef DEBUG
        // NON FATAL ERROR -> user can spawn the thread pool instead
        fprintf(stderr, "ERROR: thread pool hasn't been spawned yet\n"); 
#endif
        pthread_mutex_unlock(&thread_pool_mutex);
        return -2;
    }

    if (thread_pool->first == NULL) {
#ifdef DEBUG
        // NON FATAL ERROR -> user can wait until a thread is available
        fprintf(stderr, "ERROR: no thread is currently available\n");
#endif
        pthread_mutex_unlock(&thread_pool_mutex);
        return -3;
    }

    
    struct thread_queue_node* thread = remove_first_thread();

    thread->arg = arg;
    thread->task = task;

    pthread_cond_signal(&(thread->cond));

    pthread_mutex_unlock(&thread_pool_mutex);

    return 0;
}

bool is_thread_pool_spawned(void) {
    pthread_mutex_lock(&thread_pool_mutex);
    bool status = (thread_pool != NULL);
    pthread_mutex_unlock(&thread_pool_mutex);

    return status;
}