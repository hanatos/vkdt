#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// layered logging facility

typedef enum dt_log_mask_t
{
  s_log_none = 0,
  s_log_qvk  = 1<<0,
  s_log_pipe = 1<<1,
  s_log_gui  = 1<<2,
  s_log_db   = 1<<3,
  s_log_cli  = 1<<4,
  s_log_err  = 1<<5,
  s_log_all  = -1ul,
}
dt_log_mask_t;

typedef struct dt_log_t
{
  dt_log_mask_t mask;
}
dt_log_t;

extern dt_log_t dt_log_global;

// this can be done to parse "-d level" from
// the command line to add more verbose output
static inline void
dt_log_init_arg(int argc, char *argv[])
{
  const char *id[] = {
    "none",
    "qvk",
    "pipe",
    "gui",
    "db",
    "cli",
    "err",
    "all",
  };
  uint64_t verbose = 0ul;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-d") && i < argc-1)
    {
      i++;
      for(int j=0;j<sizeof(id)/sizeof(id[0]);j++)
      {
        if(!strcmp(argv[i], id[j]))
        {
          if(j == 0)      verbose = 0ul;
          else if(j == 7) verbose = -1ul;
          else            verbose |= 1<<(j-1);
        }
      }
    }
  }
  // user parameters add to the mask:
  dt_log_global.mask |= verbose;
}

// call this first once
static inline void
dt_log_init(dt_log_mask_t verbose)
{
  dt_log_global.mask = verbose;
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
    "[ERR]",
  };

  if(dt_log_global.mask & mask)
  {
    char str[2048];
    int index = mask ? __builtin_ctz(mask)+1 : 0;
    if(index > sizeof(pre)/sizeof(pre[0])) index = 0;
    snprintf(str, sizeof(str), "%s %s\n", pre[index], format);
    va_list args;
    va_start(args, format);
    vfprintf(stdout, str, args);
    va_end(args);
  }
}
