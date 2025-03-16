#include "modules/api.h"

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int have_Dmin = dt_connected(module->connector+2);
  const int pc[] = { have_Dmin };
  const int nodeid = dt_node_add(graph, module, "negative", "main",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, sizeof(pc), pc, 3,
      "input",  "read",  "*",    "*",    dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi,
      "dmin",   "read",  "*",    "*",    dt_no_roi);
  dt_connector_copy(graph, module, 0, nodeid, 0);
  dt_connector_copy(graph, module, 1, nodeid, 1);
  if(have_Dmin)  dt_connector_copy(graph, module, 2, nodeid, 2);
  else           dt_connector_copy(graph, module, 0, nodeid, 2); // dummy
}
