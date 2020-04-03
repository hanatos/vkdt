#include "threads.h"
#include "pthread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>

threads_t thr;
_Thread_local threads_tls_t thr_tls;

typedef struct threads_t
{
  uint32_t               num_threads;
  pthread_pool_t         pool;
  pthread_pool_worker_t *worker;
  pthread_pool_task_t   *task;

  uint32_t *cpuid;

  // sync init:
  atomic_int_fast32_t init;

  int shutdown;
}
threads_t;

// per-worker initialisation
static void *threads_tls_init_one(void *arg)
{
  // remember our thread id:
  thr_tls.tid = *(uint64_t *)arg;

#ifndef __APPLE__
  // pin ourselves to a cpu:
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(thr.cpuid[thr_tls.tid], &set);
  sched_setaffinity(0, sizeof(cpu_set_t), &set);
#endif

  // pid_t tid = syscall(SYS_gettid);
  // fprintf(stdout, "[worker %03u] pinned to cpu %03u by thread %03d\n", rt_tls.tid, rt.threads->cpuid[rt_tls.tid], tid);
  // make sure we only init one thread here, and don't pick another task
  thr.init++;
  while(thr.init < thr.num_threads) sched_yield();
  return 0;
}

static void *threads_tls_cleanup_one(void *arg)
{
  thr.init++;
  while(thr.init < thr.num_threads) sched_yield();
  return 0;
}

static inline void threads_init(threads_t *t)
{
  t->num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_pool_create(&t->pool, NULL);
  t->worker = malloc(sizeof(pthread_pool_worker_t)*t->num_threads);
  for(int k=0;k<t->num_threads;k++)
    pthread_pool_worker_init(t->worker + k, &t->pool, NULL);
  t->init = 0;
  t->shutdown = 0;

  t->task = malloc(sizeof(pthread_pool_task_t)*t->num_threads);
  t->cpuid = malloc(sizeof(uint32_t)*t->num_threads);

#if 0
  const char *def_file = "affinity";
  const char *filename = def_file;
  for(int k=0;k<thr.argc;k++) if(!strcmp(thr.argv[k], "--affinity") && k < thr.argc-1)
      filename = thr.argv[++k];
  // load cpu affinity mask, if any.
  // create one of those by doing something like:
  //  for i in $(seq 0 1 11); do cat /sys/devices/system/cpu/cpu$i/topology/thread_siblings_list; done
  //  | sort -g | uniq
  // or numactl --hardware
  // and then edit it to your needs (this list will start with one thread per core, no hyperthreading used)
  for(int k=0;k<rt.num_threads;k++)
    t->cpuid[k] = k; // default init
  FILE *f = fopen(filename, "rb");
  if(f)
  {
    int k = 0;
    while(!feof(f))
    {
      if(k >= rt.num_threads) break;
      uint32_t cpu1;
      int read = fscanf(f, "%d", &cpu1);
      if(read != 1)
      {
        fprintf(stderr, "[threads] there is a problem with your affinity file `%s' in line %d!\n", filename, k);
        break;
      }
      read = fscanf(f, "%*[^\n]");
      fgetc(f); // read newline
      t->cpuid[k++] = cpu1;
    }
    if(k < rt.num_threads)
      fprintf(stderr, "[threads] not enough entries in your affinity file `%s'! (%d/%d threads)\n", filename, k, rt.num_threads);
    fclose(f);
  }
#endif
}

static inline void threads_tls_init(threads_t *t)
{
  // init thread local storage:
  t->init = 0;
  uint64_t tid[thr.num_threads];
  for(uint64_t k=0;k<thr.num_threads;k++)
  {
    tid[k] = k;
    pthread_pool_task_init(t->task + k, &t->pool, threads_tls_init_one, tid+k);
  }
  pthread_pool_wait(&t->pool);
  for(uint64_t k=0;k<t->num_threads;k++)
    pthread_pool_task_destroy(t->task + k);
}

void threads_global_init()
{
  threads_init(&thr);
  threads_tls_init(&thr);
}

static inline void threads_tls_cleanup(threads_t *t)
{
  t->init = 0;
  // cleanup thread local storage:
  uint64_t tid[thr.num_threads];
  for(uint64_t k=0;k<thr.num_threads;k++)
  {
    tid[k] = k;
    pthread_pool_task_init(t->task + k, &t->pool, threads_tls_cleanup_one, tid+k);
  }
  pthread_pool_wait(&t->pool);
  for(uint64_t k=0;k<t->num_threads;k++)
    pthread_pool_task_destroy(t->task + k);
}

static inline void threads_cleanup(threads_t *t)
{
  threads_tls_cleanup(t);
  pthread_pool_destroy(&t->pool);
  free(t->cpuid);
  free(t->task);
  free(t->worker);
}

void threads_global_cleanup()
{
  threads_shutdown();
  threads_wait(); // does not appear to cause deadlocks even if not currently running
  threads_tls_cleanup(&thr);
  threads_cleanup(&thr);
}

void threads_task(int task, void *(*func)(void *arg), void *arg)
{
  pthread_pool_task_init(thr.task + task, &thr.pool, func, arg);
}

void threads_wait()
{
  pthread_pool_wait(&thr.pool);
  for(uint64_t k=0;k<thr.num_threads;k++)
    pthread_pool_task_destroy(thr.task + k);
}

void threads_shutdown()
{
  thr.shutdown = 1;
}

int threads_shutting_down()
{
  return thr.shutdown;
}

int threads_num()
{
  return thr.num_threads;
}
