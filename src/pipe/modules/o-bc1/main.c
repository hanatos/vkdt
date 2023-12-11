#include "modules/api.h"
#include "core/core.h"
#include "core/fs.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  const char *filename = dt_module_param_string(module, 0);
  // fprintf(stderr, "[o-bc1] writing '%s'\n", filename);

  const uint32_t wd = module->connector[0].roi.wd;
  const uint32_t ht = module->connector[0].roi.ht;
  const uint8_t *in = (const uint8_t *)buf;

  // go through all 4x4 blocks
  // parallelise via our thread pool or openmp or what?
  // probably usually bc1 thumbnails are too small to warrant a good speedup.
  const int bx = wd/4, by = ht/4;
  size_t num_blocks = bx * (uint64_t)by;
  uint8_t *out = (uint8_t *)malloc(sizeof(uint8_t)*8*num_blocks);
// #pragma omp parallel for collapse(2) schedule(static)
  for(int j=0;j<4*by;j+=4)
  {
    for(int i=0;i<4*bx;i+=4)
    { // swizzle block data together:
      uint8_t block[64];
      for(int jj=0;jj<4;jj++)
        for(int ii=0;ii<4;ii++)
          for(int c=0;c<4;c++)
            block[4*(4*jj+ii)+c] = in[4*(wd*(j+jj)+(i+ii))+c];

      stb_compress_dxt_block(
          out + 8*(bx*(j/4)+(i/4)), block, 0,
          0); // or slower: STB_DXT_HIGHQUAL
    }
  }

  char tmpfile[1024];
  snprintf(tmpfile, sizeof(tmpfile), "%s.temp", filename);
  gzFile f = gzopen(tmpfile, "wb");
  // write magic, version, width, height
  uint32_t header[4] = { dt_token("bc1z"), 1, bx*4, by*4 };
  gzwrite(f, header, sizeof(uint32_t)*4);
  gzwrite(f, out, sizeof(uint8_t)*8*num_blocks);
  gzclose(f);
  free(out);
  // atomically create filename only when we're quite done writing:
  unlink(filename); // just to be sure the link will work
  fs_link(tmpfile, filename);
  unlink(tmpfile);
}
