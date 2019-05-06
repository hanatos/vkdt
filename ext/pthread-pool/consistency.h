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

#ifndef CONSISTENCY_H
#define CONSISTENCY_H



#include "pthread_pool.h"

#include <assert.h>



/* Note: Does NOT lock for you. */
int _worker_consistent(const pthread_pool_worker_t *worker);
#define worker_consistent(worker) \
  do { assert(_worker_consistent(worker)); } while (0)

/* Note: Does NOT lock for you. */
int _task_consistent(const pthread_pool_task_t *task);
#define task_consistent(task) \
  do { assert(_task_consistent(task)); } while (0)

/* Checks the pool and all its workers for consistency.
 * Note: Does NOT lock for you. */
int _pool_consistent(const pthread_pool_t *pool);
#define pool_consistent(pool) \
  do { assert(_pool_consistent(pool)); } while (0)



#endif /* CONSISTENCY_H */

















