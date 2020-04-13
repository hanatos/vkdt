#pragma once
#include <pthread.h>
#include <stdint.h>

#define threads_mutex_t          pthread_mutex_t
#define threads_mutex_lock(m)    pthread_mutex_lock(m)
#define threads_mutex_unlock(m)  pthread_mutex_unlock(m)
#define threads_mutex_destroy(m) pthread_mutex_destroy(m)
#define threads_mutex_init(m, p) pthread_mutex_init(m, p)

// only fwd declare
typedef struct threads_t threads_t;
typedef struct threads_tls_t
{
  uint32_t tid; // thread id from 0..num_threads-1
}
threads_tls_t;


extern threads_t thr;
#ifdef __cplusplus
extern thread_local threads_tls_t thr_tls;
#else
extern _Thread_local threads_tls_t thr_tls;
#endif

void threads_global_init();
void threads_global_cleanup();

// push a new task (task < threads_num()) with given function and argument
int threads_task(
    uint32_t       work_item_cnt,
    uint32_t      *work_item,
    void          *data,
    void         (*run)(uint32_t item, void *data),
    void         (*free)(void*));

// abandon all work and prepare for shutdown
void threads_shutdown();

// query shutdown
int threads_shutting_down();

// return number of threads
int threads_num();

static inline uint32_t threads_id()
{
  return thr_tls.tid;
}

