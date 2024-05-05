#include "modules/api.h"
#include "core/core.h"
#include "core/fs.h"

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
    dt_module_t            *mod,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  buf_t *dat = mod->data;
  if(!dat->f)
  { // init if not done before
    const char *basename  = dt_module_param_string(mod, 0);
    const float p_quality = dt_module_param_float(mod, 1)[0];
    const int   p_codec   = dt_module_param_int  (mod, 2)[0];
    const int   p_profile = dt_module_param_int  (mod, 3)[0];
    const int   p_colour  = dt_module_param_int  (mod, 4)[0];

    const int width  = mod->connector[0].roi.wd & ~1;
    const int height = mod->connector[0].roi.ht & ~1;
    const float rate = mod->graph->frame_rate > 0.0f ? mod->graph->frame_rate : 24;
    if(width <= 0 || height <= 0) return;

    // establish pipe to ffmpeg binary
    char search[1024] = {0};
    char cmdline[1024], filename[512];
    // look for ffmpeg in our vkdt base directory
    if(snprintf(cmdline, sizeof(cmdline), "%s/ffmpeg", dt_pipe.basedir) < sizeof(cmdline) &&
       fs_isreg_file(cmdline) &&
       snprintf(search, sizeof(search), "%s/", dt_pipe.basedir) < sizeof(search));
    else if(snprintf(cmdline, sizeof(cmdline), "%s/ffmpeg.exe", dt_pipe.basedir) < sizeof(cmdline) &&
        fs_isreg_file(cmdline) &&
        snprintf(search, sizeof(search), "%s/", dt_pipe.basedir) < sizeof(search));
      // if we can't find it, assume it'll be on PATH:
    else search[0] = 0;
#if 1
    if(p_codec == 0)
    { // apple prores encoding, 10 bit output
      snprintf(filename, sizeof(filename), "%s.mov", basename);
      snprintf(cmdline, sizeof(cmdline),
        "%sffmpeg -y -probesize 5000000 -f rawvideo "
        "-threads 0 "
        "-colorspace bt2020nc -color_trc linear -color_primaries bt2020 -color_range pc "
        "-pix_fmt rgbaf16le -s %dx%d -r %g -i - "
//        "-vf 'colorspace=all=bt2020:trc=bt2020-10:iall=bt2020:itrc=linear' "
//        "-vf 'zscale=rangein=full:range=limited:primaries=2020:matrix=2020_ncl:primariesin=2020:transferin=linear:transfer=smpte2084' "
        "-vf zscale=rangein=full:range=limited:primaries=2020:matrix=2020_ncl:primariesin=2020:transferin=linear:transfer=arib-std-b67 "
        "-threads 0 " // after input, so it'll affect the ouput encoding
        "-color_trc smpte2084 "
//        "-color_trc arib-std-b67 "
        "-c:v prores -profile:v %d " // 0 1 2 3 for Proxy LT SQ HQ // prores_ks has slice level threading
        "-qscale:v %d " // is this our quality parameter: 2--31, lower qs -> higher bitrate
        "-vendor apl0 -pix_fmt %s " // yuv422p10le or yuva444p10le for 4444
        "\"%s\"",
        search,
        width, height, rate,
        CLAMP(p_profile, 0, 3),
        (int)CLAMP(31-p_quality*30/100.0, 1, 31),
        p_colour == 0 ? "yuv422p10le" : "yuva444p10le",
        filename);
    }
#else
    if(p_codec == 0)
    {
      // https://stackoverflow.com/questions/69251960/how-can-i-encode-rgb-images-into-hdr10-videos-in-ffmpeg-command-line
      snprintf(filename, sizeof(filename), "%s.mov", basename);
      snprintf(cmdline, sizeof(cmdline),
        "ffmpeg -threads 0 -y -probesize 5000000 -f rawvideo "
        "-hide_banner "
        "-loglevel verbose "
        "-sws_flags print_info+accurate_rnd+bitexact+full_chroma_int "
        "-color_range pc "
        "-color_trc linear "
        "-color_primaries bt2020 "
        "-colorspace bt2020nc "
        "-pix_fmt rgba64le -s %dx%d -r %g -i - "
        // "-vf zscale=rangein=full:range=limited " // XXX this needs explicit conversion to yuv422p10le i think
        "-vf format=yuv422p10le,zscale=rangein=full:range=limited:transferin=linear:transfer=smpte2084 " // XXX this needs explicit conversion to yuv422p10le i think
        "-c:v libx265 "
        "-color_range tv "
        "-color_trc smpte2084 "
        "-color_primaries bt2020 "
        "-colorspace bt2020nc "
        "-pix_fmt %s "
        "-sws_flags print_info+accurate_rnd+bitexact+full_chroma_int "
        "-x265-params \"hdr-opt=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:range=limited:master-display=G(13250,34500)B(7500,3000)R(34000,16000)WP(15635,16450)L(10000000,1):max-cll=1000,400\" "
        "\"%s\"",
        width, height, rate,
        // CLAMP(p_profile, 0, 3),
        // (int)CLAMP(31-p_quality*30/100.0, 1, 31),
        p_colour == 0 ? "yuv422p10le" : "yuva444p10le",
        filename);
    }
#endif
    // TODO: try something like this too
    // -c:v libx265 -x265-params hdr-opt=1:repeat-headers=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:master
    else
    { // h264, 8-bit
      snprintf(filename, sizeof(filename), "%s.mp4", basename);
      snprintf(cmdline, sizeof(cmdline),
          "%sffmpeg -threads 0 -y -f rawvideo "
          "-colorspace bt2020nc -color_trc linear -color_primaries bt2020 -color_range pc "
          "-pix_fmt rgbaf16le -s %dx%d -r %g -i - "
          "-vf colorspace=all=bt709:trc=bt709:iall=bt2020:itrc=linear "
          "-c:v libx264 -preset ultrafast "
//          "-c:v h264_nvenc -preset llhp " // -preset llhp/fast
//          "-c:v rawvideo -pix_fmt rgb24 -f mpegts "
          "-pix_fmt yuv420p " // -level:v 3 " // -b:v 2500 " -profile:v baseline
//          "-qp 0 "
          "-crf %d "
          // "-v error "
          "\"%s\"",
          search, width, height, rate, (int)CLAMP(51-p_quality*51.0/100.0, 0, 51), filename);
    }
    fprintf(stderr, "[o-ffmpeg] running `%s'\n", cmdline);

    if(dat->f) pclose(dat->f);
    dat->f = popen(cmdline, "w");
  }
  if(!dat->f) return;
  // uint32_t *p32 = buf; // our input buf is rgba ui8
  uint16_t *p16 = buf; // our input buf is rgba f16

  // h264 requires width and height to be divisible by 2:
  const int width  = mod->connector[0].roi.wd & ~1;
  const int height = mod->connector[0].roi.ht & ~1;
  const int stride = 4*mod->connector[0].roi.wd;

  for(int j=0;j<height;j++)
    // fwrite(p32 + j*stride, sizeof(uint32_t), width, dat->f);
    fwrite(p16 + j*stride, sizeof(uint16_t)*4, width, dat->f);
}
