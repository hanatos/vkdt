#include "alloc.h"
#include "core/core.h"
#include "dlist.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

void
dt_vkalloc_init(dt_vkalloc_t *a)
{
  memset(a, 0, sizeof(*a));
  a->heap_size = 1ul<<40; // doesn't matter, we'll never allocate this for real
  a->pool_size = 100;
  a->vkmem_pool = malloc(sizeof(dt_vkmem_t)*a->pool_size);
  dt_vkalloc_nuke(a);
}

void
dt_vkalloc_cleanup(dt_vkalloc_t *a)
{
  // free whole thing
  free(a->vkmem_pool);
  // don't free a, it's owned externally
  memset(a, 0, sizeof(*a));
}

void
dt_vkalloc_nuke(dt_vkalloc_t *a)
{
  memset(a->vkmem_pool, 0, sizeof(dt_vkmem_t)*a->pool_size); 
  a->free = a->used = a->unused = 0;
  a->free = DLIST_PREPEND(a->free, a->vkmem_pool);
  a->free->offset = 0;
  a->free->size = a->heap_size;
  for(int i=1;i<a->pool_size;i++)
    a->unused = DLIST_PREPEND(a->unused, a->vkmem_pool+i);
  a->peak_rss = a->rss = a->vmsize = 0ul;
  assert(!dt_vkalloc_check(a));
}

dt_vkmem_t*
dt_vkalloc(dt_vkalloc_t *a, uint64_t size, uint64_t alignment)
{
  assert(!dt_vkalloc_check(a));
  // linear scan through free list O(n)
  dt_vkmem_t *l = a->free;
  while(l)
  {
    dt_vkmem_t *mem = 0;
    if(l->size == size && !(l->offset & (alignment-1)))
    { // replace entry
      mem = l;
      if(l == a->free) a->free = l->next;
      DLIST_RM_ELEMENT(mem);
    }
    else if((l->size > size && !(l->offset & (alignment-1))) ||
        l->size > size + alignment)
    { // grab new mem entry from unused list
      assert(a->unused && "vkalloc: no more free slots!");
      if(!a->unused) return 0;
      mem = a->unused;
      assert(!dt_vkalloc_check(a));
      a->unused = DLIST_REMOVE(a->unused, mem); // remove first is O(1)
      // split, push to used and modify free entry.
      // we'll just forget about the bits in between our base pointer
      // and the requested alignment. we rely on nuking the memory pool
      // very often anyways. plus, what's a few kilobytes among friends.
      size_t end = l->offset + l->size;
      mem->offset = (l->offset & ~(alignment-1)) + alignment;
      mem->size = size;
      l->offset = mem->offset + mem->size;
      l->size = end - l->offset;
    }

    if(mem)
    {
      a->rss += mem->size;
      a->peak_rss = MAX(a->peak_rss, a->rss);
      a->vmsize = MAX(a->vmsize, mem->offset + mem->size);
      a->used = DLIST_PREPEND(a->used, mem);
      mem->ref = 1;
      assert(!dt_vkalloc_check(a));
      return mem;
    }
    l = l->next;
  }
  assert(0 && "out of memory slots!");
  return 0;
}

void
dt_vkfree(dt_vkalloc_t *a, dt_vkmem_t *mem)
{
  assert(!dt_vkalloc_check(a));
  assert(mem->ref > 0);
  if(mem->ref)
  {
    mem->ref--;
    if(mem->ref) return; // don't free if still referenced
  }
  else return; // no ref count: already freed
  // remove from used list, put back to free list.
  a->rss -= mem->size;
  a->used = DLIST_REMOVE(a->used, mem);
  dt_vkmem_t *l = a->free;
  do
  {
    // keep sorted
    if(!l || l->offset >= mem->offset + mem->size)
    {
      dt_vkmem_t *t = DLIST_PREPEND(l, mem);
      if(l == a->free) a->free = t; // keep consistent
      // merge blocks:
      t = mem->prev;
      if(t)
      { // merge with before
        if(t->offset + t->size == mem->offset)
        {
          t->size += mem->size;
          if(a->free == mem) a->free = mem->next;
          DLIST_RM_ELEMENT(mem);
          a->unused = DLIST_PREPEND(a->unused, mem);
          mem = t;
        }
      }
      t = mem->next;
      if(t)
      { // merge with after
        if(t->offset == mem->offset + mem->size)
        {
          t->offset = mem->offset;
          t->size += mem->size;
          if(a->free == mem) a->free = mem->next;
          DLIST_RM_ELEMENT(mem);
          a->unused = DLIST_PREPEND(a->unused, mem);
          mem = t;
        }
      }
      assert(!dt_vkalloc_check(a));
      return; // done
    }
  }
  while(l && (l = l->next));
  assert(0 && "vkalloc: inconsistent free list!");
}

