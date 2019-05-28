#include "module.h"

#include <stdio.h>
#include <string.h>

// TODO: params struct


void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // TODO: scale to our needs
  dt_roi_t *r = &module->connector[0].roi;
  r->roi_wd = r->full_wd;
  r->roi_ht = r->full_ht;
  r->roi_ox = 0;
  r->roi_oy = 0;
  r->roi_scale = 1.0f;
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  fprintf(stderr, "[export] writing 'test.pfm'\n");
  float *pixel = buf;

  const int width  = module->connector[0].roi.roi_wd;
  const int height = module->connector[0].roi.roi_ht;

  // TODO: get from ui params
  const char *basename = "test"; // XXX

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
    fwrite(pixel, sizeof(float), 3ul*width*height, f);
    fclose(f);
  }
}
