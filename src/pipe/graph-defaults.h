#pragma once

static inline dt_token_t
dt_graph_default_input_module(
    const char *filename) // main file, not the .cfg (will strip .cfg if present)
{
  int len = strlen(filename);
  if(len <= 4) return 0;
  int off = 4;
  if(len > 4 && !strcasecmp(filename+len-4, ".cfg")) off = 8;
  if(!strcasecmp(filename+len-off, ".mlv"))
    return dt_token("i-mlv");
  if(!strcasecmp(filename+len-off, ".pfm"))
    return dt_token("i-pfm");
  if(!strcasecmp(filename+len-off, ".jpg"))
    return dt_token("i-jpg");
  if(!strcasecmp(filename+len-off, ".mov"))
    return dt_token("i-vid");
  return dt_token("i-raw"); // you assume too much
}
