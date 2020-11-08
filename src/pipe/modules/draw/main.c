#include "modules/api.h"
#include <stdio.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  float *g = (float *)module->param;
  f[0] = g[0]; // opacity
  f[1] = g[1]; // radius
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*2;
  return 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  const int pi = dt_module_get_param(module->so, dt_token("draw"));
  // set flag on this module that we want to run read_source again!
  if(parid == pi)
    module->flags |= s_module_request_read_source;
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

// TODO: insert 0 0 vertices to end line strip (fix into geo shader)

// TODO: check_params to support incremental updates (store on struct here and
// memcpy only what's needed in read_source)

int read_source(
    dt_module_t *mod,
    void *mapped)
{
  const int pi = dt_module_get_param(mod->so, dt_token("draw"));
  const float *p_draw = dt_module_param_float(mod, pi);
  const int num_verts = p_draw[0];
  memcpy(mapped, p_draw+1, sizeof(float)*2*num_verts);
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
  // const int pi = dt_module_get_param(module->so, dt_token("draw"));
  // const float *p_draw = dt_module_param_float(module, pi);
  // const int num_verts = p_draw[0];
  const int num_verts = 2000;

  dt_roi_t roi_ssbo = {
    .full_wd = 2+num_verts,  // safety margin for zero verts
    .wd      = 2+num_verts,
    .full_ht = 2,
    .ht      = 2,
    .scale   = 1.0,
  };

  assert(graph->num_nodes < graph->max_nodes);
  const int id_source = graph->num_nodes++;
  graph->node[id_source] = (dt_node_t) {
    .name   = dt_token("draw"),
    .kernel = dt_token("source"),
    .module = module,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("source"),
      .type   = dt_token("source"),
      .chan   = dt_token("ssbo"),
      .format = dt_token("f32"),
      .roi    = roi_ssbo,
    }},
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_draw = graph->num_nodes++;
  graph->node[id_draw] = (dt_node_t) {
    .name   = dt_token("draw"),
    .kernel = dt_token("main"),
    .type   = s_node_graphics,
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("ssbo"),
      .format = dt_token("f32"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };
  CONN(dt_node_connect(graph, id_source, 0, id_draw, 0));
  dt_connector_copy(graph, module, 0, id_draw, 1);
}
