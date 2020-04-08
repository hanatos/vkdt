#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // always request constant histogram size:
  module->connector[1].roi = module->connector[0].roi;
  // smaller sizes are slower (at least on intel), probably more atomic contention:
  module->connector[1].roi.full_wd = 1000;
  module->connector[1].roi.full_ht =  600;
  module->connector[2].roi.full_wd = 1000;
  module->connector[2].roi.full_ht =    4;
}

// XXX why do we need this and the other histogram doesn't??
void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.scale = 1.0f;
  module->connector[2].roi.wd = module->connector[2].roi.full_wd;
  module->connector[2].roi.ht = module->connector[2].roi.full_ht;
  module->connector[2].roi.scale = 1.0f;
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  f[0] = module->img_param.noise_a;
  f[1] = module->img_param.noise_b;
  uint32_t *i = (uint32_t *)(f+2);
  i[0] = module->img_param.filters;
  i[1] = module->img_param.black[1];
  i[2] = module->img_param.white[1];
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = 2*sizeof(float) + 3*sizeof(uint32_t);
  return 0;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{

  const int block = 3; // TODO distinguish by bayer/xtrans!

  // input -> collect -> map -> output

  dt_roi_t roi_buf = module->connector[2].roi;

  assert(graph->num_nodes < graph->max_nodes);
  const int id_collect = graph->num_nodes++;
  graph->node[id_collect] = (dt_node_t) {
    .name   = dt_token("rawhist"),
    .kernel = dt_token("collect"),
    .module = module,
    .wd     = module->connector[0].roi.wd/block,
    .ht     = module->connector[0].roi.ht/block,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgga"),
      .format = dt_token("ui16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("r"),
      .format = dt_token("ui32"),
      .roi    = roi_buf,
      .flags  = s_conn_clear,
    }},
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_map = graph->num_nodes++;
  graph->node[id_map] = (dt_node_t) {
    .name   = dt_token("rawhist"),
    .kernel = dt_token("map"),
    .module = module,
    .wd     = module->connector[1].roi.wd,
    .ht     = module->connector[1].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("r"),
      .format = dt_token("ui32"),
      .roi    = roi_buf,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[1].roi,
    }},
  };

  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);
  dt_connector_copy(graph, module, 1, id_map, 1);
  dt_connector_copy(graph, module, 2, id_collect, 1);
  graph->node[id_collect].connector[1].flags  = s_conn_clear;
}

