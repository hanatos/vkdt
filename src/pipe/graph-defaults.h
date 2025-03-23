#pragma once

static inline dt_token_t
dt_graph_default_input_module(
    const char *filename) // main file, not the .cfg (will strip .cfg if present)
{
  // XXX remember to also look at the copy/paste history stack routine (TODO: unify this)
  // XXX dt_gui_paste_history in gui/api_gui.h
  int len = strlen(filename);
  if(len <= 4) return 0;
  int off = 4;
  if(len > 4 && !strcasecmp(filename+len-4, ".cfg")) off = 8;
  if(!strncasecmp(filename+len-off, ".mlv", 4))
    return dt_token("i-mlv");
  if(!strncasecmp(filename+len-off, ".pfm", 4))
    return dt_token("i-pfm");
  if(!strncasecmp(filename+len-off, ".jpg", 4))
    return dt_token("i-jpg");
  if(!strncasecmp(filename+len-off, ".exr", 4))
    return dt_token("i-exr");
  if(!strncasecmp(filename+len-off, ".hdr", 4))
    return dt_token("i-hdr");
  if(!strncasecmp(filename+len-off, ".mov", 4) ||
     !strncasecmp(filename+len-off, ".mp4", 4))
    return dt_token("i-vid");
  if(len > 6 && !strncasecmp(filename+len-off-2, ".mcraw", 6))
    return dt_token("i-mcraw");
  return dt_token("i-raw"); // you assume too much
}
