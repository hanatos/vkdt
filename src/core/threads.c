#include "threads.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>
#ifdef _WIN64
#include <Windows.h> 
#endif

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
  atomic_uint     work_item;     // work item counter (if not referring to another task)
  atomic_uint     done;          // counting how many tasks are *done* (not just *picked*), to coordinate cleanup.
  uint32_t        work_item_cnt; // global number of work items. externally set to 0 if abortion is triggered.
  uint32_t        tid;           // id of thread working on this task, or -1u if not assigned yet
  int32_t         reftask;       // use the atomics of this task (can be us)
  threads_run_t   run;           // work function
  void           *data;          // user data to be passed to run function
  threads_free_t  free;          // optionally clean up user data
  char            desc[30];      // description for debugging
}
threads_task_t;

typedef struct threads_t
{
  uint32_t        num_threads;
  atomic_int      shutdown;

  // worker list
  pthread_t      *worker;
  uint32_t       *cpuid;
  // pool of tasks
  uint32_t        task_max;
  threads_task_t *task;
  pthread_cond_t  cond_task_done;
  pthread_cond_t  cond_task_push;
  pthread_mutex_t mutex_done;
  pthread_mutex_t mutex_push;
  pthread_t       main_thread;
}
threads_t;


// thread worker function
void *threads_work(void *arg)
{
  // global init: set tls storage thread id
  const uint64_t tid = (uint64_t)arg;
  thr_tls.tid = tid;
#if defined(__linux__) && !defined(__ANDROID__)
  // pin ourselves to a cpu:
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(thr.cpuid[tid], &set);
  sched_setaffinity(0, sizeof(cpu_set_t), &set);
#endif

  while(1)
  {
    pthread_mutex_lock(&thr.mutex_push);
    pthread_cond_wait(&thr.cond_task_push, &thr.mutex_push);
    pthread_mutex_unlock(&thr.mutex_push);
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
      uint32_t item = thr.task[task->reftask].work_item++;
      if(item >= task->work_item_cnt) break;
      task->run(item, task->data);
      thr.task[task->reftask].done++;
      if(thr.shutdown) break;
    }
    if(thr.shutdown) break; // don't recycle task. in fact don't clean up and leak whatever we still have (better than lockup)
    if(task->free)
    { // only run cleanup once all tasks are finished:
      // if we knew how many threads are working on this, we could test
      // the every-increasing work_item for > work_item_cnt + num_threads
      // and avoid the done pointer.
      while(!thr.shutdown && (thr.task[task->reftask].done < task->work_item_cnt)) sched_yield();
      if(thr.shutdown) break; // don't clean up, we didn't wait for everybody!
      task->free(task->data);
    }
    // signal everybody that we're done with the task
    pthread_mutex_lock(&thr.mutex_done);
    pthread_cond_broadcast(&thr.cond_task_done);
    pthread_mutex_unlock(&thr.mutex_done);
    // mark task as recyclable:
    uint32_t oldval = __sync_val_compare_and_swap(&task->tid, tid, s_task_state_recycle);
    assert(oldval == tid); // this should never fail, people shall not mess with our task
#ifdef NDEBUG
    (void)oldval;
#endif
  }
  return 0;
}

void threads_global_cleanup()
{
  threads_shutdown();
  for(int i=0;i<thr.num_threads;i++)
    pthread_join(thr.worker[i], 0);
  pthread_cond_destroy(&thr.cond_task_done);
  pthread_cond_destroy(&thr.cond_task_push);
  pthread_mutex_destroy(&thr.mutex_done);
  pthread_mutex_destroy(&thr.mutex_push);
  free(thr.cpuid);
  free(thr.task);
  free(thr.worker);
}

static inline void
threads_task_print(
    int taskid)
{
  if(taskid >= (int)thr.task_max || taskid < 0) return;
  fprintf(stderr, "[threads] task %d '%s' items picked,done/total: %u,%u / %u, ref %d\n",
      taskid, thr.task[taskid].desc,
      thr.task[thr.task[taskid].reftask].work_item,
      thr.task[thr.task[taskid].reftask].done,
      thr.task[thr.task[taskid].reftask].work_item_cnt,
      thr.task[taskid].reftask);
}

static inline void
threads_task_print_all()
{
  for(int t=0;t<thr.task_max;t++)
    threads_task_print(t);
}

