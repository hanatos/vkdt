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

/**
 * @file pthread_pool.h
 *
 * Pthread Pool definitions.
 */

#ifndef PTHREAD_POOL_H
#define PTHREAD_POOL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <pthread.h>



/**
 * Attributes for a thread pool.
 */
typedef struct pthread_poolattr_t
{
  /* Whether to detect thread starvation on join(). */
  int detectstarve;

  /* Return an error if we try to remove workers such that we would
   * end up with fewer than this many, or if we try to add any tasks
   * with fewer workers than this. */
  int minworkers;

  /* If not 0, return an error if we try to add workers such that we
   * would end up with more than this many. */
  int maxworkers;

} pthread_poolattr_t;

/**
 * Represents a worker thread.
 */
typedef struct pthread_pool_worker_t
{
  /* Pointer to next in linked list. */
  struct pthread_pool_worker_t *next;

  /* The pool this worker is in. */
  struct pthread_pool_t *pool;

  /* Thread that constitutes this worker thread. */
  pthread_t thread;

  /* Whether the worker should keep running or not. */
  int running;

} pthread_pool_worker_t;

/**
 * Represents a task to run/that has been run.
 */
typedef struct pthread_pool_task_t
{
  /* Pointer to next in linked list. */
  struct pthread_pool_task_t *next;

  /* The argument to pass to the routine. */
  void *arg;

  /* The return value of the routine (when complete). */
  void *retval;

  /* The routine that constitutes this task. */
  void *(*routine)(void *);

} pthread_pool_task_t;

/**
 * Represents a thread pool.
 */
typedef struct pthread_pool_t
{
  /* The attributes this pool was created with. */
  pthread_poolattr_t attr;



  /* Mutex that must be acquired before accessing a pthread_pool_t.
   * This is a PTHREAD_MUTEX_NORMAL because we shouldn't use a
   * recursive lock with condition variables, and for efficiency. */
  pthread_mutex_t mutex;



  /* Linked list of worker threads. New worker threads are added to
   * the end. */
  pthread_pool_worker_t *workers;

  /* Linked list of tasks waiting to run. New tasks are added to the
   * end, tasks to run should be taken from the front. */
  pthread_pool_task_t *tasks_pending;

  /* The number of tasks currently running (i.e. taken out of the
   * pending list, but not put back in the complete list). */
  size_t tasks_running;

  /* Linked list of tasks that have been completed. Newly completed
   * tasks are added to the end, tasks to recover should be taken from
   * the front. */
  pthread_pool_task_t *tasks_complete;



  /* Condition that signifies that a worker thread has finished. */
  pthread_cond_t worker_finished;

  /* Condition that signifies when a worker thread must take some
   * action, e.g. there is a new task for one of the threads to take,
   * or one of the threads has been asked to exit.
   *
   * The pool's mutex must be used in conjunction with this. */
  pthread_cond_t worker_action;

  /* Condition that signifies when a task has been completed.
   *
   * The pool's mutex must be used in conjunction with this. */
  pthread_cond_t tasks_complete_added;

} pthread_pool_t;



/*
 * Pool attribute functions.
 */

/**
 * Initialize a thread pool attributes object.
 *
 * Initializes the thread pool attributes object to have the default
 * attributes.
 */
int pthread_poolattr_init(pthread_poolattr_t *attr);

/**
 * Destroy a thread pool attributes object.
 */
int pthread_poolattr_destroy(pthread_poolattr_t *attr);

/**
 * Get the detectstarve attribute for the given thread pool.
 *
 * @see pthread_poolattr_setdetectstarve()
 */
int pthread_poolattr_getdetectstarve(const pthread_poolattr_t *attr,
                                     int *detectstarve);

/**
 * Get the maxworkers attribute for the given thread pool.
 *
 * @see pthread_poolattr_setmaxworkers()
 */
int pthread_poolattr_getmaxworkers(const pthread_poolattr_t *attr,
                                   int *maxworkers);

/**
 * Get the minworkers attribute for the given thread pool.
 *
 * @see pthread_poolattr_setminworkers()
 */
