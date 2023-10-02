#include "modules/api.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t hroi = (dt_roi_t){.wd = 16+1, .ht = 16 };
  dt_roi_t lroi = (dt_roi_t){.wd = 8, .ht = 8};
  const int id_hist = dt_node_add(graph, module, "autoexp", "hist", (wd+15)/16 * DT_LOCAL_SIZE_X, (ht+15)/16 * DT_LOCAL_SIZE_Y, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   -1ul,
      "hist",   "write", "ssbo", "u32", &hroi);

  const int id_lum = dt_node_add(graph, module, "autoexp", "lum", 1, 1, 1, 0, 0, 3,
      "input",  "read",  "*",    "*",   -1ul,
      "hist",   "read",  "ssbo", "u32", -1ul,
      "lum",    "write", "ssbo", "f32", &lroi);

  const int id_exp = dt_node_add(graph, module, "autoexp", "exp", wd, ht, 1, 0, 0, 4,
      "input",  "read",  "*",    "*",   -1ul,
      "hist",   "read",  "ssbo", "u32", -1ul,
      "lum",    "read",  "ssbo", "f32", -1ul,
      "output", "write", "rgba", "*",   &module->connector[0].roi);

  dt_connector_copy(graph, module, 0, id_hist, 0);
  dt_connector_copy(graph, module, 0, id_lum,  0);
  dt_connector_copy(graph, module, 0, id_exp,  0);
  dt_connector_copy(graph, module, 1, id_exp,  3);
  dt_node_connect(graph,  id_hist, 1, id_lum,  1);
  dt_node_connect(graph,  id_hist, 1, id_exp,  1);
  dt_node_connect(graph,  id_lum,  2, id_exp,  2);
  graph->node[id_hist].connector[1].flags |= s_conn_clear; // clear histogram buffer
  graph->node[id_lum].connector[2].flags |= s_conn_protected;
}
