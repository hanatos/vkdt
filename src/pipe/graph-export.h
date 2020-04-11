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

VkResult
dt_graph_export(
    dt_graph_t *graph,         // graph to run, will overwrite filename param
    int         output_cnt,    // number of outputs to export
    dt_token_t  output[],      // instance of output module i (o-jpg or o-pfm)
    const char *fname[],       // filename i to go with the output module i
    float       quality);      // overwrite output quality

// quick interface:
VkResult dt_graph_export_quick(
    const char *graphcfg,      // cfg filename of graph to export
    const char *filename);     // output filename "main" display will be rendered to

