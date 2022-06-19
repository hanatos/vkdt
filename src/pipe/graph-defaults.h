#pragma once

static inline dt_token_t
dt_graph_default_input_module(
    const char *filename) // main file, not the .cfg
{
  int len = strlen(filename);
  if(len <= 4) return 0;
  if(!strcasecmp(filename+len-4, ".mlv"))
    return dt_token("i-mlv");
  if(!strcasecmp(filename+len-4, ".pfm"))
    return dt_token("i-pfm");
  if(!strcasecmp(filename+len-4, ".jpg"))
    return dt_token("i-jpg");
  if(!strcasecmp(filename+len-4, ".mov"))
    return dt_token("i-vid");
  return dt_token("i-raw"); // you assume too much
}
