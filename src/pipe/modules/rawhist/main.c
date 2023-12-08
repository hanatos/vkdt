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
  module->connector[2].roi.full_ht =    5;
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[1].roi.wd = module->connector[1].roi.full_wd;
  module->connector[1].roi.ht = module->connector[1].roi.full_ht;
  module->connector[1].roi.scale = 1.0f;
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
  i[1] = MAX(1, module->img_param.black[1]);
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
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return;
  const int block = img_param->filters == 9u ? 3 : 2;

  // input -> collect -> map -> output
  dt_roi_t roi_buf = module->connector[2].roi;

  const int id_collect = dt_node_add(graph, module, "rawhist",
      graph->float_atomics_supported ? "collect" : "coldumb",
    module->connector[0].roi.wd/block, module->connector[0].roi.ht/block, 1,
    0, 0, 2,
    "input",  "read",  "rggb", "ui16", &module->connector[0].roi,
    "output", "write", "r",    "atom", &roi_buf);
  const int id_map = dt_node_add(graph, module, "rawhist", "map",
    module->connector[1].roi.wd, module->connector[1].roi.ht, 1,
    0, 0, 2,
    "input",  "read",  "r",    "atom", &roi_buf,
    "output", "write", "rgba", "f16",  &module->connector[1].roi);

  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);
  dt_connector_copy(graph, module, 1, id_map, 1);
  dt_connector_copy(graph, module, 2, id_collect, 1);
  graph->node[id_collect].connector[1].flags = s_conn_clear;
}
