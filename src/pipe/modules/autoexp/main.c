#include "modules/api.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t hroi = (dt_roi_t){.wd = DT_LOCAL_SIZE_X, .ht = DT_LOCAL_SIZE_Y };
  dt_roi_t lroi = (dt_roi_t){.wd = DT_LOCAL_SIZE_X, .ht = DT_LOCAL_SIZE_Y };
  const int id_hist = dt_node_add(graph, module, "autoexp", "hist", wd, ht, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   -1u,
      "hist",   "write", "ssbo", "u32", &hroi);

  const int id_lum = dt_node_add(graph, module, "autoexp", "lum", 1, 1, 1, 0, 0, 3,
      "input",  "read",  "*",    "*",   -1u,
      "hist",   "write", "ssbo", "u32", -1u,
      "lum",    "write", "ssbo", "f32", &lroi);

  const int id_exp = dt_node_add(graph, module, "autoexp", "exp", wd, ht, 1, 0, 0, 3,
      "input",  "read",  "*",    "*",   -1u,
      "lum",    "read",  "ssbo", "f32", -1u,
      "output", "write", "*",    "*",   &module->connector[0].roi);

  dt_connector_copy(graph, module, 0, id_hist, 0);
  dt_connector_copy(graph, module, 0, id_lum,  0);
  dt_connector_copy(graph, module, 0, id_exp,  0);
  dt_connector_copy(graph, module, 1, id_exp,  2);
  dt_node_connect(graph,  id_hist, 1, id_lum,  1);
  dt_node_connect(graph,  id_lum,  2, id_exp,  1);
  graph->node[id_hist].connector[1].flags |= s_conn_clear; // clear histogram buffer
  graph->node[id_lum].connector[2].flags |= s_conn_protected;
}
