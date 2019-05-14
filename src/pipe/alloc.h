#pragma once
#include <stdint.h>

// simple vulkan buffer memory allocator
// for the node graph. single thread use, not optimised in any sense.

typedef struct dt_vkmem_t
{
  uint64_t offset;   // to be uploaded as uniform/push const
  uint64_t size;     // only for us, the gpu will know what they asked for
  dt_vkmem_t *prev;  // for alloced/free lists
  dt_vkmem_t *next;
}
dt_vkmem_t;

typedef struct dt_vkalloc_t
{
  dt_vkmem_t *used;
  dt_vkmem_t *free;

  // fixed size pool of dt_vkmem_t to not fragment our real heap with this nonsense:
  uint64_t pool_size;
  dt_vkmem_t *vkmem_pool; // fixed size pool allocation
  dt_vkmem_t *unused;     // linked list into the above which are neither used nor free

  uint64_t peak_rss;
  uint64_t rss;
  uint64_t vmsize; // <= necessary to stay within limits here!
}
dt_vkalloc_t;

static inline void
dt_vkalloc_init(dt_vkalloc_t *a)
{
  const uint64_t heap_size = 1ul<<40; // doesn't matter, we'll never allocate this for real
  memset(a, 0, sizeof(*a));
  a->pool_size = 100;
  a->vkmem_pool = malloc(sizeof(dt_vkmem_t)*a->pool_size);
  memset(a->vkmem_pool, 0, sizeof(dt_vkmem_t)*a->pool_size); 
  a->free = DLIST_PREPEND(a->free, a->vkmem_pool);
  a->free->offset = 0;
  a->free->size = heap_size;
  for(int i=1;i<a->pool_size;i++)
    a->unused = DLIST_PREPEND(a->unused, a->vkmem_pool+i);
}

static inline void
dt_vkalloc_cleanup(dt_vkalloc_t *a)
{
  // free whole thing
  free(a->vkmem_pool);
  // don't free a, it's owned externally
  memset(a, 0, sizeof(*a));
}

// perform an (expensive) internal consistency check
static inline int
dt_vkalloc_check(dt_vkalloc_t *a)
{
  // TODO: count number of elements in linked lists
  // TODO: make sure free list refs sorted blocks of memory
  // TODO: make sure every pointer in pool is ref'd exactly once
  // TODO: make sure no memory block is ref'd twice O(n^2)
  return 0;
}

static inline dt_vkmem_t*
dt_vkalloc(uint64_t size)
{
  // TODO: linear scan through free list
  dt_vkmem_t *l = a->free;
  while(l)
  {
    if(l->size == size) break; // done
    // TODO: if not then chop up the largest one?
    if(l->size > size) break;
    l = l->next;
  }
  if(!l) return 0; // ouch
  // TODO: put inline above?
  // TODO: get large enough block, either move to used list or cut off the portion we need
  // TODO: and push that to used list (split the block)
  if(l->size > size)
  { // grab new mem entry from unused list
    assert(a->unused);
    vk_mem_t *mem = a->unused;
    a->unused = DLIST_REMOVE(a->unused, mem); // remove first is O(1)
    // split, push to used and modify free entry
    mem->offset = l->offset;
    mem->size = size;
    l->size -= size;
    l->offset += size;
    a->used = DLIST_PREPEND(a->used, mem);
  }
  // TODO: rss += size, check peak_rss
  // TODO: error if no large enough block found
}

static inline void
dt_vkfree(dt_vkmem_t *mem)
{
  // remove from used list, put back to free list.
  // TODO adjust rss and vmsize if applicable
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
          DLIST_RM_ELEMENT(mem);
          a->unused = DLIST_PREPEND(a->unused, mem);
          mem = t;
        }
      }
      return; // done
    }
  }
  while(l && (l = l->next));
  assert(0 && "free failed??");
}

