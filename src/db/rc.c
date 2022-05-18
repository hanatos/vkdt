#include "db.h"
#include "stringpool.h"
#include "rc.h"

void
dt_rc_init(
    dt_rc_t *rc)
{
  // allocate sp and data, init data[] = 0
  memset(rc, 0, sizeof(*rc));
  rc->data_max = 500;
  dt_stringpool_init(&rc->sp, rc->data_max, 20); // 500 entries with avg 20 chars
  rc->data = calloc(sizeof(rc->data[0]), rc->data_max);
}

void
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

const char*
dt_rc_get(
    dt_rc_t    *rc,
    const char *key,
    const char *defval)
{
  char tkey[30] = "str";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return defval;
  if(pos == rc->data_cnt)
  { // we inserted the entry, copy default value
    rc->data[pos] = calloc(strlen(defval)+1, 1);
    rc->data_cnt++;
    memcpy(rc->data[pos], defval, strlen(defval));
  }
  return rc->data[pos];
}

void
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

int
dt_rc_get_int(
    dt_rc_t    *rc,
    const char *key,
    const int   defval)
{
  char tkey[30] = "int";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return defval;
  if(pos == rc->data_cnt)
  {
    *(int *)(rc->data+pos) = defval;
    rc->data_cnt++;
  }
  return *(int *)(rc->data+pos);
}

void
dt_rc_set_int(
    dt_rc_t    *rc,
    const char *key,
    const int   val)
{
  char tkey[30] = "int";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(tkey+3, 26, "%s", key); // this always prints a null termination byte to truncate. gcc doesn't understand.
#pragma GCC diagnostic pop
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return; // string pool full :(
  if(pos == rc->data_cnt) rc->data_cnt++;
  ((int *)(rc->data+pos))[0] = val;
}

float
dt_rc_get_float(
    dt_rc_t    *rc,
    const char *key,
    const float defval)
{
  char tkey[30] = "flt";
  snprintf(tkey+3, 26, "%s", key);
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return defval;
  if(pos == rc->data_cnt)
  {
    *(float *)(rc->data + pos) = defval;
    rc->data_cnt++;
  }
  return *(float *)(rc->data + pos);
}

void
dt_rc_set_float(
    dt_rc_t    *rc,
    const char *key,
    const float val)
{
  char tkey[30] = "flt";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(tkey+3, 26, "%s", key);
#pragma GCC diagnostic pop
  uint32_t pos = rc->data_cnt;
  pos = dt_stringpool_get(&rc->sp, tkey, strlen(tkey), pos, 0);
  if(pos == -1u || pos >= rc->data_max) return; // string pool full :(
  if(pos == rc->data_cnt) rc->data_cnt++;
  ((float *)(rc->data+pos))[0] = val;
}

// read colon separated list of key:value strings
int
dt_rc_read(
    dt_rc_t    *rc,
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return -1;
  char line[4000];
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
      dt_rc_set_float(rc, line+3, atof(val));
    else if(!strncmp(line, "int", 3))
      dt_rc_set_int(rc, line+3, atol(val));
    else if(!strncmp(line, "str", 3))
      dt_rc_set(rc, line+3, val);
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
int
dt_rc_write(
    dt_rc_t    *rc,
    const char *filename)
{
  FILE *f = fopen(filename, "wb");
  if(!f) return -1;
  for(int i=0;i<rc->sp.buf_max;)
  {
    int len = strlen(rc->sp.buf+i);
    if(len)
    { // query value of this string:
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
    }
    i += len+1; // eat terminating 0, too
  }
  return 0;
}
