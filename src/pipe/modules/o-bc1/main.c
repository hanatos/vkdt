#include "modules/api.h"
#include "core/core.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>


void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  dt_roi_t *r = &mod->connector[0].roi;
  r->scale = 1.0f;
  // scale to fit into requested roi
  if(graph->output_wd > 0 || graph->output_ht > 0)
    r->scale = MAX(
        r->full_wd / (float) graph->output_wd,
        r->full_ht / (float) graph->output_ht);
  r->wd = r->full_wd/r->scale;
  r->ht = r->full_ht/r->scale;
  r->wd = (r->wd/4)*4; // make sure we have bc1 blocks aligned
  r->ht = (r->ht/4)*4;
  r->x = r->y = 0;
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  const char *filename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-bc1] writing '%s'\n", filename);

  const uint32_t wd = module->connector[0].roi.wd;
  const uint32_t ht = module->connector[0].roi.ht;
  const uint8_t *in = (const uint8_t *)buf;

  // go through all 4x4 blocks
  // TODO: parallelise via our thread pool or openmp or what?
  const int bx = wd/4, by = ht/4;
  size_t num_blocks = bx * by;
  uint8_t *out = (uint8_t *)malloc(sizeof(uint8_t)*8*num_blocks);
// #pragma omp parallel for collapse(2) schedule(static)
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

      stb_compress_dxt_block(
          out + 8*(bx*(j/4)+(i/4)), block, 0,
          0); // or slower: STB_DXT_HIGHQUAL
    }
  }

  char tmpfile[1024];
  snprintf(tmpfile, sizeof(tmpfile), "%s.temp", filename);
  gzFile f = gzopen(tmpfile, "wb");
  // write magic, version, width, height
  uint32_t header[4] = { dt_token("bc1z"), 1, wd, ht };
  gzwrite(f, header, sizeof(uint32_t)*4);
  gzwrite(f, out, sizeof(uint8_t)*8*num_blocks);
  gzclose(f);
  free(out);
  // atomically create filename only when we're quite done writing:
  unlink(filename); // just to be sure the link will work
  link(tmpfile, filename);
  unlink(tmpfile);
}
