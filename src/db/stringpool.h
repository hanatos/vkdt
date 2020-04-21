#pragma once

#include "murmur3.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// string pool. this serves two purposes:
// store hashtable string -> id (e.g. for database to associate file names with imageid)
// store null-terminated strings themselves in a compact memory layout (locality of reference)

typedef struct dt_stringpool_entry_t
{
  uint32_t next; // -1u no next, else: next index
  uint32_t val;
  char    *buf;  // null terminated
}
dt_stringpool_entry_t;

static inline void
dt_stringpool_init(
    dt_stringpool_t *sp,
    uint32_t num_entries, // number of entries. will add 20% extra.
    uint32_t avg_len)     // assume average string length. filenames straight from cam are 12.
{
  num_entries += num_entries / 5; // add 20% slack
  size_t buf_size = num_entries * avg_len;
  memset(sp, 0, sizeof(*sp));
  sp->entry_max = num_entries;
  sp->entry     = malloc(sizeof(dt_stringpool_entry_t)*num_entries);
  sp->buf       = malloc(buf_size);
  sp->buf_max   = buf_size;
  sp->buf_cnt   = 0;
}

static inline void
dt_stringpool_cleanup(dt_stringpool_t *sp)
{
  free(sp->entry);
  free(sp->buf);
}

// return primary key (may be different to what was passed in case it was already there)
static inline uint32_t
dt_stringpool_get(
    dt_stringpool_t *sp,    // string pool
    const char      *str,   // string
    uint32_t         sl,    // string length (you can cut it short to compute the hash only on the leading chars)
    uint32_t         val)   // primary key to associate with the string, in case it's not been inserted before. pass -1u if you don't want to insert.
{
  const uint32_t seed = 1337;
  uint32_t j = murmur_hash3(str, sl, seed);
  int step = 1;
  while(1)
  {
    j = j % sp->entry_max;
    dt_stringpool_entry_t *entry = sp->entry + j;
    if(entry->buf)
    { // entry already taken
      if(!strncmp(entry->buf, str, sl))
        return entry->val; // this is us, we have been inserted before
      // else jump to next candidate
      if(entry->next == -1u)
      {
        entry->next = j + step;
        step *= 2;
      }
      j = entry->next;
    }
    else
    { // free entry found, allocate string:
      if(val == -1u) return -1u; // no insert requested
      if(sp->buf_cnt + sl >= sp->buf_max)
      {
        fprintf(stderr, "[stringpool] ran out of memory!\n");
        return -1u; // would need +one for null terminator
      }
      entry->buf   = sp->buf + sp->buf_cnt;
      sp->buf_cnt += sl+1;
      entry->next  = -1u;
      entry->val   = val;
      strncpy(entry->buf, str, sl);
      entry->buf[sl] = 0; // explicitly null-terminate (potentially again, can't hurt)
      return entry->val;
    }
  }
  // yes, as if you can reach here. good luck.
  return val;
}
