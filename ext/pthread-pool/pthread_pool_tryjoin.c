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
#include <stdlib.h>



static int _pthread_pool_tryjoin(pthread_pool_t *pool,
                                 pthread_pool_task_t **task)
{
  /* TODO: Return ESRCH if the detectstarve attribute is set? */
  
  if (!pool->tasks_complete)
    return EBUSY;

  if (task)
    *task = pool->tasks_complete;

  pool->tasks_complete = pool->tasks_complete->next;

  return 0;
}

int pthread_pool_tryjoin(pthread_pool_t *pool,
                         pthread_pool_task_t **task)
{
  int ret;
  pool_lock(pool);
  ret = _pthread_pool_tryjoin(pool, task);
  pool_unlock(pool);
  return ret;
}























