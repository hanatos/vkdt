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
#include "worker_thread.h"

#include <errno.h>



static int _pthread_pool_worker_init(pthread_pool_worker_t *worker,
                                     pthread_pool_t *pool,
                                     const pthread_attr_t *attr)
{
  int err;
  pthread_pool_worker_t **worker_pp;

  if (pool_has_worker(pool, worker))
    return EEXIST;

  /* See if this would put us over the worker limit. */
  if (pool->attr.maxworkers)
    if (pool_nworkers(pool) + 1 > pool->attr.maxworkers)
      return ERANGE;

  /* Find the last worker. */
  worker_pp = &pool->workers;
  while (*worker_pp)
    worker_pp = &(*worker_pp)->next;

  /* Add the worker to the end of the list. */
  *worker_pp = worker;
  worker->next        = NULL;
  worker->pool        = pool;
  worker->running     = 1;

  /* EINVAL => invalid `attr', so here it isn't fatal. */
  err = pthread_create(&(*worker_pp)->thread, attr,
                       &worker_thread, *worker_pp);

  if (0 != err)
  {
    *worker_pp = NULL; /* Re-terminate linked list. */
    return err;
  }

  /* Failure => not joinable, which is fine (user's attributes may
   * have included this, but it's no big deal). */
  err = pthread_detach((*worker_pp)->thread);

  /* Mostly do the same action whether successful or not. */
  if (0 != err)
    *worker_pp = NULL; /* Re-terminate linked list. */

  return err;
}

int pthread_pool_worker_init(pthread_pool_worker_t *worker,
                             pthread_pool_t *pool,
                             const pthread_attr_t *attr)
{
    int ret;
    pool_lock(pool);
    ret = _pthread_pool_worker_init(worker, pool, attr);
    pool_unlock(pool);
    return ret;
}

























