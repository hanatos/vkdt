#pragma once

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
  dt_vkmem_t *vkmem_pool; // fixed size pool allocation
  dt_vkmem_t *unused;     // linked list into the above which are neither used nor free

  uint64_t peak_rss;
  uint64_t rss;
  uint64_t vmsize; // <= necessary to stay within limits here!
}
dt_vkalloc_t;

static inline dt_vkalloc_t *
dt_vkalloc_init()
{
  dt_vkalloc_t *a = malloc(sizeof(dt_vkalloc_t));
  // TODO:
  rss = peak_rss = 0;
  free = (0, max_size);
  used = 0;
}

static inline void
dt_vkalloc_cleanup(dt_vkalloc_t *)
{
  // TODO: free whole thing
}

static inline dt_vkmem_t*
dt_vkalloc(uint64_t size)
{
  // TODO: linear scan through free list
  dt_vkmem_t *l = a->free;
  while(l)
  {
    if(l->size == size) break; // done
    // TODO: if not then chop up the largest one
    l = l->next;
  }
  if(!l) return 0; // ouch
  // TODO: get large enough block, either move to used list or cut off the portion we need
  // TODO: and push that to used list (split the block)
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
          // TODO: push mem back to unused list
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
          // TODO: push mem back to unused list
          mem = t;
        }
      }
    }
  }
  while(l && (l = l->next));
  // TODO: some consistency check in another O(n)?
}

