#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
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

static inline float dt_tofloat(uint32_t i)
{
  union { uint32_t i; float f; } u = {.i=i};
  return u.f;
}

static inline uint32_t dt_touint(float f)
{
  union { float f; uint32_t i; } u = {.f=f};
  return u.i;
}

static inline double dt_time()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return time.tv_sec - 1290608000 + (1.0/1000000.0)*time.tv_usec;
}
