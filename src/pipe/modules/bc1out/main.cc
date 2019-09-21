#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <squish.h>

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
extern "C" void write_sink(
    dt_module_t *module,
    void *buf)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[bc1out] writing '%s'\n", basename);

  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const uint8_t *in = (const uint8_t *)buf;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.bc1", basename);

  // go through all 4x4 blocks and call squish::CompressMasked
  // TODO: parallelise via our thread pool or openmp or what?
  uint8_t *out = (uint8_t *)malloc(sizeof(uint8_t)*8*wd*ht);
  for(int j=0;j<ht;j+=4)
  {
    for(int i=0;i<wd;i+=4)
    {
      // swizzle block data together:
      uint8_t block[64];
      for(int jj=0;jj<4;jj++)
        for(int ii=0;ii<4;ii++)
          for(int c=0;c<4;c++)
            block[4*(4*jj+ii)+c] = in[4*(wd*(j+jj)+(i+ii))+c];
      squish::CompressMasked(block, 0xffff, out + 8*(wd/4*j/4+i/4),
          squish::kDxt1 | squish::kColourRangeFit);
    }
  }

  FILE* f = fopen(filename, "wb");
  // TODO: write magic, version, width, height
  fwrite(out, sizeof(uint8_t)*8, wd*ht, f);
  fclose(f);
}
