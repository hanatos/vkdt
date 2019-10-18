#pragma once
#include <pthread.h>

#define threads_mutex_t          pthread_mutex_t
#define threads_mutex_lock(m)    pthread_mutex_lock(m)
#define threads_mutex_unlock(m)  pthread_mutex_unlock(m)
#define threads_mutex_destroy(m) pthread_mutex_destroy(m)
#define threads_mutex_init(m, p) pthread_mutex_init(m, p)

// only fwd declare
typedef struct threads_t threads_t;
typedef struct threads_tls_t threads_tls_t;

extern threads_t thr;
extern _Thread_local threads_tls_t thr_tls;

void threads_global_init();
void threads_global_cleanup();

// push a new task (task < threads_num()) with given function and argument
void threads_task(int task, void *(*func)(void *arg), void *arg);

// abandon all work and prepare for shutdown
void threads_shutdown();

// query shutdown
int threads_shutting_down();

// wait for all tasks to complete
void threads_wait();

// return number of threads
int threads_num();
