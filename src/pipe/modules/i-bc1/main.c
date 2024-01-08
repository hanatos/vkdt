#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
	fprintf(stderr, "XXX bc1 input!\n");
  // load only header
  const char *filename = dt_module_param_string(mod, 0);

  uint32_t header[4] = {0};
  gzFile f = gzopen(filename, "rb");
  if(!f || gzread(f, header, sizeof(uint32_t)*4) != sizeof(uint32_t)*4)
  {
    fprintf(stderr, "[i-bc1] %s: can't open file!\n", filename);
    if(f) gzclose(f);
  }
  // checks: magic != dt_token("bc1z") || version != 1
  if(header[0] != dt_token("bc1z") || header[1] != 1)
  {
    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
    gzclose(f);
  }
  const uint32_t wd = 4*(header[2]/4), ht = 4*(header[3]/4);
  mod->connector[0].roi.full_wd = wd;
  mod->connector[0].roi.full_ht = ht;
	fprintf(stderr, "XXX bc1 input! %d %d\n", wd, ht);
  gzclose(f);
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const char *filename = dt_module_param_string(mod, 0);

  uint32_t header[4] = {0};
  gzFile f = gzopen(filename, "rb");
  if(!f || gzread(f, header, sizeof(uint32_t)*4) != sizeof(uint32_t)*4)
  {
    fprintf(stderr, "[i-bc1] %s: can't open file!\n", filename);
    if(f) gzclose(f);
    return 1;
  }
  // checks: magic != dt_token("bc1z") || version != 1
  if(header[0] != dt_token("bc1z") || header[1] != 1)
  {
    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
    gzclose(f);
    return 1;
  }
  const uint32_t wd = 4*(header[2]/4), ht = 4*(header[3]/4);
  // fprintf(stderr, "[i-bc1] %s magic %"PRItkn" version %u dim %u x %u\n",
  //     filename, dt_token_str(header[0]),
  //     header[1], header[2], header[3]);
  gzread(f, mapped, sizeof(uint8_t)*8*(wd/4)*(ht/4));
  gzclose(f);
  return 0;
}
