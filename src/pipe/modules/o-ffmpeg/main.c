#include "modules/api.h"
#include "core/core.h"
#include "../o-pfm/half.h"

#include <stdio.h>
#include <string.h>

typedef struct buf_t
{
  FILE *f;
}
buf_t;

int init(dt_module_t *mod)
{
  buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  mod->flags = s_module_request_write_sink;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  buf_t *dat= mod->data;
  if(dat->f)
  {
    pclose(dat->f);
    dat->f = 0;
  }
  free(dat);
  mod->data = 0;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  if(graph->frame_cnt <= 1) return;
  const char *basename = dt_module_param_string(mod, 0);
  char filename[512];
  snprintf(filename, sizeof(filename), "%s.mp4", basename);

  const int width  = mod->connector[0].roi.full_wd & ~1;
  const int height = mod->connector[0].roi.full_ht & ~1;
  const float rate = mod->graph->frame_rate;
  if(width <= 0 || height <= 0) return;

  // establish pipe to ffmpeg binary
  char cmdline[1024];
  snprintf(cmdline, sizeof(cmdline),
    "ffmpeg "
    "-y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %g -i - "
    "-c:v libx264 -profile:v baseline -pix_fmt yuv420p " // -level:v 3 " // -b:v 2500 "
    // "-an /tmp/out_tempData.h264 "
    "%s",
    width, height, rate, filename);
  fprintf(stderr, "running `%s'\n", cmdline);

  buf_t *buf = mod->data;
  if(buf->f) pclose(buf->f);
  buf->f = popen(cmdline, "w");
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void        *buf)
{
  buf_t *dat = module->data;
  if(!dat->f) return;
  if(feof(dat->f))
  {
    dat->f = 0;
    return;
  }
  const char *basename = dt_module_param_string(module, 0);
  // fprintf(stderr, "[o-ffmpeg] writing '%s' frame %d\n", basename, module->graph->frame);
  uint16_t *p16 = buf;

  const int width  = module->connector[0].roi.wd & ~1;
  const int height = module->connector[0].roi.ht & ~1;
  const int stride = module->connector[0].roi.wd;

  // could probably ask for ui8 input so gpu will convert for us
  for(int j=0;j<height;j++) for(int i=0;i<width;i++)
  {
    uint64_t k = stride * j + i;
    float p32[3] = {
      half_to_float(p16[4*k+0]),
      half_to_float(p16[4*k+1]),
      half_to_float(p16[4*k+2])};
    uint8_t rgb[3] = {
      CLAMP(p32[0] * 256.0, 0, 255),
      CLAMP(p32[1] * 256.0, 0, 255),
      CLAMP(p32[2] * 256.0, 0, 255)};
    fwrite(rgb, sizeof(uint8_t), 3, dat->f);
  }
}
