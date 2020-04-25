#pragma once

// export convenience functions, see cli/main.c

// fine grained interface:

// replace given display node instance by export module.
// returns 0 on success.
int
dt_graph_replace_display(
    dt_graph_t *graph,
    dt_token_t  inst,
    int         ldr);

// disconnect all (remaining) display modules
void
dt_graph_disconnect_display_modules(
    dt_graph_t *graph);

// TODO: 0 is default
// TODO: also use this for thumbnails
// TODO: support live graph, so we can drop in without re-alloc..?
typedef struct dt_graph_export_t
{
  int max_width;              // maximum output dimensions
  int max_height;
  dt_token_t  *p_output_fmt;   // o-bc1, o-jpg, o-pfm, .. ?
  int          output_cnt;     // how many output modules
  dt_token_t  *p_output;       // instance names of sink modules to replace
  const char **p_filename;     // filenames, to go with output instances (can be NULL)
  float        quality;        // output quality
}
dt_graph_export_t;

VkResult
dt_graph_export(
    dt_graph_t *graph,         // graph to run, will overwrite filename param
    dt_graph_export_t *param);

// quick interface:
VkResult dt_graph_export_quick(
    const char *graphcfg,      // cfg filename of graph to export
    const char *filename);     // output filename "main" display will be rendered to

