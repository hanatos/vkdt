#include "modules/api.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-phm] writing '%s'\n", basename);
  uint16_t *p16 = buf;

  const size_t width  = module->connector[0].roi.wd;
  const size_t height = module->connector[0].roi.ht;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.phm", basename);
  FILE* f = fopen(filename, "wb");
  if(f)
  {
    // align pfm header to sse, assuming the file will
    // be mmapped to page boundaries.
    fprintf(f, "PH\n%zu %zu\n-1.0\n", width, height);
    for(size_t k=0;k<width*height;k++)
    {
      uint16_t out[3] = {p16[3*k+0], p16[3*k+1], p16[3*k+2]};
      fwrite(out, sizeof(uint16_t), 3, f);
    }
    fclose(f);
  }
}
