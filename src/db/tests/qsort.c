/*
    This file is part of corona-13.

    copyright (c) 2016--2019 johannes hanika.

    corona-13 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    corona-13 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with corona-13. If not, see <http://www.gnu.org/licenses/>.
*/

#include "core.h"
#include "threads.h"

#include <stdint.h>

typedef enum job_type_t
{
  s_job_all = 0,
  s_job_node,
  s_job_scan,
  s_job_swap,
}
job_type_t;

typedef struct job_t
{
  job_type_t type;
  union
  {
    struct
    {
      int64_t left;
      int64_t right;
    }
    node;
    struct
    {
      uint64_t pivot;
      int64_t begin, back;
      // output:
      int64_t *right;
      int *done;
    }
    scan;
    struct
    {
      uint64_t read, num_read;          // indices to read from
      uint64_t write;                   // current write index
      uint64_t read_block, write_block; // block id of read/write indices
      uint64_t *b_l, *b_r, *b_e;        // pointers to shared thread blocks
      int *done;
    }
    swap;
  };
}
job_t;

typedef struct queue_t
{
  pthread_mutex_t mutex;
  job_t *jobs;
  int32_t num_jobs;
}
queue_t;

typedef struct qsort_t
{
  // TODO: max jobs is determined by node job which only splits off so many
  // TODO: other jobs in queue (say N). there will be at most one scan/swap
  // TODO: partition job in flight per thread queue, which means it's
  // TODO: N + #threads jobs total.
  queue_t *queue;

  uint64_t *id;
  uint64_t sorted;
  uint64_t num;
}
qsort_t;

static uint64_t node_job_work(qsort_t *s, queue_t *q, uint64_t left, uint64_t right);
static void scan_job_work(qsort_t *s, queue_t *q, job_t *j);
static void swap_job_work(qsort_t *s, queue_t *q, job_t *j);

static int do_one_job(qsort_t *s, job_type_t mask)
{
  const int tid = thr_tls.tid;
  int t = tid;
  int tries = 0;
  job_t job;
  while(1)
  {
    threads_mutex_lock(&s->queue[t].mutex);
    int gotjob = s->queue[t].num_jobs > 0;
    if(gotjob)
      gotjob = (mask == s_job_all) || (s->queue[t].jobs[s->queue[t].num_jobs-1].type != s_job_node);
    if(gotjob)
    {
      job = s->queue[t].jobs[--s->queue[t].num_jobs]; // take copy for sync
      threads_mutex_unlock(&s->queue[t].mutex);
      break;
    }
    else
    {
      threads_mutex_unlock(&s->queue[t].mutex);
      if(s->sorted >= s->num) return 1; // reading is atomic
      // search the other queues:
      t = (t+1)%thr.num_threads;
      if(tries ++ >= thr.num_threads)
      {
        usleep(100);
        return 0;
      }
      continue;
    }
  }
  switch(job.type)
  {
    case s_job_node:
    {
      const uint64_t done = node_job_work(s, s->queue+tid, job.node.left, job.node.right);
      __sync_fetch_and_add(&s->sorted, done);
      break;
    }
    case s_job_scan:
      scan_job_work(s, s->queue+tid, &job);
      break;
    case s_job_swap:
      swap_job_work(s, s->queue+tid, &job);
      break;
    default: // to formally handle s_job_all
      break;
  }
  return 0;
}

static int64_t partition_ser(
    qsort_t *s,
    queue_t *q,
    uint64_t pivot,
    int64_t begin,
    int64_t back)
{
  int64_t right = back; // start of right block
  for(int64_t i=begin;i<right;)
  {
    if(s->id[i] >= pivot)
    { // prim is right
      // swap bufs:
      uint64_t tmp = s->id[i];
      s->id[i] = s->id[--right];
      s->id[right] = tmp;
    }
    else
    { // primitive is left
      i++; // buffers are all good already
    }
  }
  return right;
}

static void scan_job_work(qsort_t *s, queue_t *q, job_t *j)
{
  *j->scan.right = partition_ser(s, q, j->scan.pivot, j->scan.begin, j->scan.back);
  __sync_fetch_and_add(j->scan.done, 1);
}

