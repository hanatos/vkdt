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


#include "util.h"

#include "fatal.h"



void _pool_lock(const char *file, int line, pthread_pool_t *pool)
{
  if (0 != pthread_mutex_lock(&pool->mutex))
    _fatal(file, line, "Could not lock pool mutex");
}

void _pool_unlock(const char *file, int line, pthread_pool_t *pool)
{
  if (0 != pthread_mutex_unlock(&pool->mutex))
    _fatal(file, line, "Could not unlock pool mutex");
}

void _cond_wait(const char *file, int line,
                pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  if (0 != pthread_cond_wait(cond, mutex))
    _fatal(file, line, "Could not wait on condition");
}

void _cond_signal(const char *file, int line, pthread_cond_t *cond)
{
  if (0 != pthread_cond_signal(cond))
    _fatal(file, line, "Could not signal condition");
}

void _cond_broadcast(const char *file, int line, pthread_cond_t *cond)
{
  if (0 != pthread_cond_broadcast(cond))
    _fatal(file, line, "Could not broadcast condition");
}

int pool_has_worker(const pthread_pool_t *pool,
                    const pthread_pool_worker_t *worker)
{
  pthread_pool_worker_t *pw;

  for (pw = pool->workers ; pw ; pw = pw->next)
    if (pw == worker)
      return 1;

  return 0;
}

int pool_nworkers(const pthread_pool_t *pool)
{
  int n = 0;
  pthread_pool_worker_t *w;

  for (w = pool->workers ; w ; w = w->next)
    n++;

  return n;
}

int pool_has_this_thread(const pthread_pool_t *pool)
{
  pthread_pool_worker_t *worker;
  pthread_t self = pthread_self();

  for (worker = pool->workers ; worker ; worker = worker->next)
    if (pthread_equal(self, worker->thread))
      return 1;

  return 0;
}