int pthread_poolattr_getminworkers(const pthread_poolattr_t *attr,
                                   int *minworkers);

/**
 * Set the detectstarve attribute for the given thread pool.
 *
 * If non-zero, the pthread_wait() and pthread_join() functions will
 * attempt to detect situations where they could block forever,
 * e.g. if either of these functions is called on a thread pool that
 * must wait for tasks to complete, but that has no worker threads.
 *
 * This is an attribute because it is possible that an application
 * will be adding workers to the pool from another thread, in which
 * case the thread that calls pthread_wait() or pthread_join() may
 * complete after these workers have been added.
 *
 * @see pthread_poolattr_getdetectstarve()
 */
int pthread_poolattr_setdetectstarve(pthread_poolattr_t *attr,
                                     int detectstarve);

/**
 * Set the maxworkers attribute for the given thread pool.
 *
 * @see pthread_poolattr_getmaxworkers()
 */
int pthread_poolattr_setmaxworkers(pthread_poolattr_t *attr,
                                   int maxworkers);

/**
 * Set the maxworkers attribute for the given thread pool.
 *
 * @see pthread_poolattr_getmixworkers()
 */
int pthread_poolattr_setminworkers(pthread_poolattr_t *attr,
                                   int minworkers);



/*
 * General pool functions.
 */

/**
 * Create a new thread pool. Initially, the pool has no worker
 * threads; these must be added with pthread_pool_worker_init().
 *
 * The results of calling pthread_pool_create() on a thread pool twice
 * on the same pthread_pool_t without calling pthread_pool_destroy()
 * in between are undefined.
 *
 * After this call, changes to attr have no effect on the
 * pthread_pool_t.
 *
 * @param[in] pool The thread pool to initialize.
 *
 * @param[in] attr The attributes with which to initialize the pool,
 * or NULL to use the default attributes.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error and the contents of
 * *pool are undefined.
 */
int pthread_pool_create(pthread_pool_t *pool,
                        const pthread_poolattr_t *attr);

/**
 * Release all resources used by a thread pool. This requires waiting
 * for all associated worker threads to finish.
 *
 * After this call, the only function that may be called on the
 * specified pthread_pool_t is pthread_pool_create(), in order to
 * create a new thread pool.  In particular, the results of any tasks
 * cannot be retrieved - if you require this, call pthread_pool_wait()
 * to wait until all tasks are finished, then call pthread_pool_join()
 * or pthread_pool_tryjoin() until all finished tasks have been
 * processed before calling pthread_pool_destroy().
 * 
 * @param[in] pool The thread pool to destroy.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_destroy(pthread_pool_t *pool);

/**
 * Wait for all scheduled tasks to complete.
 *
 * If the detectstarve attribute (see
 * pthread_poolattr_setdetectstarve()) of the given pthread_pool_t is
 * non-zero, this function will return ESRCH if there are pending
 * tasks but no worker threads to process the pending tasks.
 * Otherwise, this call may block forever if no other thread is adding
 * worker tasks.
 *
 * If this function completes successfully, then upon return the given
 * pthread_pool_t is guaranteed to have no pending or running tasks,
 * provided that the thread calling this function is the only one
 * adding tasks to the pool. Otherwise, a competing thread may have
 * added further tasks between this function seeing that there are no
 * pending or running tasks, and returning.
 *
 * @param[in] pool The thread pool to wait for.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_wait(pthread_pool_t *pool);



/*
 * Worker functions.
 */

/**
 * Add a worker to a thread pool. The worker thread will be created by
 * passing the specified attributes directly to the pthread_create(3)
 * function; it is the client programmer's responsibility to make sure
 * that these attributes are appropriate for a worker thread.
 *
 * @param[in] worker The worker thread to initialize. The object
 * pointed to by worker must remain valid until the worker thread or
 * the thread pool itself is destroyed.
 *
 * @param[in] pool The thread pool to add the worker to.
 *
 * @param[in] attr The (possibly NULL) attributes to give to the
 * worker thread.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_worker_init(pthread_pool_worker_t *worker,
                             pthread_pool_t *pool,
                            const pthread_attr_t *attr);

/**
 * Remove a worker thread from this thread pool. This function will
 * block until the worker thread exits, which will either be after the
 * worker has finished its current task, or immediately if the worker
 * is not currently executing a task.
 *
 * @param[in] worker The worker thread to destory.
 *
 * @param[in] pool The thread pool to remove the worker from. This
 * must be the thread pool the worker thread was added to.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 *
 * @bug This function will cause a deadlock if called for a worker
 * thread ro remove itself; at some point this behavior will be
 * modified to detect this deadlock and simply return an error.
 */
