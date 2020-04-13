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

#if 0 // rewrite:

// run function
typedef void (*threads_run_t)(uint32_t item, void *data);
typedef void (*threads_free_t)(void *data);

typedef enum threads_task_state_t
{
  s_task_state_ready   = -1u,
  s_task_state_initing = -2u,
  s_task_state_recyle  = -3u,
  // everything else is the threadid of the worker thread
}
threads_task_state_t;

// task to work on, task:thread is 1:1
typedef struct threads_task_t
{
  uint32_t       work_item_storage; // optional data backing for work_item, if no sync is needed (to avoid further alloc)
  uint32_t      *work_item;         // pointing to an atomically increased synchronising work item counter
  uint32_t       work_item_cnt;     // global number of work items. externally set to 0 if abortion is triggered.
  uint32_t       tid;               // id of thread working on this task, or -1u if not assigned yet
  uint32_t       gid;               // group id of this task (might be part of a set of tasks working on the same thing)
  threads_run_t  run;               // work function
  void          *data;              // user data to be passed to run function
  threads_free_t free;              // optionally clean up user data
}
threads_task_t;

// TODO: thread worker function
void *threads_work(void *arg)
{
  // global init: set tls storage thread id
  const uint64_t tid = *(uint64_t *)arg;
  thr_tls.tid = tid;
#ifndef __APPLE__
  // pin ourselves to a cpu:
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(thr.cpuid[tid], &set);
  sched_setaffinity(0, sizeof(cpu_set_t), &set);
#endif

  while(1)
  {
    pthread_mutex_lock(&thr.mtx_task_push);
    pthread_cond_wait(&thr.cond_task_push, &thr.mtx_task_push);
    pthread_mutex_unlock(&thr.mtx_task_push);
    threads_task_t *task = 0;
    for() // TODO: where to look for tasks? have upper and lower bound in ring buffer?
    { // pick task (sync):
      uint32_t oldval = __sync_val_compare_and_swap(task->tid, s_task_state_ready, tid);
      if(oldval != s_task_state_ready) continue; // try another task
    }
    if(task == 0) continue; // wait for a bit more
    while(1)
    { // work on this task
      uint32_t item = __sync_fetch_and_add(task->work_item, 1);
      if(item > task->work_item_cnt) break;
      task->run(item, task->data);
      if(thr.shutdown) break;
    }
    // clean up task
    // make sure work_item_cnt <= *work_item so it won't be taken up by others
    task->work_item_cnt = 0;
    if(task->free) task->free(task->data);
    if(thr.shutdown) break; // don't recycle task
    // signal everybody that we're done with the task
    pthread_cond_broadcast(&thr.cond_task_done);
    // mark task as recyclable:
    uint32_t oldval = __sync_val_compare_and_swap(task->tid, tid, s_task_state_recycle);
    assert(oldval == tid); // this should never fail, people shall not mess with our task
  }
  // cleanup needed?
}

// TODO: cleanup/shutdown
{
  // shutdown
  thr.shutdown = 1;
  // unblock everyone, even though we're not done:
  pthread_cond_broadcast(&thr.cond_task_done);
  pthread_cond_broadcast(&thr.cond_task_push);
}

// TODO: push task
// TODO: interface: pass external work item pointer or 0 so we'll use the task internal one
// return:
// -1 no more recyclable tasks, too many tasks running
// -2 argument error, run function is zero or no work to be done cnt <= item
int threads_task_push(
    threads_t      t,
    uint32_t       work_item_cnt,
    uint32_t      *work_item,
    uint32_t       gid,
    threads_run_t  run,
    threads_free_t free)
{
  // TODO: pick dead entry in task list (dunno probably just linear search)
  // TODO: how to remove old entries?
  // 
  // cmpxchange s_task_state_initing (for master is messing with this task, don't work on it yet)
  // set all required entries on task
  // cmpxchange s_task_state_ready (ready to be picked)
  // wake up one thread by signaling the condition
  pthread_cond_signal(&thr.cond_task_push);
}

// TODO: active wait for task:
// give us the one global work item atomic you passed to the task[s] when creating them
// give us the global work item cnt
// waiting for all threads to idle
// waiting for any work item to complete
// waiting for task to complete
{
  // TODO: call thread worker function and work while we wait
  // after every step, compare work item cnt and work item counter. if done, return
  // if not active waiting: wait for "done with one job" signal and check condition afterwards
}
#endif

typedef struct threads_t
{
  uint32_t num_threads;
  int      shutdown;

  // ===== TODO
  // worker list
  // pool of tasks
  // uint32_t task_max;
  // threads_task_t *task;
  pthread_cond_t cond_task_done;
  pthread_cond_t cond_task_push;
  // =====

  pthread_pool_t         pool;
  pthread_pool_worker_t *worker;
  pthread_pool_task_t   *task;

  uint32_t *cpuid;

  // sync init:
  atomic_int_fast32_t init;
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
