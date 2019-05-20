#pragma once

#include <string.h>
#include <stdlib.h>

static inline dt_token_t
dt_read_token(
    char *line, // will be destroyed in the process, we'll insert 0 as end point
    char **out)
{
  char *c = line;
  while(*c != ':' && *c != '\n' && *c != 0 && c < line+8) c++;
  *c = 0;
  dt_token_t t = dt_token(line);
  *out = c+1;
  return t;
}

static inline int
dt_read_int(char *line, char **out)
{
  return strtol(line, out, 10);
}

static inline float
dt_read_float(char *line, char **out)
{
  return strtof(line, out);
}
