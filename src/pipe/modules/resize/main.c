#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{ // request the full thing, we'll rescale
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  // we *don't* set the scale factor. this is used internally as
  // proof that the roi is initialised, but the only use is disambiguation
  // when one output is connected to several chains. by *not* setting the scale
  // here, we suggest a new resolution but if any other chain is more opinionated
  // than us, we let them override our numbers.
  // module->connector[0].roi.scale = 1.0f;
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  if(module->connector[0].roi.wd == module->connector[1].roi.wd &&
     module->connector[0].roi.ht == module->connector[1].roi.ht)
  return dt_connector_bypass(graph, module, 0, 1);
  const int nodeid = dt_node_add(graph, module, "resize", "main",
      module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "*",    dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, nodeid, 0);
  dt_connector_copy(graph, module, 1, nodeid, 1);
}
