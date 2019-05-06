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

#include "pthread_pool.h"

#include "util.h"

#include <errno.h>



static int _pthread_pool_task_init(pthread_pool_task_t *task,
                                   pthread_pool_t *pool,
                                   void *(*routine)(void *), void *arg)
{
  pthread_pool_task_t **task_pp;

  task->next    = NULL;
  task->arg     = arg;
  task->retval  = NULL;
  task->routine = routine;

  /* See if we have enough tasks. */
  if (pool->attr.minworkers)
    if (pool->attr.minworkers < pool_nworkers(pool))
      return ERANGE;

  /* Find the last pending task. */
  task_pp = &pool->tasks_pending;
  while (*task_pp)
    task_pp = &(*task_pp)->next;

  /* Add the task to the end of the list. */
  *task_pp = task;

  /* Signal to one waiting thread that some tasks are ready. */
  cond_signal(pool, worker_action);

  return 0;
}

int pthread_pool_task_init(pthread_pool_task_t *task,
                           pthread_pool_t *pool,
                           void *(*routine)(void *), void *arg)
{
    int ret;
    pool_lock(pool);
    ret = _pthread_pool_task_init(task, pool, routine, arg);
    pool_unlock(pool);
    return ret;
}























