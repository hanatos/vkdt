#pragma once
#include "db.h" // for stringpool type

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

void dt_rc_init(dt_rc_t *rc);
void dt_rc_cleanup(dt_rc_t *rc);

const char* dt_rc_get(
    dt_rc_t    *rc,
    const char *key,
    const char *defval);

void dt_rc_set(
    dt_rc_t    *rc,
    const char *key,
    const char *val);

int dt_rc_get_int(
    dt_rc_t    *rc,
    const char *key,
    const int   defval);

void dt_rc_set_int(
    dt_rc_t    *rc,
    const char *key,
    const int   val);

float dt_rc_get_float(
    dt_rc_t    *rc,
    const char *key,
    const float defval);

void dt_rc_set_float(
    dt_rc_t    *rc,
    const char *key,
    const float val);

// read colon separated list of key:value strings
int dt_rc_read(
    dt_rc_t    *rc,
    const char *filename);

// write config file. walk all existing strings in pool, query data,
// write out in ascii
int dt_rc_write(
    dt_rc_t    *rc,
    const char *filename);
