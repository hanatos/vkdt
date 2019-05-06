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



static int _pthread_pool_worker_destroy(pthread_pool_worker_t *worker,
                                        pthread_pool_t *pool)
{
  if (!pool_has_worker(pool, worker))
    return EINVAL;

  /* See if this would take us below the worker limit. */
  if (pool->attr.minworkers)
    if (pool_nworkers(pool) - 1 < pool->attr.minworkers)
      return ERANGE;

  /* Tell worker to stop running. */
  worker->running = 0;

  /* Wake up all workers, in case it's blocked. */
  cond_broadcast(pool, worker_action);

  /* Wait for it to have removed itself from the pool. */
  while (pool_has_worker(pool, worker))
    cond_wait(pool, worker_finished);

  assert(!worker->pool);
  assert(!worker->next);

  return 0;
}

int pthread_pool_worker_destroy(pthread_pool_worker_t *worker,
                                pthread_pool_t *pool)
{
  int ret;
  pool_lock(pool);
  ret = _pthread_pool_worker_destroy(worker, pool);
  pool_unlock(pool);
  return ret;
}




















