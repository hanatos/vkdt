#include "modules/api.h"

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{ // do not init [0], we'll go with whatever the other connectors may ask
  module->connector[1].roi.wd = module->connector[1].roi.full_wd;
  module->connector[1].roi.ht = module->connector[1].roi.full_ht;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // always request constant histogram size:
  module->connector[1].roi = module->connector[0].roi;
  module->connector[1].roi.full_wd = 1000;
  module->connector[1].roi.full_ht = 1000;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int id_collect = dt_node_add(graph, module, "ciediag", "collect",
    module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 2,
    "input",  "read",  "rgba", "f16",  dt_no_roi,
    "output", "write", "r",    "ui32", &module->connector[1].roi);
  graph->node[id_collect].connector[1].flags = s_conn_clear;
  const int id_map = dt_node_add(graph, module, "ciediag", "map", 
    module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
    "input",  "read",  "r",    "ui32", dt_no_roi,
    "output", "write", "rgba", "f16",  &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);
  dt_connector_copy(graph, module, 1, id_map, 1);
}
