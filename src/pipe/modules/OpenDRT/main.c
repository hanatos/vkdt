#include "modules/api.h"

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[1].roi = module->connector[0].roi;
  module->connector[2].roi.full_wd = 1024;
  module->connector[2].roi.full_ht =  512;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t hroi = (dt_roi_t){.wd = 16+1, .ht = 16 };
  const int id_hist = dt_node_add(graph, module, "OpenDRT", "hist",
      (wd+15)/16 * DT_LOCAL_SIZE_X, (ht+15)/16 * DT_LOCAL_SIZE_Y, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "hist",   "write", "ssbo", "u32", &hroi);
  const int id_main = dt_node_add(graph, module, "OpenDRT", "main",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  const int id_dspy = dt_node_add(graph, module, "OpenDRT", "dspy",
      module->connector[2].roi.wd, module->connector[2].roi.ht, 1, 0, 0, 2,
      "hist",   "read",  "ssbo", "u32", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[2].roi);
  dt_node_connect_named(graph, id_hist, "hist", id_dspy, "hist");
  dt_connector_copy(graph, module, 0, id_hist, 0);
  dt_connector_copy(graph, module, 0, id_main, 0);
  dt_connector_copy(graph, module, 1, id_main, 1);
  dt_connector_copy(graph, module, 2, id_dspy, 1);
}
