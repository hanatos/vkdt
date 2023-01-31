#pragma once
#include "fs.h"
#include <unistd.h>

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
    "cli", "eval-profile", "fit", "lutinfo", "mkclut", "mkssf", "gallery", "noise-profile", // XXX read-icc
  };
  for(int i=0;i<sizeof(cmds)/sizeof(cmds[0]);i++)
    if(!strcmp(argv[1], cmds[i])) { cmd = cmds[i]; break; }
  if(!cmd) return; // no such tool
  char filename[256];
  snprintf(filename, sizeof(filename), "%s/vkdt-%s", basedir, cmd);
  if(execv(filename, argv+1) < 0)
    fprintf(stderr, "[vkdt] dispatch tool: failed to execute %s\n", filename);
}
