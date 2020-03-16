#pragma once
#include "pipe/graph.h"
#include "pipe/modules/api.h"

// TODO: export convenience functions.
// TODO: see cli/main.c

// TODO: need: coarse dt_graph_export(graph, "filename")


// fine grained interface:

// replace given display node instance by export module.
// returns 0 on success.
static inline int
dt_graph_replace_display(
    dt_graph_t *graph,
    dt_token_t  inst,
    int         ldr)   // TODO: output format and params of all sorts
{
  const int mid = dt_module_get(graph, dt_token("display"), inst);
  if(mid < 0) return 1; // no display node by that name

  // get module the display is connected to:
  int cid = dt_module_get_connector(graph->module+mid, dt_token("input"));
  int m0 = graph->module[mid].connector[cid].connected_mi;
  int o0 = graph->module[mid].connector[cid].connected_mc;

  if(m0 < 0) return 2; // display input not connected

  // new module export with same inst
  // maybe new module 8-bit in between here
  dt_token_t export = ldr ? dt_token("o-jpg") : dt_token("o-pfm");
  const int m1 = dt_module_add(graph, dt_token("f2srgb"), inst);
  const int i1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
  const int o1 = dt_module_get_connector(graph->module+m1, dt_token("output"));
  const int m2 = dt_module_add(graph, export, inst);
  const int i2 = dt_module_get_connector(graph->module+m2, dt_token("input"));
  if(ldr)
  {
    // output buffer reading is inflexible about buffer configuration. texture units can handle it, so just push further:
    graph->module[m1].connector[o1].format = graph->module[m0].connector[o0].format = graph->module[m2].connector[i2].format;
    graph->module[m1].connector[o1].chan   = graph->module[m0].connector[o0].chan   = graph->module[m2].connector[i2].chan;
    CONN(dt_module_connect(graph, m0, o0, m1, i1));
    CONN(dt_module_connect(graph, m1, o1, m2, i2));
  }
  else
  {
    graph->module[m0].connector[o0].format = graph->module[m2].connector[i2].format;
    CONN(dt_module_connect(graph, m0, o0, m2, i2));
  }
  return 0;
}

// disconnect all (remaining) display modules
static inline void
dt_graph_disconnect_display_modules(
    dt_graph_t *graph)
{
  for(int m=0;m<graph->num_modules;m++)
    if(graph->module[m].name == dt_token("display"))
      dt_module_remove(graph, m); // disconnect and reset/ignore
}

static inline VkResult
dt_graph_export(
    dt_graph_t *graph,         // graph to run, will overwrite filename param
    int         output_cnt,    // number of outputs to export
    dt_token_t  output[],      // instance of output module i (o-jpg or o-pfm)
    const char *fname[])       // filename i to go with the output module i
{
  graph->frame = 0;
  if(graph->frame_cnt > 1)
  {
    dt_module_t *mod_out[20] = {0};
    assert(output_cnt <= sizeof(mod_out)/sizeof(mod_out[0]));
    char filename[256];
    for(int i=0;i<output_cnt;i++) for(int m=0;m<graph->num_modules;m++)
    {
      if(graph->module[m].inst == output[i] &&
          (graph->module[m].name == dt_token("o-jpg") ||
           graph->module[m].name == dt_token("o-pfm")))
      {
        mod_out[i] = graph->module+m;
        if(fname)
          snprintf(filename, sizeof(filename), "%s_%04d", fname[i], 0);
        else
          snprintf(filename, sizeof(filename), "%"PRItkn"_%04d", dt_token_str(output[i]), 0);
        dt_module_set_param_string(
            mod_out[i], dt_token("filename"),
            filename);
        break;
      }
    }
    dt_graph_run(graph, s_graph_run_all);
    for(int f=1;f<graph->frame_cnt;f++)
    {
      graph->frame = f;
      for(int i=0;i<output_cnt;i++)
      {
        if(fname)
          snprintf(filename, sizeof(filename), "%s_%04d", fname[i], f);
        else
          snprintf(filename, sizeof(filename), "%"PRItkn"_%04d", dt_token_str(output[i]), f);
        dt_module_set_param_string(
            mod_out[i], dt_token("filename"),
            filename);
      }
      VkResult res = dt_graph_run(graph,
          s_graph_run_record_cmd_buf | 
          s_graph_run_download_sink  |
          s_graph_run_wait_done);
      if(res != VK_SUCCESS) return res;
    }
    return VK_SUCCESS;
  }
  else
  {
    return dt_graph_run(graph, s_graph_run_all);
  }
}
