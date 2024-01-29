#pragma once
#include "fs.h"
#include <unistd.h>

static inline void
dt_tool_print_usage()
{
  fprintf(stderr, "[vkdt] dispatch tool usage:\n"
                  "vkdt cli           -- command line interface\n"
                  "vkdt fit           -- parameter optimisation\n"
                  "vkdt lutinfo       -- print .lut file metadata\n"
                  "vkdt mkssf         -- estimate ssf from dcp\n"
                  "vkdt mkclut        -- create a clut camera input profile\n"
                  "vkdt eval-profile  -- evaluate a clut profile\n"
                  "vkdt gallery       -- create a static web gallery from jpg\n"
                  "vkdt read-icc      -- create a display profile from matrix icc\n"
                  "vkdt noise-profile -- create a noise profile for a given raw\n");
}

// dispatch execution to tools for convenient commandline interaction
// if successful, this never returns
static inline void
dt_tool_dispatch(int argc, char *argv[])
{
  if(argc < 2) return; // no arguments to parse
  char basedir[256];
  fs_basedir(basedir, sizeof(basedir));
  const char *cmd = 0;
  const char *cmds[] = {
    "cli", "eval-profile", "fit", "lutinfo", "mkclut", "mkssf", "gallery", "noise-profile", "read-icc",
  };
  for(int i=0;i<sizeof(cmds)/sizeof(cmds[0]);i++)
    if(!strcmp(argv[1], cmds[i])) { cmd = cmds[i]; break; }
  if(!cmd) return; // no such tool
  char filename[256];
  if(snprintf(filename, sizeof(filename), "%s/vkdt-%s", basedir, cmd) >= sizeof(filename))
    goto error;
  if(execv(filename, argv+1) < 0)
    goto error;
  return; // never reached
error:
  fprintf(stderr, "[vkdt] dispatch tool: failed to execute %s\n", filename);
}
