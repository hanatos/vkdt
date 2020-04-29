#pragma once

#include <string.h>
#include <stdlib.h>

static inline dt_token_t
dt_read_token(
    char  *line,
    char **out)
{
  dt_token_t t = dt_token(line);
  char *cc = dt_token_str(t), *c = cc;
  while(*c != ':' && *c != '\n' && *c != 0 && c < cc+8) c++;
  *out = line + (c-cc) + 1;
  for(;c < cc+8;c++) *c = 0;
  return t;
}

static inline int
dt_read_int(char *line, char **out)
{
  const int res = strtol(line, out, 10);
  *out = *out + 1; // eat : or \n
  return res;
}

static inline float
dt_read_float(char *line, char **out)
{
  const float res = strtof(line, out);
  *out = *out + 1; // eat : or \n
  return res;
}