static void swap_job_work(qsort_t *s, queue_t *q, job_t *j)
{
  uint64_t r = j->swap.read - 1; // will increment first thing below
  uint64_t w = j->swap.write - 1;
  uint64_t rb = j->swap.read_block;
  uint64_t wb = j->swap.write_block;
  uint64_t rB = j->swap.b_e[j->swap.read_block];
  uint64_t wB = j->swap.b_r[j->swap.write_block];
  const int nt = thr.num_threads;
  for(uint64_t i=0;i<j->swap.num_read;i++)
  {
    if(++r >= rB)
    { // move to next read block (read dislocated right prims)
      while(++rb<nt)
      { // jump over empty blocks
        assert(rb < nt);
        r  = j->swap.b_r[rb];
        rB = j->swap.b_e[rb];
        if(r < rB) break;
      }
    }
    assert(rb < nt);
    assert(r < rB);
    if(++w >= wB)
    { // move to next write block (write dislocated left prims)
      while(++wb<nt)
      { // jump over empty blocks
        assert(wb < nt);
        w  = j->swap.b_l[wb];
        wB = j->swap.b_r[wb];
        if(w < wB) break;
      }
    }
    assert(wb < nt);
    assert(w < wB);

    // swap
    uint64_t tmp = s->id[r];
    s->id[r] = s->id[w];
    s->id[w] = tmp;
  }
  __sync_fetch_and_add(j->swap.done, 1);
}

// sort indices in parallel
static uint64_t partition_par(
    qsort_t *s,
    queue_t *q,
    const uint64_t pivot,
    const int64_t begin, // begin of buffer
    const int64_t back)  // element after buffer
{
  // partition list with couple of threads for couple of blocks between [begin..back]
  const int NVLA = 1024; // faster to allocate this way
  const int nt = thr.num_threads;
  assert(nt < NVLA);
  int64_t right[NVLA];
  uint64_t b_l[NVLA], b_r[NVLA], b_e[NVLA];

  int done = 0;
  threads_mutex_lock(&q->mutex);
  for(int t=0;t<nt;t++)
  {
    int j = q->num_jobs++;
    assert(j < 3*nt);
    job_t *job = q->jobs + j;
    job->type = s_job_scan;
    job->scan.pivot = pivot;
    job->scan.right = right+t;
    job->scan.begin = begin + (uint64_t)( t   /(double)nt*(back-begin));
    job->scan.back  = begin + (uint64_t)((t+1)/(double)nt*(back-begin));
    b_l[t] = job->scan.begin;
    b_e[t] = job->scan.back;
    job->scan.done = &done;
  }
  threads_mutex_unlock(&q->mutex);

  // work while waiting
  while(done < nt) do_one_job(s, s_job_scan);

  uint64_t num_left = 0, num_right = 0;
  for(int t=0;t<nt;t++)
  {
    num_left  += right[t] - b_l[t];
    num_right += b_e[t] - right[t];
    b_r[t] = right[t];
  }

  // then merge the blocks by copying the data again :(
  // list will look something like this (blocked by thread):
  // [ l r | l r | l r | .. | l r ]
  // then determine globally from reduction:
  // [ sum(l)       | sum (r)     ]
  // and swap all displaced r and l blocks:
  // generate global swap lists and parallelise displaced (r,l) pair indices
  uint64_t num_dislocated = 0;
  for(int t=0;t<nt;t++)
  { // count all primitives classified `right' which are left of the pivot index:
    if(b_r[t] - b_l[0] >= num_left) break;
    num_dislocated += MIN(num_left, b_e[t] - b_l[0]) - (b_r[t] - b_l[0]);
  }

  // XXX TODO: find out how often this really happens.
  // XXX i think the morton order of geo verts may be upside down.
  if(num_dislocated == 0) return begin + num_left; // yay.
  // single threaded construction never gets here.

  // submit swap_job_t
  done = 0;
  threads_mutex_lock(&q->mutex);
  uint64_t read = b_r[0];
  uint64_t write = -1;
  uint64_t read_block = 0, write_block = 0;
  for(int t=0;t<nt;t++)
  {
    if(b_r[t] - b_l[0] > num_left)
    {
      write_block = t;
      write = MAX(b_l[0] + num_left, b_l[t]);
      break;
    }
  }
  for(int t=0;t<nt;t++)
  {
    int j = q->num_jobs++;
    // TODO: fix assertion
    assert(j < 3*nt);
    job_t *job = q->jobs + j;
    job->type = s_job_swap;
    uint64_t beg = ( t     *num_dislocated)/nt;
    uint64_t end = ((t+1ul)*num_dislocated)/nt;
    job->swap.read  = read;
    job->swap.write = write;
    job->swap.read_block  = read_block;
    job->swap.write_block = write_block;
    job->swap.num_read = end - beg;
    job->swap.b_l = b_l;
    job->swap.b_r = b_r;
    job->swap.b_e = b_e;
    job->swap.done = &done;
    int64_t cnt = end - beg;
    // skip for last iteration:
    if(t == nt-1) break;
    do
    { // jump over empty blocks
      int64_t remaining = b_e[read_block] - read;
      if(remaining > cnt)
      {
        read += cnt;
        cnt = 0;
      }
      else
      {
        cnt -= remaining;
        read_block++;
        assert(read_block < nt);
        read = b_r[read_block];
      }
    }
    while(cnt > 0);
    cnt = end - beg;
    do
    { // jump over empty blocks
      int64_t remaining = b_r[write_block] - write;
      if(remaining > cnt)
      {
        write += cnt;
        cnt = 0;
      }
      else
      {
        cnt -= remaining;
        write_block++;
        assert(write_block < nt);
        write = b_l[write_block];
      }
    }
    while(cnt > 0);
  }
  threads_mutex_unlock(&q->mutex);

  // work while waiting
  while(done < nt) do_one_job(s, s_job_swap);

  return begin + num_left;
}

