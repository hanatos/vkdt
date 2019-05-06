/*
 * Copyright (C) 2013 John Graham <johngavingraham@googlemail.com>.
 *
 * This file is part of Pthread Pool.
 *
 * Pthread Pool is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * Pthread Pool is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Pthread Pool.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "worker_thread.h"

#include "consistency.h"
#include "pthread_pool.h"
#include "util.h"

#include <assert.h>



/* TODO: cleanup_push in case task exits. */



/*
 * Get the next task in the pool's list of pending tasks. Returns NULL
 * if we should exit the thread because either (i) the pool is
 * finishing and there are no more tasks or (ii) the pool has
 * finished.
 *
 * On error, don't return - just exit the thread.
 */
static pthread_pool_task_t *get_task(pthread_pool_worker_t *worker)
{
  pthread_pool_task_t *task = NULL;

  pool_lock(worker->pool);
  pool_consistent(worker->pool);
  worker_consistent(worker);

  while (!task)
  {
    if (worker->running)
    {
      /* If a task isn't available, wait for one and loop again. */
      if (worker->pool->tasks_pending)
        task = worker->pool->tasks_pending;
      else
        cond_wait(worker->pool, worker_action);
    }
    else
    {
      /* Exit without looking at the task queue. */
      worker_consistent(worker);
      pool_unlock(worker->pool);
      return NULL;
    }
  }

  worker->pool->tasks_running++;
  worker->pool->tasks_pending = task->next;

  task->next = NULL;

  worker_consistent(worker);
  pool_unlock(worker->pool);

  return task;
}

/*
 * Put a task into the pool's list of completed tasks.
 */
static void put_task(pthread_pool_t *pool, pthread_pool_task_t *task)
{
  int err;
  pthread_pool_task_t **task_pp;

  pool_lock(pool);

  /* Find the last completed task. */
  task_pp = &pool->tasks_complete;
  while (*task_pp)
    task_pp = &(*task_pp)->next;

  pool->tasks_running--;
  *task_pp = task;
  task->next = NULL; /* This task now terminates the list. */

  /* Signal any threads waiting on complete tasks. */
  err = pthread_cond_signal(&pool->tasks_complete_added);
  if (0 != err)
    pthread_exit(0);

  pool_unlock(pool);
}

static void remove_self(pthread_pool_worker_t *worker)
{
  pthread_pool_t *pool = worker->pool;
  pthread_pool_worker_t **worker_pp;

  pool_lock(pool);

  /* Make worker_pp point to the pointer in the linked list that
   * points to us. */
  worker_pp = &pool->workers;
  while (*worker_pp != worker)
    worker_pp = &(*worker_pp)->next;

  /* Now break us out of the linked list. */
  *worker_pp = worker->next;
  worker->next = NULL;
  worker->pool = NULL;

  worker_consistent(worker);

  /* Signal that we've finished. */
  cond_broadcast(pool, worker_finished);

  pool_unlock(pool);
}



void *worker_thread(void *worker_)
{
  pthread_pool_worker_t *worker = (pthread_pool_worker_t *)worker_;
  pthread_pool_task_t   *task;

  /* Main thread loop. */
  while (NULL != (task = get_task(worker)))
  {
    task->retval = task->routine(task->arg);
    put_task(worker->pool, task);
  }

  remove_self(worker);

  return NULL;
}

































