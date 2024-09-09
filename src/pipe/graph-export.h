#pragma once

// export convenience functions, see cli/main.c

// disconnect all (remaining) display modules
void
dt_graph_disconnect_display_modules(
    dt_graph_t *graph);

typedef struct dt_graph_export_output_t
{
  int max_width;           // scale ROI request to fit inside this, if > 0
  int max_height;
  dt_token_t mod;          // o-bc1, o-jpg, o-pfm, .. ?
  dt_token_t inst;         // instance name of sink to replace
  dt_token_t inst_out;     // instance name of the module to replace the sink. leave 0 for == inst.
  const char *p_filename;  // set filename param to this
  const char *p_audio;     // if set, write audio to this file
  const char *p_pdata;     // if set, overwrite params of output module
  float quality;           // set quality param to this
  int   colour_primaries;  // request a certain colour space for the output
  int   colour_trc;
}
dt_graph_export_output_t;

// parameter struct for export.
// 0 is default
// TODO: support live graph, so we can drop in without re-alloc..?
typedef struct dt_graph_export_t
{
  const char  *p_cfgfile;      // if not NULL, read this config file (or the default)
  const char  *p_defcfg;       // if not NULL and p_cfgfile is no cfg, read this (else read default-darkroom.cfg)
                               // the mechanics are that cfgfile is interpreted as a raw input and the params
                               // in defcfg will be wired to read this file
  dt_token_t   input_module;   // if other than i-raw for default configs
  int          extra_param_cnt;// number of extra parameters
  char       **p_extra_param;  // extra parameter lines

  int          output_cnt;     // how many output modules
  dt_graph_export_output_t output[20];

  int          dump_modules;   // debug output: write module graph in dot format
  int          last_frame_only;// only write the very last frame of an animation
  int          print_progress; // print progress (for long animations)
}
dt_graph_export_t;

VkResult
dt_graph_export(
    dt_graph_t *graph,         // graph to run, will overwrite filename param
    dt_graph_export_t *param);

