#pragma once
#include "db.h" // for stringpool type
#include "stringpool.h"

// vkdtrc file support. this reads/writes a colon separated key:value store.
// it supports getting and setting values, but is not thread-safe.
// this should thus only be used by the main/gui thread.

typedef struct dt_rc_t
{
  dt_stringpool_t sp;  // links key string to value uint32_t.
  char    **data;      // data[value] is the string associated with the key.
  uint32_t  data_max;  // allocation size of data[] (number of elements, not bytes)
  uint32_t  data_cnt;  // number of elements in data[]
}
dt_rc_t;

static inline void
dt_rc_init(
    dt_rc_t *rc)
{
  // allocate sp and data, init data[] = 0
  memset(rc, 0, sizeof(*rc));
  rc->data_max = 500;
  dt_stringpool_init(&rc->sp, rc->data_max, 20); // 500 entries with avg 20 chars
  rc->data = calloc(sizeof(rc->data[0]), rc->data_max);
}

static inline void
dt_rc_cleanup(
    dt_rc_t *rc)
{
  // free all allocated string values.
  // for this find all strings in string pool:
  for(int i=0;i<rc->sp.buf_max;)
  {
    int len = strlen(rc->sp.buf+i);
    // read string
    if(!strncmp(rc->sp.buf+i, "str", 3))
    {
      uint32_t pos = dt_stringpool_get(&rc->sp, rc->sp.buf+i, len, -1u, 0);
      if(pos != -1u && pos < rc->data_max)
      {
        free(rc->data[pos]);
        rc->data[pos] = 0;
      }
    }
    i += len+1; // eat terminating 0, too
  }
  free(rc->data);
  rc->data_cnt = rc->data_max = 0;
  dt_stringpool_cleanup(&rc->sp);
}

static inline const char*
dt_rc_get(
    dt_rc_t    *rc,
    const char *key,
    const char *defval)
{
  char tkey[30] = "str";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), -1u, 0);
  if(pos == -1u || pos >= rc->data_max) return defval;
  return rc->data[pos];
}

static inline void
dt_rc_set(
    dt_rc_t    *rc,
    const char *key,
    const char *val)
{
  char tkey[30] = "str";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return; // string pool full :(
  free(rc->data[pos]);
  rc->data[pos] = calloc(strlen(val)+1, 1);
  if(pos == rc->data_cnt) rc->data_cnt++;
  memcpy(rc->data[pos], val, strlen(val));
}

static inline int
dt_rc_get_int(
    dt_rc_t    *rc,
    const char *key,
    const int   defval)
{
  char tkey[30] = "int";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), -1u, 0);
  if(pos == -1u || pos >= rc->data_max) return defval;
  return *(int *)(rc->data+pos);
}

static inline void
dt_rc_set_int(
    dt_rc_t    *rc,
    const char *key,
    const int   val)
{
  char tkey[30] = "int";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return; // string pool full :(
  if(pos == rc->data_cnt) rc->data_cnt++;
  ((int *)(rc->data+pos))[0] = val;
}

static inline float
dt_rc_get_float(
    dt_rc_t    *rc,
    const char *key,
    const float defval)
{
  char tkey[30] = "flt";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), -1u, 0);
  if(pos == -1u || pos >= rc->data_max) return defval;
  return *(float *)(rc->data + pos);
}

static inline void
dt_rc_set_float(
    dt_rc_t    *rc,
    const char *key,
    const float val)
{
  char tkey[30] = "flt";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return; // string pool full :(
  if(pos == rc->data_cnt) rc->data_cnt++;
  ((float *)(rc->data+pos))[0] = val;
}

// read colon separated list of key:value strings
static inline int
dt_rc_read(
    dt_rc_t    *rc,
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return -1;
  char line[100];
  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%99[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    lno++;

    char *val = line;
    while(val < line + sizeof(line) - 1 && *val != ':') val++;
    (val++)[0] = 0;

    if(!strncmp(line, "flt", 3))
      dt_rc_set_float(rc, line, atof(val));
    else if(!strncmp(line, "int", 3))
      dt_rc_set_float(rc, line, atol(val));
    else if(!strncmp(line, "str", 3))
      dt_rc_set(rc, line, val);
    else goto error;
  }

  fclose(f);
  return 0;
error:
  fclose(f);
  return lno;
}

// write config file. walk all existing strings in pool, query data,
// write out in ascii
static inline int
dt_rc_write(
    dt_rc_t    *rc,
    const char *filename)
{
  FILE *f = fopen(filename, "wb");
  if(!f) return -1;
  for(int i=0;i<rc->sp.buf_max;)
  {
    int len = strlen(rc->sp.buf+i);
    // query value of this string:
    uint32_t pos = dt_stringpool_get(&rc->sp, rc->sp.buf+i, len, -1u, 0);
    if(pos != -1u && pos < rc->data_max)
    {
      if(!strncmp(rc->sp.buf+i, "flt", 3))
        fprintf(f, "%s:%g\n", rc->sp.buf+i, *(float *)(rc->data+pos));
      else if(!strncmp(rc->sp.buf+i, "int", 3))
        fprintf(f, "%s:%d\n", rc->sp.buf+i, *(int *)(rc->data+pos));
      else if(!strncmp(rc->sp.buf+i, "str", 3))
        fprintf(f, "%s:%s\n", rc->sp.buf+i, rc->data[pos]);
    }
    i += len+1; // eat terminating 0, too
  }
  return 0;
}
