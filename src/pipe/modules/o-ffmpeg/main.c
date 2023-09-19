#include "modules/api.h"
#include "core/core.h"

#include <stdio.h>
#include <stdlib.h>
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
  buf_t *buf = mod->data;
  if(buf->f) pclose(buf->f);
  buf->f = 0; // de-init
}

void write_sink(
    dt_module_t *mod,
    void        *buf)
{
  buf_t *dat = mod->data;
  if(!dat->f)
  { // init if not done before
    const char *basename = dt_module_param_string(mod, 0);
    char filename[512];
    // snprintf(filename, sizeof(filename), "%s.h264", basename);
    snprintf(filename, sizeof(filename), "%s.mov", basename);

    const int width  = mod->connector[0].roi.wd & ~1;
    const int height = mod->connector[0].roi.ht & ~1;
    const float rate = mod->graph->frame_rate > 0.0f ? mod->graph->frame_rate : 24;
    if(width <= 0 || height <= 0) return;

    // establish pipe to ffmpeg binary
    char cmdline[1024];
#if 1 // apple prores encoding, trying 10 bit. will need some 10-bit input!
    snprintf(cmdline, sizeof(cmdline),
      "ffmpeg -y -probesize 5000000 -f rawvideo "
      "-colorspace bt2020nc -color_trc linear -color_primaries bt2020 -color_range pc "
      // "-colorspace bt709 -color_trc bt709 -color_primaries bt709 " ??? debug
      "-pix_fmt rgba64le -s %dx%d -r %g -i - "
      // "-vf 'colorspace=all=bt2020:trc=smpte2084' " 
      // "-vf 'colorspace=all=smpte240m' "
      "-vf 'colorspace=all=bt2020:trc=bt2020-10' "
      "-c:v prores -profile:v 3 "
      // TODO: use quality parameter and scale from 31--2
      // "-qscale:v 10 " // ??? is this our quality parameter? 2--31, the lower the higher the bitrate
      "-vendor apl0 -pix_fmt yuv422p10le " // output stuff: "-s %dx%d -r %g"
      // "-color_trc smpte2084 " // hopefully set correctly by the vf above
      "%s",
      width, height, rate, filename);
#else
    snprintf(cmdline, sizeof(cmdline),
        "ffmpeg "
        "-y -f rawvideo -pix_fmt rgba -s %dx%d -r %g -i - "
        "-c:v libx264 -profile:v baseline -pix_fmt yuv420p " // -level:v 3 " // -b:v 2500 "
        "-v error "
        // "-an /tmp/out_tempData.h264 "
        "%s",
        width, height, rate, filename);
#endif
    fprintf(stderr, "[o-ffmpeg] running `%s'\n", cmdline);

    if(dat->f) pclose(dat->f);
    dat->f = popen(cmdline, "w");
  }
  if(!dat->f) return;
  // uint32_t *p32 = buf; // our input buf is rgba ui8
  uint16_t *p16 = buf; // our input buf is rgba ui16

  // h264 requires width and height to be divisible by 2:
  const int width  = mod->connector[0].roi.wd & ~1;
  const int height = mod->connector[0].roi.ht & ~1;
  const int stride = 4*mod->connector[0].roi.wd;

  for(int j=0;j<height;j++)
    // fwrite(p32 + j*stride, sizeof(uint32_t), width, dat->f);
    fwrite(p16 + j*stride, sizeof(uint16_t)*4, width, dat->f);
}