// perform an (expensive) internal consistency check in O(n^2)
int
dt_vkalloc_check(dt_vkalloc_t *a)
{
  // check list integrity:
  dt_vkmem_t *l = a->free;
  if(l)
  {
    if(l->prev) return 9;
    while(l)
    {
      if(l->next && l->next->prev != l) return 10;
      l = l->next;
    }
  }
  l = a->used;
  if(l)
  {
    if(l->prev) return 11;
    while(l)
    {
      if(l->next && l->next->prev != l) return 12;
      l = l->next;
    }
  }
  l = a->unused;
  if(l)
  {
    if(l->prev) return 13;
    while(l)
    {
      if(l->next && l->next->prev != l) return 14;
      l = l->next;
    }
  }

  // count number of elements in linked lists
  uint64_t num_used = DLIST_LENGTH(a->used);
  uint64_t num_free = DLIST_LENGTH(a->free);
  uint64_t num_unused = DLIST_LENGTH(a->unused);
  if(num_used + num_free + num_unused != a->pool_size)
  {
    fprintf(stderr, "used %lu free %lu unused %lu\n", num_used, num_free, num_unused);
    return 1;
  }

  // make sure free list refs sorted blocks of memory
  // also make sure they don't overlap
  l = a->free;
  uint64_t pos = 0, next_pos = 0;
  while(l)
  {
    if(l->offset < pos) return 2;
    if(l->offset + l->size < next_pos) return 3;
    pos = l->offset;
    next_pos = l->offset + l->size;
    l = l->next;
  }

  // make sure every pointer in pool is ref'd exactly once
  uint8_t *mark = alloca(a->pool_size*sizeof(uint8_t));
  memset(mark, 0, sizeof(uint8_t)*a->pool_size);
  for(int i=0;i<3;i++)
  {
    l = a->used;
    if(i == 1) l = a->free;
    if(i == 2) l = a->unused;
    while(l)
    {
      int m = l - a->vkmem_pool;
      if(mark[m]) return 4; // already marked
      mark[m] = 1;
      l = l->next;
    }
  }
  for(int i=0;i<a->pool_size;i++)
    if(!mark[i]) return 5; // we lost one entry!

  // make sure no memory block is ref'd twice O(n^2)
  for(int i=0;i<2;i++)
  {
    l = a->used;
    if(i == 1) l = a->free;
    while(l)
    {
      for(int i=0;i<2;i++)
      {
        dt_vkmem_t *l2 = a->used;
        if(i == 1) l2 = a->free;
        while(l2)
        {
          int good = 0;
          if(l == l2) good = 1;
          if(l->offset >= l2->offset + l2->size) good = 1;
          if(l->offset +  l->size <= l2->offset) good = 1;
          if(!good) return 6; // overlap detected!
          l2 = l2->next;
        }
      }
      l = l->next;
    }
  }

  // see whether rss and vmsize are lying to us:
  l = a->used;
  uint64_t rss = 0, vmsize = 0;
  while(l)
  {
    vmsize = MAX(vmsize, l->offset+l->size);
    rss += l->size;
    l = l->next;
  }
  if(vmsize > a->vmsize) return 7;
  if(rss != a->rss) return 8;

  return 0; // yay, we made it!
}

