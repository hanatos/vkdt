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

#include "consistency.h"
#include "util.h"

#include <assert.h>
#include <errno.h>



static int _pthread_pool_join(pthread_pool_t *pool,
                              pthread_pool_task_t **task)
{
  /* Check that we are not trying to wait for ourself to finish, as
   * that's obviously not going to end well (or at all). Detect this
   * here, as this is most likely a programming error. */
  if (pool_has_this_thread(pool))
    return EDEADLK;

  while (!pool->tasks_complete)
  {
    if (pool->attr.detectstarve)
      /* No complete tasks - must have *both* some workers *and*
       * some tasks to run/that are running, or we're starving. */
      if ((!pool->workers) ||
          (!pool->tasks_pending && !pool->tasks_running))
        /* No tasks => starvation. */
        return ESRCH;

    /* Wait for something to finish. */
    cond_wait(pool, tasks_complete_added);
  }

  if (task)
    *task = pool->tasks_complete;

  pool->tasks_complete = pool->tasks_complete->next;

  return 0;
}

int pthread_pool_join(pthread_pool_t *pool,
                      pthread_pool_task_t **task)
{
  int ret;
  pool_lock(pool);
  ret = _pthread_pool_join(pool, task);
  pool_unlock(pool);
  return ret;
}
























