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
  while(c < cc+8 && *c != ':' && *c != '\n' && *c != 0) c++;
  *out = line + (c-cc);
  if((*out)[0] == ':' || (*out)[0] == '\n') (*out)++; // don't move past 0 byte
  for(;c < cc+8;c++) *c = 0;
  return t;
}

static inline int
dt_read_int(char *line, char **out)
{
  if(line[0] == 0) return 0;
  const int res = strtol(line, out, 10);
  if(**out && *out != line) *out = *out + 1; // eat : or \n
  return res;
}

static inline float
dt_read_float(char *line, char **out)
{
  if(line[0] == 0) return 0.0f;
  const float res = strtof(line, out);
  if(**out && *out != line) *out = *out + 1; // eat : or \n
  return res;
}