// push task
// return:
// -1 no more recyclable tasks, too many tasks running
// -2 argument error, run function is zero or no work to be done cnt <= item
// -3 invalid taskid
// or >= 0: the new task id
int threads_task(
    const char *desc,
    uint32_t    work_item_cnt,
    int         taskid,
    void       *data,
    void      (*run)(uint32_t item, void *data),
    void      (*free)(void*))
{
  if(taskid >= (int)thr.task_max) return -3;
  if(run == 0 || work_item_cnt == 0) return -2;
  if(taskid >= 0 && thr.task[taskid].work_item_cnt <= thr.task[taskid].work_item) return -2;
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
    threads_task_print_all();
    return -1;
  }
  // set all required entries on task
  task->run  = run;
  task->free = free;
  task->data = data;
  task->work_item_cnt = work_item_cnt;
  task->reftask = taskid >= 0 ? taskid : task - thr.task;
  task->work_item = 0;
  task->done = 0;
  (void)snprintf(task->desc, sizeof(task->desc), "%s", desc);

  // mark as ready
  uint32_t oldval = __sync_val_compare_and_swap(&task->tid, s_task_state_initing, s_task_state_ready);
  assert(oldval == s_task_state_initing); // should never fail, nobody should write initing tasks
#ifdef NDEBUG
  (void)oldval;
#endif
  // wake up one thread by signaling the condition
  pthread_mutex_lock(&thr.mutex_push);
  pthread_cond_signal(&thr.cond_task_push);
  pthread_mutex_unlock(&thr.mutex_push);
  return task->reftask; // return taskid of original job we're working on
}

void threads_wait(int taskid)
{
  if(taskid < 0 || taskid >= thr.task_max) return;
  while(thr.task[taskid].done < thr.task[taskid].work_item_cnt)
  {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1; // wait for one second max
    // this is because the threads may finish in between us checking the condition
    // and waiting on the condition (so the condition will not be triggered again)
    pthread_mutex_lock(&thr.mutex_done);
    pthread_cond_timedwait(&thr.cond_task_done, &thr.mutex_done, &ts);
    pthread_mutex_unlock(&thr.mutex_done);
  }
}

void threads_global_init()
{
#ifdef _WIN64
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  thr.num_threads = si.dwNumberOfProcessors;
#else
  thr.num_threads = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  thr.shutdown = 0;
  thr.task_max = thr.num_threads * 10;
  thr.task     = malloc(sizeof(threads_task_t)*thr.task_max);
  thr.cpuid    = malloc(sizeof(uint32_t)*thr.num_threads);
  thr.worker   = malloc(sizeof(pthread_t)*thr.num_threads);

  thr.main_thread = pthread_self();

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
  // TODO: add search path relative to binary?
#ifndef __ANDROID__
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
#endif

  pthread_cond_init(&thr.cond_task_done, 0);
  pthread_cond_init(&thr.cond_task_push, 0);
  pthread_mutex_init(&thr.mutex_done, 0);
  pthread_mutex_init(&thr.mutex_push, 0);

  thr.worker = malloc(sizeof(pthread_t)*thr.num_threads);
  for(uint64_t k=0;k<thr.num_threads;k++)
    pthread_create(thr.worker+k, 0, threads_work, (void*)k);
}

void threads_shutdown()
{
  thr.shutdown = 1;
  // unblock everyone, even though we're not done:
  pthread_mutex_lock(&thr.mutex_done);
  pthread_cond_broadcast(&thr.cond_task_done);
  pthread_mutex_unlock(&thr.mutex_done);
  pthread_mutex_lock(&thr.mutex_push);
  pthread_cond_broadcast(&thr.cond_task_push);
  pthread_mutex_unlock(&thr.mutex_push);
}

int threads_shutting_down()
{
  return thr.shutdown;
}

int threads_num()
{
  return thr.num_threads;
}

// returns zero if the task is done
int threads_task_running(int taskid)
{
  if(taskid < 0 || taskid >= thr.task_max) return 0;
  return thr.task[taskid].done < thr.task[taskid].work_item_cnt;
}

// returns a progress indicator
float threads_task_progress(int taskid)
{
  if(taskid < 0 || taskid >= thr.task_max) return 0.0f;
  return thr.task[taskid].done / (float) thr.task[taskid].work_item_cnt;
}

int threads_i_am_gui()
{
  return pthread_self() == thr.main_thread;
}
