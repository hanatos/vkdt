#include "threads.h"

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
#include <pthread.h>

threads_t thr;
_Thread_local threads_tls_t thr_tls;

typedef void (*threads_run_t)(uint32_t item, void *data);
typedef void (*threads_free_t)(void *data);

typedef enum threads_task_state_t
{
  s_task_state_ready   = -1u,
  s_task_state_initing = -2u,
  s_task_state_recycle = -3u,
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
  threads_run_t  run;               // work function
  void          *data;              // user data to be passed to run function
  threads_free_t free;              // optionally clean up user data
}
threads_task_t;

typedef struct threads_t
{
  uint32_t num_threads;
  int      shutdown;

  // worker list
  pthread_t      *worker;
  uint32_t       *cpuid;
  // pool of tasks
  uint32_t        task_max;
  threads_task_t *task;
  pthread_cond_t  cond_task_done;
  pthread_cond_t  cond_task_push;
}
threads_t;


// thread worker function
void *threads_work(void *arg)
{
  // global init: set tls storage thread id
  const uint64_t tid = (uint64_t)arg;
  thr_tls.tid = tid;
#ifndef __APPLE__
  // pin ourselves to a cpu:
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(thr.cpuid[tid], &set);
  sched_setaffinity(0, sizeof(cpu_set_t), &set);
#endif

  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, 0);

  while(1)
  {
    pthread_mutex_lock(&mutex);
    // pthread_cond_wait(&thr.cond_task_push, &mutex);
    struct timespec dt = {
      .tv_sec = 0,
      .tv_nsec = 5000,
    };
    pthread_cond_timedwait(&thr.cond_task_push, &mutex, &dt);
    pthread_mutex_unlock(&mutex);
    if(thr.shutdown) break;
    threads_task_t *task = 0;
    for(int k=0;k<thr.task_max;k++)
    { // brute force search for task
      task = thr.task + k;
      uint32_t oldval = __sync_val_compare_and_swap(&task->tid, s_task_state_ready, tid);
      if(oldval == s_task_state_ready) break;
      else task = 0;
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
    uint32_t oldval = __sync_val_compare_and_swap(&task->tid, tid, s_task_state_recycle);
    assert(oldval == tid); // this should never fail, people shall not mess with our task
  }
  // cleanup
  pthread_mutex_destroy(&mutex);
  return 0;
}

void threads_global_cleanup()
{
  threads_shutdown();
  for(int i=0;i<thr.num_threads;i++)
    pthread_join(thr.worker[i], 0);
  pthread_cond_destroy(&thr.cond_task_done);
  pthread_cond_destroy(&thr.cond_task_push);
  free(thr.cpuid);
  free(thr.task);
  free(thr.worker);
}

// push task
// return:
// -1 no more recyclable tasks, too many tasks running
// -2 argument error, run function is zero or no work to be done cnt <= item
int threads_task(
    uint32_t       work_item_cnt,
    uint32_t      *work_item,
    void          *data,
    void         (*run)(uint32_t item, void *data),
    void         (*free)(void*))
{
  if(run == 0 || work_item_cnt == 0) return -2;
  if(work_item && work_item_cnt <= *work_item) return -2;
  threads_task_t *task = 0;
  for(int k=0;k<thr.task_max;k++)
  { // brute force search for task
    task = thr.task + k;
    uint32_t oldval = __sync_val_compare_and_swap(&task->tid, s_task_state_recycle, s_task_state_initing);
    if(oldval == s_task_state_recycle) break;
    else task = 0;
  }
  if(task == 0)
  {
    fprintf(stderr, "[threads] no more free tasks!\n");
    return -1;
  }
  // set all required entries on task
  task->run  = run;
  task->free = free;
  task->data = data;
  task->work_item_cnt = work_item_cnt;
  task->work_item_storage = 0;
  if(work_item) task->work_item = work_item;
  else task->work_item = &task->work_item_storage;

  // mark as ready
  uint32_t oldval = __sync_val_compare_and_swap(&task->tid, s_task_state_initing, s_task_state_ready);
  assert(oldval == s_task_state_initing); // should never fail, nobody should write initing tasks
  // wake up one thread by signaling the condition
  pthread_cond_signal(&thr.cond_task_push);
  return 0;
}

#if 0
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

void threads_global_init()
{
  thr.num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  thr.shutdown = 0;
  thr.task_max = thr.num_threads * 10;
  thr.task     = malloc(sizeof(threads_task_t)*thr.task_max);
  thr.cpuid    = malloc(sizeof(uint32_t)*thr.num_threads);
  thr.worker   = malloc(sizeof(pthread_t)*thr.num_threads);

  for(int k=0;k<thr.task_max;k++)
    thr.task[k].tid = s_task_state_recycle;

  for(int k=0;k<thr.num_threads;k++)
    thr.cpuid[k] = k; // default init

  const char *def_file = "affinity";
  const char *filename = def_file;
  // for(int k=0;k<thr.argc;k++) if(!strcmp(thr.argv[k], "--affinity") && k < thr.argc-1)
  //    filename = thr.argv[++k];
  // load cpu affinity mask, if any.
  // create one of those by doing something like:
  //  for i in $(seq 0 1 11); do cat /sys/devices/system/cpu/cpu$i/topology/thread_siblings_list; done
  //  | sort -g | uniq
  // or numactl --hardware
  // and then edit it to your needs (this list will start with one thread per core, no hyperthreading used)
  FILE *f = fopen(filename, "rb");
  if(f)
  {
    int k = 0;
    while(!feof(f))
    {
      if(k >= thr.num_threads) break;
      uint32_t cpu1;
      int read = fscanf(f, "%d", &cpu1);
      if(read != 1)
      {
        fprintf(stderr, "[threads] there is a problem with your affinity file `%s' in line %d!\n", filename, k);
        break;
      }
      read = fscanf(f, "%*[^\n]");
      fgetc(f); // read newline
      thr.cpuid[k++] = cpu1;
    }
    if(k < thr.num_threads)
      fprintf(stderr, "[threads] not enough entries in your affinity file `%s'! (%d/%d threads)\n", filename, k, thr.num_threads);
    fclose(f);
  }

  pthread_cond_init(&thr.cond_task_done, 0);
  pthread_cond_init(&thr.cond_task_push, 0);

  thr.worker = malloc(sizeof(pthread_t)*thr.num_threads);
  for(uint64_t k=0;k<thr.num_threads;k++)
    pthread_create(thr.worker+k, 0, threads_work, (void*)k);
}

void threads_shutdown()
{
  thr.shutdown = 1;
  // unblock everyone, even though we're not done:
  pthread_cond_broadcast(&thr.cond_task_done);
  pthread_cond_broadcast(&thr.cond_task_push);
}

int threads_shutting_down()
{
  return thr.shutdown;
}

int threads_num()
{
  return thr.num_threads;
}
