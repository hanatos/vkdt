#pragma once
#include <stdint.h>

// simple vulkan buffer memory allocator
// for the node graph. single thread use, not optimised in any sense.
// employs a free list and an allocation list, both allocation and free
// are O(n). we hope to afford it because we'll use n~=10 buffers max.

typedef struct dt_vkmem_t
{
  uint64_t offset;          // to be uploaded as uniform/push const
  uint64_t offset_orig;     // unaligned offset
  uint64_t ref  : 16;       // reference count
  uint64_t size : 48;       // only for us, the gpu will know what they asked for
  struct dt_vkmem_t *prev;  // for alloced/free lists
  struct dt_vkmem_t *next;
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

  uint64_t heap_size;
  uint64_t peak_rss;
  uint64_t rss;
  uint64_t vmsize; // <= necessary to stay within limits here!
}
dt_vkalloc_t;

void dt_vkalloc_init(dt_vkalloc_t *a, uint64_t pool_size, uint64_t bytesize);
void dt_vkalloc_cleanup(dt_vkalloc_t *a);

// allocate memory
dt_vkmem_t* dt_vkalloc(dt_vkalloc_t *a, uint64_t size, uint64_t alignment);
dt_vkmem_t* dt_vkalloc_feedback(dt_vkalloc_t *a, uint64_t size, uint64_t alignment);

// free memory
void dt_vkfree(dt_vkalloc_t *a, dt_vkmem_t *mem);

// free all the mallocs!
void dt_vkalloc_nuke(dt_vkalloc_t *a);

// perform an (expensive) internal consistency check in O(n^2)
int dt_vkalloc_check(dt_vkalloc_t *a);
