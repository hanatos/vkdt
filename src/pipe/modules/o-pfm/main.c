#include "modules/api.h"

#include <stdio.h>
#include <string.h>

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-pfm] writing '%s'\n", basename);
  float *pf = buf;

  const int width  = module->connector[0].roi.wd;
  const int height = module->connector[0].roi.ht;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.pfm", basename);
  FILE* f = fopen(filename, "wb");
  if(f)
  {
    // align pfm header to sse, assuming the file will
    // be mmapped to page boundaries.
    char header[1024];
    snprintf(header, 1024, "PF\n%d %d\n-1.0", width, height);
    size_t len = strlen(header);
    fprintf(f, "PF\n%d %d\n-1.0", width, height);
    ssize_t off = 0;
    while((len + 1 + off) & 0xf) off++;
    while(off-- > 0) fprintf(f, "0");
    fprintf(f, "\n");
    for(size_t k=0;k<width*(uint64_t)height;k++)
    {
      float p32[3] = {pf[4*k+0], pf[4*k+1], pf[4*k+2]};
      fwrite(p32, sizeof(float), 3ul, f);
    }
    fclose(f);
  }
}
