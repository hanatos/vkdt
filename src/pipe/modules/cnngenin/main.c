#include "modules/api.h"

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.scale = 1.0f;
  module->connector[1].roi.wd = module->connector[0].roi.full_wd;
  module->connector[1].roi.ht = module->connector[0].roi.full_ht;
  module->connector[1].roi.scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int tiles = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("tiles")))[0];
  if(tiles)
  {
    const int wd = 64;//256;
    module->connector[2].roi.full_wd = wd;
    module->connector[2].roi.full_ht = wd;
    module->connector[3].roi.full_wd = wd;
    module->connector[3].roi.full_ht = wd;
  }
  else
  {
    module->connector[2].roi = module->connector[0].roi;
    module->connector[3].roi = module->connector[1].roi;
  }
  module->connector[4].roi.full_wd = 2;
  module->connector[4].roi.full_ht = 2;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 1) // tiles
    return s_graph_run_all; // need no do modify_roi_out again to read noise model from file
  return s_graph_run_record_cmd_buf;
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int id = dt_node_add(
      graph, module, "cnngenin", "main", 
      module->connector[2].roi.wd, module->connector[2].roi.ht, 1,
      0, 0, 5,
      "imgi", "read",  "*",    "*",   &module->connector[0].roi,
      "refi", "read",  "*",    "*",   &module->connector[1].roi,
      "imgo", "write", "rgba", "f16", &module->connector[2].roi,
      "refo", "write", "rgba", "f16", &module->connector[3].roi,
      "nab",  "write", "ssbo", "f32", &module->connector[4].roi);
  for(int i=0;i<5;i++) dt_connector_copy(graph, module, i, id, i);
}
