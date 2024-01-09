#include "modules/api.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t croi = (dt_roi_t){.wd = 7, .ht = 1024 };
  dt_roi_t rroi = (dt_roi_t){.wd = 6, .ht = 1};
  const int id_col = dt_node_add(graph, module, "mv2rot", "col", 1024*DT_LOCAL_SIZE_X, 1, 1, 0, 0, 3,
      "mv",   "read",  "*",    "*",   dt_no_roi,
      "mask", "read",  "*",    "*",   dt_no_roi,
      "col",  "write", "ssbo", "u32", &croi);

  const int id_sel = dt_node_add(graph, module, "mv2rot", "sel", 1, 1, 1, 0, 0, 2,
      "col", "read",  "ssbo", "u32", dt_no_roi,
      "rot", "write", "ssbo", "f32", &rroi);

  const int id_rot = dt_node_add(graph, module, "mv2rot", "rot", wd, ht, 1, 0, 0, 3,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "rot",    "read",  "ssbo", "f32", dt_no_roi,
      "output", "write", "rgba", "*",   &module->connector[0].roi);

  dt_connector_copy(graph, module, 1, id_col, 0);
  dt_connector_copy(graph, module, 2, id_col, 1);
  dt_connector_copy(graph, module, 0, id_rot, 0);
  dt_connector_copy(graph, module, 3, id_rot, 2);
  dt_node_connect(graph,  id_col, 2, id_sel,  0);
  dt_node_connect(graph,  id_sel, 1, id_rot,  1);
}
