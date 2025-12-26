#pragma once
#include "fs.h"

static inline void
dt_exiftool_get_binary(char *cmd, size_t cmd_size)
{
  const char *paths[] = {"/usr/bin", "/usr/bin/vendor_perl", "/usr/local/bin", dt_pipe.basedir };
  for(int k=0;k<4;k++)
  {
    char test[PATH_MAX];
    char cmd_real[PATH_MAX] = {0};
#ifdef _WIN64
    snprintf(test, sizeof(test), "%s/exiftool.exe", paths[k]);
#else
    snprintf(test, sizeof(test), "%s/exiftool", paths[k]);
#endif
    if(fs_isreg_file(test))
    { // found at least a file here
      fs_realpath(test, cmd_real); // use GNU extension: fill path even if it doesn't exist
      // convert backslashes to slashes before dt_sanitize_user_string()
      for(int i=0;cmd_real[i]!=0;i++) if(cmd_real[i] == '\\') cmd_real[i] = '/';
      snprintf(cmd, cmd_size, "%s", cmd_real);
      return;
    }
  }
}
