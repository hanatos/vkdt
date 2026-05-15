#include "modules/api.h"

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int id_main = dt_node_add(graph, module, "reinhard", "main",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id_main, 0);
  dt_connector_copy(graph, module, 1, id_main, 1);
}
