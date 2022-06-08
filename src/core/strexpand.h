#pragma once

#include <string.h>

// pattern replaces ${variable key name} in strings
// by a value strings. the pattern and the output strings
// have to be allocated by the caller. the keys and values
// are passed as arrays of char* where the last entry
// of key has to be a null pointer, and val[i] has to
// be valid for every key[i].
static inline void
dt_strexpand(
    const char  *pattern,
    size_t       pattern_size,
    char        *output,
    size_t       output_size,
    const char **key,         // 0 terminated list
    const char **val)
{
  uint32_t j = 0;
  for(uint32_t i=0;i<pattern_size && j<output_size;i++)
  {
    if(pattern[i] == '$' && pattern[i+1] == '{')
    { // replace ${key} by val
      uint32_t end = i+1;
      for(;end < pattern_size && pattern[end] != '}';end++);
      if(end < pattern_size)
      { // compare all keys and replace by value
        for(int k=0;key[k];k++)
        {
          if(!strncmp(pattern+i+2, key[k], end-i-2))
          {
            size_t d = snprintf(output+j, output_size, "%s", val[k]);
            output_size -= d;
            j += d;
            break;
          }
        }
      }
      i = end+1;
    }
    output[j++] = pattern[i];
  }
  output[j] = 0;
}
