#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <squish.h>
#include <zlib.h>

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
extern "C" void write_sink(
    dt_module_t *module,
    void *buf)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[bc1out] writing '%s'\n", basename);

  const uint32_t wd = module->connector[0].roi.wd;
  const uint32_t ht = module->connector[0].roi.ht;
  const uint8_t *in = (const uint8_t *)buf;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.bc1", basename);

  // go through all 4x4 blocks and call squish::CompressMasked
  // TODO: parallelise via our thread pool or openmp or what?
  const int bx = wd/4, by = ht/4;
  size_t num_blocks = bx * by;
  uint8_t *out = (uint8_t *)malloc(sizeof(uint8_t)*8*num_blocks);
  for(int j=0;j<4*by;j+=4)
  {
    for(int i=0;i<4*bx;i+=4)
    {
      // swizzle block data together:
      uint8_t block[64];
      for(int jj=0;jj<4;jj++)
        for(int ii=0;ii<4;ii++)
          for(int c=0;c<4;c++)
            block[4*(4*jj+ii)+c] = in[4*(wd*(j+jj)+(i+ii))+c];
      squish::CompressMasked(block, 0xffff, out + 8*(bx*j/4+i/4),
          squish::kDxt1 | squish::kColourRangeFit);
    }
  }

  gzFile f = gzopen(filename, "wb");
  // write magic, version, width, height
  uint32_t header[4] = { dt_token("bc1z"), 1, wd, ht };
  gzwrite(f, header, sizeof(uint32_t)*4);
  gzwrite(f, out, sizeof(uint8_t)*8*num_blocks);
  gzclose(f);
}
