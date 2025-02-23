#include "modules/api.h"
#include <stdio.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  float *g = (float *)module->param;
  f[0] = g[0]; // opacity
  f[1] = g[1]; // radius
  f[2] = g[2]; // hardness

  // now pass the actual number of vertices onwards, too.
  // unfortunately need to search for our node, setting it on the module
  // will be too late
  const int pi = dt_module_get_param(module->so, dt_token("draw"));
  const uint32_t *p_draw = dt_module_param_uint32(module, pi);
  const int num_verts = p_draw[0];
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("draw") &&
       graph->node[n].kernel == dt_token("source") &&
       graph->node[n].module->inst == module->inst)
    {
      dt_connector_t *c = graph->node[n].connector;
      c->roi.full_wd = 2+num_verts;
      break;
    }
  }
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*3;
  return 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pi = dt_module_get_param(module->so, dt_token("draw"));
  // set flag on this module that we want to run read_source again!
  if(parid == pi)
    module->flags |= s_module_request_read_source;
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

// TODO: check_params to support incremental updates (store on struct here and
// memcpy only what's needed in read_source)

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int pi = dt_module_get_param(module->so, dt_token("draw"));
  // const uint32_t *p_draw = dt_module_param_uint32(module, pi);
  // const int num_verts = p_draw[0];
  const int num_verts = module->so->param[pi]->cnt;
  module->connector[0].roi = (dt_roi_t){ .full_wd = 1024, .full_ht = 1024 };
  module->connector[1].roi = (dt_roi_t){ .full_wd = 2+num_verts, .full_ht = 2 };
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int pi = dt_module_get_param(module->so, dt_token("draw"));
  // const uint32_t *p_draw = dt_module_param_uint32(module, pi);
  // const int num_verts = p_draw[0];
  const int num_verts = module->so->param[pi]->cnt;
  module->connector[1].roi.wd = 2+num_verts;
  module->connector[1].roi.ht = 2;
  module->connector[1].roi.scale = 1;
}

int read_source(
    dt_module_t *mod,
    void *mapped)
{
  const int pi = dt_module_get_param(mod->so, dt_token("draw"));
  const uint32_t *p_draw = dt_module_param_uint32(mod, pi);
  const int num_verts = p_draw[0];
  memcpy(mapped, p_draw+1, sizeof(uint32_t)*2*num_verts);
  mod->flags = 0; // yay, we uploaded.
  return 0;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // we will create two nodes: one graphics and one input.
  // the input node creates an ssbo source connector
  // which is manually fed the params (TODO: update only necessary parts) in read_source above.
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const int dp = 1;

  // in fact we'd only need as much memory as we have vertices here.
  // unfortunately doing that requires re-running buffer allocations
  // and the full thing, which is potentially slow. and what's a couple
  // megabytes among friends, right?
  const int pi = dt_module_get_param(module->so, dt_token("draw"));
  // const float *p_draw = dt_module_param_float(module, pi);
  // const int num_verts = p_draw[0];
  const int num_verts = module->so->param[pi]->cnt;

  dt_roi_t roi_ssbo = {
    .full_wd = 2+num_verts,  // safety margin for zero verts
    .wd      = 2+num_verts,
    .full_ht = 2,
    .ht      = 2,
    .scale   = 1.0,
  };

  float aspect = wd/(float)ht;
  uint32_t aspecti = dt_touint(aspect);

  const int id_source = dt_node_add(graph, module, "draw", "source", 1, 1, 1, 0, 0, 1,
      "source", "source", "ssbo", "ui32", &roi_ssbo);
  int pc[] = { aspecti, wd };
  const int id_draw = dt_node_add(graph, module, "draw", "main", wd, ht, dp, sizeof(pc), pc, 2,
      "input", "read", "ssbo", "ui32", dt_no_roi,
      "output", "write", "y", "f16", &module->connector[0].roi);
  graph->node[id_draw].type = s_node_graphics; // mark for rasterisation via vert/geo/frag shaders
  CONN(dt_node_connect(graph, id_source, 0, id_draw, 0));
  dt_connector_copy(graph, module, 0, id_draw, 1);
  dt_connector_copy(graph, module, 1, id_source, 0); // route out the raw stroke data
}