int pthread_pool_worker_destroy(pthread_pool_worker_t *worker,
                                pthread_pool_t *pool);



/*
 * Task functions.
 */

/**
 * Schedule a task to be run by a thread pool.
 *
 * When run, routine will be called from the context of one of the
 * thread pool's worker threads, and will be passed arg as its sole
 * argument.
 *
 * @param[in] task The task to initialize and add to the list of
 * waiting tasks in the thread pool.
 *
 * @param[in] pool The thread pool to add the task to.
 *
 * @param[in] routine The function to call when the task is started.
 *
 * @param[in] arg The argument to pass to routine.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_task_init(pthread_pool_task_t *task,
                           pthread_pool_t *pool,
                           void *(*routine)(void *), void *arg);

/**
 * Destroy a task.
 */
int pthread_pool_task_destroy(pthread_pool_task_t *task);

/**
 * Get the argument given to the specified task. This was the arg
 * value passed to pthread_pool_task_init().
 *
 * @param[in] task The task to get the argument of.
 *
 * @param[in] arg Pointer to store the argument into.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_task_getarg(pthread_pool_task_t *task,
                             void **arg);

/**
 * Get the value returned by the specified task. This is the value
 * returned from the routine run when this task was called.
 *
 * You must only call this function when the task has been completed,
 * e.g. any task in the pool when pthread_pool_wait() was successfully
 * called on, or the task for which pthread_pool_join() or
 * pthread_pool_tryjoin() was successful for.
 *
 * @param[in] task The task to get the return value of.
 *
 * @param[in] retval Pointer to store the return value into.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_task_getretval(pthread_pool_task_t *task,
                                void **retval);

/**
 * Get the routine that constitutes the specified task. This was the
 * routine value passed to pthread_pool_task_init().
 *
 * @param[in] task The task to get the routing of.
 *
 * @param[in] routine Pointer to store the routine into.
 *
 * @return If successful, this function returns zero; otherwise, an
 * error number is returned to indicate the error.
 */
int pthread_pool_task_getroutine(pthread_pool_task_t *task,
                                 void *(**routine)(void *));

/**
 * Wait for a task to complete. This function will either immediately
 * return a task that has been completed, or else will block until a
 * task becomes complete.
 *
 * @param[in] pool The pool to retrieve completed tasks from.
 *
 * @param[out] task If not NULL, place a pointer to the completed task
 * here. Otherwise, the task is simply discarded.
 *
 * @return If successful, this function returns zero and, if task is
 * not NULL, *task contains a pointer to the completed
 * task. Otherwise, an error number is returned to indicate the error
 * and the contents of *task are undefined.
 */
int pthread_pool_join(pthread_pool_t *pool,
                      pthread_pool_task_t **task);

/**
 * See if a task to complete. This function will either immediately
 * return a task that has been completed, or else will return EBUSY.
 *
 * @param[in] pool The pool to retrieve completed tasks from.
 *
 * @param[out] task If not NULL, place a pointer to the completed task
 * here. Otherwise, the task is simply discarded.
 *
 * @return If successful and a completed task is available
 * immediately, this function returns zero and, if task is not NULL,
 * *task contains a pointer to the completed task. If unsuccesful
 * because there were no tasks ready immediately, this function
 * returns EBUSY and *task is not modified. Otherwise, an error number
 * is returned to indicate the error and the contents of *task are
 * undefined.
 */
int pthread_pool_tryjoin(pthread_pool_t *pool,
                         pthread_pool_task_t **task);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PTHREAD_POOL_H */

















