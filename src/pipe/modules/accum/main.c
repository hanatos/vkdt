#include "modules/api.h"
void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int id_main = dt_node_add(graph, module, "accum", "main",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 3,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "output", "write", "rgba", "f32", &module->connector[0].roi,
      "back",   "read",  "*",    "*",   dt_no_roi);
  graph->node[id_main].connector[1].flags |= s_conn_clear_once;
  dt_connector_copy(graph, module, 0, id_main, 0);
  dt_connector_copy(graph, module, 1, id_main, 1);
  dt_connector_copy(graph, module, 2, id_main, 2);
}
