#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
// some random helpers

#define MIN(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })
#define MAX(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a > _b ? _a : _b; })
#define CLAMP(a,m,M) (MIN(MAX((a), (m)), (M)))

// allocates aligned, also copies the old data, frees the old buffer p.
// does nothing if addr is within current size.
// returns new allocation.
static inline void*
dt_realloc(void *p, uint64_t *size, uint64_t addr)
{
  if(*size > addr) return p;
  const uint64_t align = 32;
  uint64_t nsize = ((16+2*addr + align-1)/align)*align;
  void *n = aligned_alloc(32, nsize);
  if(*size) memcpy(n, p, *size);
  *size = nsize;
  free(p);
  return n;
}
