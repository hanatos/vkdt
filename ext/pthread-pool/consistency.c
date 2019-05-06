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

#include "consistency.h"

#include "util.h"

#include <stdio.h>



#define ensure(thing)                                         \
  do {                                                        \
    if (!(thing))                                             \
    {                                                         \
      fprintf(stderr, "Inconsistent state: "                  \
          "test \"%s\" failed\n", #thing);                    \
      fprintf(stderr, "%s, line %d\n", __FILE__, __LINE__);   \
      return 0;                                               \
    }                                                         \
  } while (0)



int _worker_consistent(const pthread_pool_worker_t *worker)
{
  if (worker->pool)
  {
    ensure(pool_has_worker(worker->pool, worker));
  }
  else
  {
    ensure(!worker->next);
    ensure(!worker->running);
  }

  return 1;
}

int _task_consistent(const pthread_pool_task_t *task)
{
  return task != 0;
}

int _pool_consistent(const pthread_pool_t *pool)
{
  pthread_pool_worker_t *worker;

  ensure(pool);

  if (pool->attr.maxworkers)
    ensure(pool_nworkers(pool) <= pool->attr.maxworkers);

  /* We're allowed fewer workers than the minimum, so long as we
   * haven't asked to do any tasks. */
  if (pool->attr.minworkers &&
        (pool->tasks_pending ||
         pool->tasks_running ||
         pool->tasks_complete))
    ensure(pool_nworkers(pool) >= pool->attr.minworkers);

  for (worker = pool->workers ; worker ; worker = worker->next)
  {
    ensure(worker->pool == pool);
    worker_consistent(worker);
  }

  return 1;
}































