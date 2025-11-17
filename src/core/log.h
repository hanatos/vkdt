#pragma once
#include "pipe/token.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

// layered logging facility

typedef enum dt_log_mask_t
{
  s_log_none = 0,
  s_log_qvk  = 1<<0,
  s_log_pipe = 1<<1,
  s_log_gui  = 1<<2,
  s_log_db   = 1<<3,
  s_log_cli  = 1<<4,
  s_log_snd  = 1<<5,
  s_log_perf = 1<<6,
  s_log_mem  = 1<<7,
  s_log_ray  = 1<<8,
  s_log_err  = 1<<9,
  s_log_all  = -1ul,
}
dt_log_mask_t;

typedef struct dt_log_t
{
  dt_log_mask_t mask;
}
dt_log_t;

#ifndef VKDT_DSO_BUILD
extern VKDT_API dt_log_t dt_log_global;
#endif

// this can be done to parse "-d level" from
// the command line to add more verbose output
// or "-D level" to remove a specific level again (like "-d all -D perf")
// returns the index to the last argument that has been used here.
static inline int
dt_log_init_arg(int argc, char *argv[])
{
  const char *id[] = {
    "none",
    "qvk",
    "pipe",
    "gui",
    "db",
    "cli",
    "snd",
    "perf",
    "mem",
    "ray",
    "err",
    "all",
  };
  int num = sizeof(id)/sizeof(id[0]);
  uint64_t verbose = s_log_err; // error only by default
  int lastarg = 0; // will return the last index we used
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-d") && i < argc-1)
    {
      lastarg = ++i;
      for(int j=0;j<num;j++)
      {
        if(!strcmp(argv[i], id[j]))
        {
          if(j == 0)          verbose = 0ul;
          else if(j == num-1) verbose = -1ul;
          else                verbose |= 1<<(j-1);
        }
      }
    }
    else if(!strcmp(argv[i], "-D") && i < argc-1)
    {
      lastarg = ++i;
      for(int j=0;j<num;j++)
      {
        if(!strcmp(argv[i], id[j]))
        {
          if(j == num-1) verbose = 0ul;
          else           verbose &= ~(1<<(j-1));
        }
      }
    }
  }

  // user parameters add to the mask:
#ifdef __cplusplus // now comes the shit code. whoever invented that should be %^&
  dt_log_global.mask = static_cast<dt_log_mask_t>(dt_log_global.mask | verbose);
  if(verbose == 0ul) dt_log_global.mask = static_cast<dt_log_mask_t>(verbose);
#else
  dt_log_global.mask |= verbose;
  if(verbose == 0ul) dt_log_global.mask = verbose;
#endif
  return lastarg;
}

// call this first once
static inline void
dt_log_init(dt_log_mask_t verbose)
{
#ifdef __ANDROID__
  dt_log_global.mask = s_log_all;
#else
  dt_log_global.mask = verbose;
#endif
}

static inline void
dt_log(
    dt_log_mask_t mask,
    const char *format,
    ...)
{
  const char *pre[] = {
    "",
    "[qvk]",
    "[pipe]",
    "[gui]",
    "[db]",
    "[cli]",
    "[snd]",
    "[perf]",
    "[mem]",
    "[ray]",
    "\033[31m[ERR]\033[0m",
  };

  if(dt_log_global.mask & mask)
  {
    uint32_t index = mask ? 32-__builtin_clz(mask) : 0;
    if(index > sizeof(pre)/sizeof(pre[0])) index = 0;
    va_list args;
    va_start(args, format);
    char str[2048];
#ifdef __ANDROID__ // android logs are so busy, make sure we can grep for [vkdt]:
    snprintf(str, sizeof(str), "[vkdt] %s\n", format);
     __android_log_vprint(ANDROID_LOG_WARN, pre[index], str, args);
#else
    snprintf(str, sizeof(str), "%s %s\n", pre[index], format);
    vfprintf(stdout, str, args);
#endif
    va_end(args);
  }
}
