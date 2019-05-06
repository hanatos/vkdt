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



static int _pthread_pool_wait(pthread_pool_t *pool)
{
  /* Check that we are not trying to wait for ourself to finish, as
   * that's obviously not going to end well (or at all). Detect this
   * here, as this is most likely a programming error. */
  if (pool_has_this_thread(pool))
    return EDEADLK;

  /* Wait for all tasks to complete. */
  while (pool->tasks_pending || pool->tasks_running)
  {
    if (pool->attr.detectstarve && !pool->workers)
      /* Would potentially block forever. */
      return ESRCH;

    /* TODO: What if the last thread were to exit here? */
    cond_wait(pool, tasks_complete_added);
  }

  return 0;
}

int pthread_pool_wait(pthread_pool_t *pool)
{
  int ret;
  pool_lock(pool);
  ret = _pthread_pool_wait(pool);
  pool_unlock(pool);
  return ret;
}

























