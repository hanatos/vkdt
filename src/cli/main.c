#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-print.h"
#include "pipe/graph-export.h"
#include "pipe/global.h"
#include "pipe/modules/api.h"
#include "core/log.h"
#include "core/version.h"

#include <stdlib.h>


static inline int
parse_prim(const char *str)
{
  if(!strcasecmp(str, "sRGB"))     return s_colour_primaries_srgb;
  if(!strcasecmp(str, "bt2020"))   return s_colour_primaries_2020;
  if(!strcasecmp(str, "adobergb")) return s_colour_primaries_adobe;
  if(!strcasecmp(str, "P3"))       return s_colour_primaries_P3;
  if(!strcasecmp(str, "XYZ"))      return s_colour_primaries_XYZ;
  return s_colour_primaries_unknown;
}

static inline int
parse_trc(const char *str)
{
  if(!strcasecmp(str, "linear"))   return s_colour_trc_linear;
  if(!strcasecmp(str, "709"))      return s_colour_trc_709;
  if(!strcasecmp(str, "sRGB"))     return s_colour_trc_srgb;
  if(!strcasecmp(str, "PQ"))       return s_colour_trc_PQ;
  if(!strcasecmp(str, "DCI"))      return s_colour_trc_DCI;
  if(!strcasecmp(str, "gamma2.2")) return s_colour_trc_gamma;
  return s_colour_trc_unknown;
}

int main(int argc, char *argv[])
{
  for(int i=0;i<argc;i++) if(!strcmp(argv[i], "--version"))
  {
    printf("vkdt "VKDT_VERSION" (c) 2020--2026 johannes hanika\n");
    exit(0);
  }
  // init global things, log and pipeline:
  dt_log_init(s_log_cli);
  dt_log_init_arg(argc, argv);
  // this prints the version for -d all but remains silent in the default operation
  dt_log(s_log_gui, "vkdt "VKDT_VERSION" (c) 2020--2026 johannes hanika");
  dt_pipe_global_init();
  threads_global_init();

  int dump_nodes = 0;
  int output_cnt = 0;
  int config_start = 0; // start of arguments which are interpreted as additional config lines
  dt_graph_export_t param = {0};
  // default to sRGB if nothing given on cmdline
  // maybe sane defaults per output module would be interesting
  param.output[0].colour_primaries = parse_prim("sRGB");
  param.output[0].colour_trc = parse_prim("sRGB");
  const char *gpu_name = 0;
  int gpu_id = -1;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      param.p_cfgfile = argv[++i];
    else if(!strcmp(argv[i], "--quality") && i < argc-1)
      param.output[output_cnt].quality = atof(argv[++i]);
    else if(!strcmp(argv[i], "--width") && i < argc-1)
      param.output[output_cnt].max_width = atof(argv[++i]);
    else if(!strcmp(argv[i], "--height") && i < argc-1)
      param.output[output_cnt].max_height = atof(argv[++i]);
    else if(!strcmp(argv[i], "--filename") && i < argc-1)
      param.output[output_cnt].p_filename = argv[++i];
    else if(!strcmp(argv[i], "--format") && i < argc-1)
      {i++; param.output[output_cnt].mod = dt_token(argv[i]);}
    else if(!strcmp(argv[i], "--audio") && i < argc-1)
      param.output[output_cnt].p_audio = argv[++i];
    else if(!strcmp(argv[i], "--colour-prim") && i < argc-1)
      param.output[output_cnt].colour_primaries = parse_prim(argv[++i]);
    else if(!strcmp(argv[i], "--colour-trc") && i < argc-1)
      param.output[output_cnt].colour_trc = parse_trc(argv[++i]);
    else if(!strcmp(argv[i], "--last-frame-only"))
      param.last_frame_only = 1;
    else if(!strcmp(argv[i], "--dump-modules"))
      param.dump_modules = 1;
    else if(!strcmp(argv[i], "--dump-nodes"))
      dump_nodes = 1;
    else if(!strcmp(argv[i], "--progress"))
      param.print_progress = 1;
    else if(!strcmp(argv[i], "--output") && i < argc-1 && ++i)
      param.output[output_cnt++].inst = dt_token(argv[i]);
    else if(!strcmp(argv[i], "--device") && i < argc-1)
      gpu_name = argv[++i];
    else if(!strcmp(argv[i], "--device-id") && i < argc-1)
      gpu_id = atol(argv[++i]);
    else if(!strcmp(argv[i], "--config"))
    { config_start = i+1; break; }
  }
  param.output_cnt = MAX(1, output_cnt);

  if(qvk_init(gpu_name, gpu_id, 0, 0, 0)) exit(1);

  if(!param.p_cfgfile)
  {
    fprintf(stderr, "usage: vkdt-cli -g <graph.cfg>\n"
    "    [-d verbosity]                set log verbosity (none,qvk,pipe,gui,db,cli,snd,perf,mem,err,all)\n"
    "    [--last-frame-only]           only write the last frame, not the intermediates\n"
    "    [--progress]                  print some progress information (useful for long animations)\n"
    "    [--dump-modules|--dump-nodes] write graphvis dot files to stdout\n"
    "    [--quality <0-100>]           (jpg) output quality\n"
    "    [--width <x>]                 max output width\n"
    "    [--height <y>]                max output height\n"
    "    [--filename <f>]              output filename (without extension or frame number)\n"
    "    [--format <fm>]               output format (o-jpg, o-bc1, o-pfm, ..)\n"
    "    [--audio <file>]              dump output audio stream to this file, if any\n"
    "    [--colour-prim <prim-id>]     colour primaries to use for encoding, one of:\n"
    "                                  sRGB, bt2020, AdobeRGB, P3, XYZ\n"
    "    [--colour-trc <trc-id>]       colour tone response curve for encoding, one of:\n"
    "                                  linear, bt709, sRGB, PQ, DCI, HLG, gamma2.2\n"
    "    [--output <inst>]             name the instance of the output to write (can use multiple)\n"
    "                                  this harvests and then resets output specific options: quality, width, height,\n"
    "                                  audio, colour-prim, colour-trc, filename\n"
    "    [--device <gpu name>]         explicitly use this gpu if you have multiple\n"
    "    [--device-id <gpu id>]        explicitly use this gpu id if you have multiple\n"
    "    [--config]                    everything after this will be interpreted as additional cfg lines\n"
        );
    threads_global_cleanup();
    qvk_cleanup();
    exit(1);
  }

  dt_graph_t graph;
  dt_graph_init(&graph, s_queue_compute);
  snprintf(graph.searchpath, sizeof(graph.searchpath), ".");

  param.extra_param_cnt = config_start ? argc - config_start : 0;
  param.p_extra_param   = argv + config_start;

  VkResult res = dt_graph_export(&graph, &param);

  if(param.output[0].p_audio)
  {
    dt_log(s_log_cli, "wrote audio channel to %s. to combine the streams, use something like", param.output[0].p_audio);
    dt_log(s_log_cli, "ffmpeg -i %s.h264 -f s16le -sample_rate %d -channels %d  -i %s -c:v copy combined.mp4",
        param.output[0].p_filename,
        graph.main_img_param.snd_samplerate,
        graph.main_img_param.snd_channels,
        param.output[0].p_audio);
  }

  // nodes we can only print after run() has been called:
  if(dump_nodes)
    dt_graph_print_nodes(&graph);

  dt_graph_cleanup(&graph);
  threads_global_cleanup();
  qvk_cleanup();
  exit(res);
}
