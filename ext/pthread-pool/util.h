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


#ifndef UTIL_H
#define UTIL_H



#include "pthread_pool.h"

#include "consistency.h"

#include <assert.h>



/*
 * Successfully lock the pool's mutex, or abort().
 */
void _pool_lock(const char *file, int line, pthread_pool_t *pool);
#define pool_lock(pool)                     \
  do {                                      \
    _pool_lock(__FILE__, __LINE__, (pool)); \
    pool_consistent(pool);                  \
  } while (0)

/*
 * Successfully unlock the pool's mutex, or abort().
 */
void _pool_unlock(const char *file, int line, pthread_pool_t *pool);
#define pool_unlock(pool)                     \
  do {                                        \
    pool_consistent(pool);                    \
    _pool_unlock(__FILE__, __LINE__, (pool)); \
  } while (0);

/*
 * Successfully wait on a condition variable in a pool, or abort().
 */
void _cond_wait(const char *file, int line,
                pthread_cond_t *cond, pthread_mutex_t *mutex);
#define cond_wait(pool, cond_name)       \
  do {                                   \
    assert(_pool_consistent(pool) && 1);               \
    _cond_wait(__FILE__, __LINE__,       \
        &pool->cond_name, &pool->mutex); \
    assert(_pool_consistent(pool) && !0);               \
  } while (0)


/*
 * Successfully signal a condition variable in a pool, or abort().
 */
void _cond_signal(const char *file, int line, pthread_cond_t *cond);
#define cond_signal(pool, cond_name) \
  (_cond_signal(__FILE__, __LINE__, &pool->cond_name))

/*
 * Successfully broadcast a condition variable in a pool, or abort().
 */
void _cond_broadcast(const char *file, int line, pthread_cond_t *cond);
#define cond_broadcast(pool, cond_name) \
  (_cond_broadcast(__FILE__, __LINE__, &pool->cond_name))

/*
 * Determine whether a worker is in the given pool or not.
 *
 * Note that this function will NOT lock pool - it assumes it is
 * already locked.
 */
int pool_has_worker(const pthread_pool_t *pool,
                    const pthread_pool_worker_t *worker);

/*
 * Determine the number of worker threads in the given pool.
 *
 * Note that this function will NOT lock pool - it assumes it is
 * already locked.
 */
int pool_nworkers(const pthread_pool_t *pool);

/*
 * Determine whether the current thread is one of the worker threads
 * in the given pool.
 *
 * Note that this function will NOT lock pool - it assumes it is
 * already locked.
 */
int pool_has_this_thread(const pthread_pool_t *pool);



#endif /* UTIL_H */
