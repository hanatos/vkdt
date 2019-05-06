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

#include <assert.h>
#include <errno.h>



int pthread_pool_destroy(pthread_pool_t *pool)
{
  pthread_pool_worker_t *worker;

  pool_lock(pool);

  if (pool->workers)
  {
    /* Get all the threads to stop working, but don't use
     * pthread_pool_worker_destroy() as this would attempt to
     * reacquire the mutex, which would lead to a deadlock. */
    for (worker = pool->workers ; worker ; worker = worker->next)
      worker->running = 0;

    /* Wake up threads that are waiting for work. */
    cond_broadcast(pool, worker_action);

    /* Wait for all threads to remove themselves. */
    while (pool->workers)
      cond_wait(pool, worker_finished);
  }

  /* All workers should have removed themselves by now. */
  assert(!pool->workers);

  pool_unlock(pool);

  /* TODO: Not sure we need this any more... */
  /* pthread_pool_wait(pool); */

  /*
   * TODO: We have waited for all tasks to complete -
   * is this good enough? Probably not. Think of something.
   */

  /* TODO: Return error if these fail (but try to do the rest). */

  (void)pthread_mutex_destroy(&pool->mutex);

  pool->workers        = NULL;
  pool->tasks_pending  = NULL;
  pool->tasks_running  = 0;
  pool->tasks_complete = NULL;

  (void)pthread_cond_destroy(&pool->worker_finished);
  (void)pthread_cond_destroy(&pool->worker_action);
  (void)pthread_cond_destroy(&pool->tasks_complete_added);

  return 0;
}

























