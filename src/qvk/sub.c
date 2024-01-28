#include "threads.h"
#include "sub.h"
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

typedef enum qvk_sub_status_t
{
  s_sub_free    = 0,
  s_sub_initing = 1,
  s_sub_ready   = 2,
  s_sub_running = 3,
}
qvk_sub_status_t;

typedef struct qvk_sub_item_t
{
  VkQueue          queue;
  VkSubmitInfo     si;
  VkFence          fence;
  qvk_sub_status_t status;
}
qvk_sub_item_t;

// we don't need much. 2 for gui animation rendering + 2 for thumbnails
#define QVK_SUB_QUEUE_SIZE 10
typedef struct qvk_sub_t
{
  atomic_uint     running;
  pthread_mutex_t mutex_push;
  pthread_cond_t  cond_push;
  qvk_sub_item_t  list[QVK_SUB_QUEUE_SIZE];
  pthread_t       thread;
}
qvk_sub_t;

qvk_sub_t qvk_sub;


void qvk_submit(
  VkQueue       queue,
  int           cnt,
  VkSubmitInfo *si,
  VkFence       fence)
{
  qvk_sub_item_t *task = 0;
  for(int k=0;k<QVK_SUB_QUEUE_SIZE;k++)
  { // brute force search for task
    task = qvk_sub.list + k;
    uint32_t oldval = __sync_val_compare_and_swap(&task->status, s_sub_free, s_sub_initing);
    if(oldval == s_sub_free) break;
    else task = 0;
  }
  task->queue = queue;
  task->si    = *si;
  task->fence = fence;

  fprintf(stderr, "[sub] pushing new task!\n");
  // mark as ready
  __sync_val_compare_and_swap(&task->status, s_sub_initing, s_sub_ready);
  // wake up thread
  pthread_mutex_lock(&qvk_sub.mutex_push);
  pthread_cond_signal(&qvk_sub.cond_push);
  pthread_mutex_unlock(&qvk_sub.mutex_push);
}

// almost same as thread workers do, only that this thread remains pinned
// to do only vkQueueSubmits and doesn't need cleanup/signals when done.
void* run(void*)
{
  while(qvk_sub.running)
  { // wait for signal to wake up
    fprintf(stderr, "[sub] worker waiting for cmdbuf\n");
    pthread_mutex_lock(&qvk_sub.mutex_push);
    pthread_cond_wait(&qvk_sub.cond_push, &qvk_sub.mutex_push);
    pthread_mutex_unlock(&qvk_sub.mutex_push);
    if(!qvk_sub.running) break;
    qvk_sub_item_t *task = 0;
    for(int k=0;k<QVK_SUB_QUEUE_SIZE;k++)
    { // brute force search for task
      task = qvk_sub.list + k;
      uint32_t oldval = __sync_val_compare_and_swap(&task->status, s_sub_ready, s_sub_running);
      if(oldval == s_sub_ready) break;
      else task = 0;
    }
    if(task == 0) continue; // wait for a bit more
    fprintf(stderr, "[sub] submitting task!\n");
    vkQueueSubmit(task->queue, 1, &task->si, task->fence);
    if(!qvk_sub.running) break;
    __sync_val_compare_and_swap(&task->status, s_sub_running, s_sub_free);
  }
  return 0;
}

void qvk_sub_init()
{
  qvk_sub.running = 1;
  for(int i=0;i<QVK_SUB_QUEUE_SIZE;i++)
    qvk_sub.list[i].status = s_sub_free;
  pthread_cond_init(&qvk_sub.cond_push, 0);
  pthread_mutex_init(&qvk_sub.mutex_push, 0);
  pthread_create(&qvk_sub.thread, 0, run, 0);
}

void qvk_sub_cleanup()
{
  qvk_sub.running = 0;
  pthread_mutex_lock(&qvk_sub.mutex_push);
  pthread_cond_broadcast(&qvk_sub.cond_push);
  pthread_mutex_unlock(&qvk_sub.mutex_push);
}

#undef QVK_SUB_QUEUE_SIZE