// returns pivot element in sorted list where the split occurs begin<pivot<back
static uint64_t partition(
    qsort_t *s,
    queue_t *q,
    const uint64_t pivot, // pivot element (not an index)
    const int64_t begin, // begin of buffer
    const int64_t back)  // element after buffer
{
  if(thr.num_threads > 1 && back - begin > thr.num_threads * 100000)
    return partition_par(s, q, pivot, begin, back);

  return partition_ser(s, q, pivot, begin, back);
}


// only returns after it has actually sorted some portion of the array.
// will return the number of sorted elements
static uint64_t node_job_work(
    qsort_t *s,
    queue_t *q,
    uint64_t left,
    uint64_t right)
{
  // less than 2 elements left to sort?
  if(left + 1 >= right) return right - left;

  uint64_t pivot = s->id[(left+right)/2];
  int64_t split = partition(s, q, pivot, left, right);
  int64_t part[3] = {left, split, right};
  const int nt = thr.num_threads;

  uint64_t done = 0;
  for(int p=0;p<2;p++)
  {
    if(part[p+1] - part[p] == 1) done += 1;
    if(part[p+1] - part[p] <  2) continue;
    if(p || q->num_jobs >= nt)
    { // in this thread:
      done += node_job_work(s, q, part[p], part[p+1]);
    }
    else
    { // push job to our queue for other threads:
      threads_mutex_lock(&q->mutex);
      int j = q->num_jobs++;
      assert(j < 3*nt);
      job_t *job = q->jobs + j;
      job->type = s_job_node;
      job->node.left  = part[p];
      job->node.right = part[p+1];
      assert(job->node.left < job->node.right);
      threads_mutex_unlock(&q->mutex);
    }
  }
  return done;
}

static void *qsort_work(void *arg)
{
  qsort_t *s = arg;
  while(1)
    if(do_one_job(s, s_job_all) || (s->sorted >= s->num))
      return 0;
}

void pqsort(uint64_t *id, uint64_t num)
{
  qsort_t s = {0};
  s.num = num;
  s.id = id;
  s.sorted = 0;
  const int nt = thr.num_threads;
  s.queue = malloc(sizeof(queue_t)*nt);
  for(int k=0;k<nt;k++)
  {
    s.queue[k].num_jobs = 0;
    s.queue[k].jobs = malloc(sizeof(job_t)*3*nt);
    threads_mutex_init(&s.queue[k].mutex, 0);
  }
  // TODO: pull out this initialisation so it can be reused?
  // TODO: alloc task queues and init mutices
  // TODO: empty all queues
  // TODO: if too small just sort in this thread?

  // first job, root node:
  // threads_mutex_lock(&s.queue[0].mutex);
  s.queue[0].jobs[0].type = s_job_node;
  s.queue[0].jobs[0].node.left = 0;
  s.queue[0].jobs[0].node.right = s.num;
  s.queue[0].num_jobs = 1;
  // threads_mutex_unlock(&b->queue[0].mutex);

  for(int k=0;k<nt;k++)
    pthread_pool_task_init(thr.task + k, &thr.pool, qsort_work, &s);

  // wait for the worker threads
  pthread_pool_wait(&thr.pool);

  // TODO: tear down queues and mutices
}
