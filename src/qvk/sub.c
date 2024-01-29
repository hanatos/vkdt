#include "core/threads.h"
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
  s_sub_done    = 4,
}
qvk_sub_status_t;

typedef struct qvk_sub_item_t
{
  VkQueue          queue;
  VkSubmitInfo     si;
  VkFence          fence;
  VkResult         result;
  qvk_sub_status_t status;
}
qvk_sub_item_t;

// we don't need much. 2 for gui animation rendering + 2 for thumbnails
#define QVK_SUB_QUEUE_SIZE 10
typedef struct qvk_sub_t
{
  atomic_uint     shutdown;
  atomic_uint     cnt;
  qvk_sub_item_t  list[QVK_SUB_QUEUE_SIZE];
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
}
qvk_sub_t;
qvk_sub_t qvk_sub;


VkResult qvk_submit(
  VkQueue       queue,
  int           cnt,
  VkSubmitInfo *si,
  VkFence       fence)
{
  if(qvk_sub.shutdown) return VK_ERROR_INITIALIZATION_FAILED;
  if(threads_i_am_gui())
    return vkQueueSubmit(queue, cnt, si, fence);
  qvk_sub_item_t *task = 0;
  for(int k=0;k<QVK_SUB_QUEUE_SIZE;k++)
  { // brute force search for task
    task = qvk_sub.list + k;
    uint32_t oldval = __sync_val_compare_and_swap(&task->status, s_sub_free, s_sub_initing);
    if(oldval == s_sub_free) break;
    else task = 0;
  }
  task->queue  = queue;
  task->si     = *si;
  task->fence  = fence;
  task->result = VK_INCOMPLETE;
  qvk_sub.cnt++;

  // mark as ready
  __sync_val_compare_and_swap(&task->status, s_sub_initing, s_sub_ready);
  // wake up thread
  // XXX how?
  // glfwPostEmptyEvent but not without gui?
  if(fence == VK_NULL_HANDLE) return VK_SUCCESS; // no fence no problem
  while(1)
  { // if a fence is passed, we need to block the caller until the queue was submitted:
    pthread_mutex_lock  (&qvk_sub.mutex);
    pthread_cond_wait   (&qvk_sub.cond, &qvk_sub.mutex);
    uint32_t oldval = __sync_val_compare_and_swap(&task->status, s_sub_done, s_sub_free);
    pthread_mutex_unlock(&qvk_sub.mutex);
    if(oldval == s_sub_done) return task->result; // this worked, it is now safe to vkWaitForFences
  }
}

// process one submit item found in the queue.
// return the new number of work items in the queue.
int qvk_sub_work()
{
  if(qvk_sub.cnt)
  { // wait for signal to wake up
    qvk_sub_item_t *task = 0;
    for(int k=0;k<QVK_SUB_QUEUE_SIZE;k++)
    { // brute force search for task
      task = qvk_sub.list + k;
      uint32_t oldval = __sync_val_compare_and_swap(&task->status, s_sub_ready, s_sub_running);
      if(oldval == s_sub_ready) break;
      else task = 0;
    }
    if(task)
    {
      task->result = vkQueueSubmit(task->queue, 1, &task->si, task->fence);
      if(task->fence == VK_NULL_HANDLE)
        __sync_val_compare_and_swap(&task->status, s_sub_running, s_sub_free);
      else // extra step: block submit caller until done->free
        __sync_val_compare_and_swap(&task->status, s_sub_running, s_sub_done);
      qvk_sub.cnt--;
      // trigger one global condition variable to wake up all waiting folks
      pthread_mutex_lock    (&qvk_sub.mutex);
      pthread_cond_broadcast(&qvk_sub.cond);
      pthread_mutex_unlock  (&qvk_sub.mutex);
    }
  }
  return qvk_sub.cnt;
}

void qvk_sub_init()
{
  qvk_sub.cnt = 0;
  qvk_sub.shutdown = 0;
  for(int i=0;i<QVK_SUB_QUEUE_SIZE;i++)
    qvk_sub.list[i].status = s_sub_free;
  pthread_cond_init (&qvk_sub.cond, 0);
  pthread_mutex_init(&qvk_sub.mutex, 0);
}

void qvk_sub_shutdown()
{
  qvk_sub.shutdown = 1;
  while(qvk_sub_work()) {} // drain, this is called from the main thread only
}

void qvk_sub_cleanup()
{
  pthread_mutex_destroy(&qvk_sub.mutex);
  pthread_cond_destroy(&qvk_sub.cond);
}
#undef QVK_SUB_QUEUE_SIZE
