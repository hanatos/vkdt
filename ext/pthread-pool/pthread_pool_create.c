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

#define _GNU_SOURCE

#include "pthread_pool.h"

#include "consistency.h"
#include "fatal.h"

#include <assert.h>



/*
 * Initialize the mutex for a pthread_pool_t. Returns 0 on success,
 * or one of ENOMEM, EBUSY, EAGAIN or EPERM on failure.
 */
static int init_mutex(pthread_mutex_t *mutex)
{
  int err;
  pthread_mutexattr_t attr;

  fatal_inval(err = pthread_mutexattr_init(&attr));

  if (0 != err)
    /* Can still fail with ENOMEM. */
    return err;

  fatal_inval(pthread_mutexattr_settype(&attr,
                                        PTHREAD_MUTEX_NORMAL));

  fatal_inval(pthread_mutex_init(mutex, &attr));

  fatal_inval(pthread_mutexattr_destroy(&attr));

  return 0;
}



int pthread_pool_create(pthread_pool_t *pool,
                        const pthread_poolattr_t *attr)
{
  int err;

  if (!pool)
    return EINVAL;

  /* Initialize everything to its default value. */
  pool->workers        = NULL;
  pool->tasks_pending  = NULL;
  pool->tasks_running  = 0;
  pool->tasks_complete = NULL;

  /* Initialize attributes. */
  if (attr)
    pool->attr = *attr;
  else
    pthread_poolattr_init(&pool->attr);

  err = init_mutex(&pool->mutex);
  if (0 != err)
    return err;

  fatal_inval(err = pthread_cond_init(&pool->worker_finished,
                                      NULL));
  if (0 != err)
  {
    (void)pthread_mutex_destroy(&pool->mutex);
    return err;
  }


  fatal_inval(err = pthread_cond_init(&pool->worker_action,
                                      NULL));
  if (0 != err)
  {
    (void)pthread_mutex_destroy(&pool->mutex);
    (void)pthread_cond_destroy(&pool->worker_finished);
    return err;
  }

  fatal_inval(err = pthread_cond_init(&pool->tasks_complete_added,
                                      NULL));
  if (0 != err)
  {
    (void)pthread_mutex_destroy(&pool->mutex);
    (void)pthread_cond_destroy(&pool->worker_finished);
    (void)pthread_cond_destroy(&pool->worker_action);
    return err;
  }

  pool_consistent(pool);

  return 0;
}























