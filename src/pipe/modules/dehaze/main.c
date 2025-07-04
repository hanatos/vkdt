#include "modules/api.h"
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;

  // dt_roi_t roi = { .wd = (wd+7)/8, .ht = (ht+7)/8, .scale=1 };
  dt_roi_t roi = { .wd = wd, .ht = ht, .scale=1 };
  const int id_depth = dt_node_add(graph, module, "dehaze", "depth", wd, ht, 1, 0, 0, 2,
      "input", "read",  "rgba", "*",    dt_no_roi,
      "depth", "write", "rgba", "f16", &roi);
  dt_connector_copy(graph, module, 0, id_depth, 0);
  const float radius = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("radius")))[0];
  const float edges  = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("epsilon")))[0];
  const int id_guided = dt_api_guided_filter_full(graph, module,
      -1, 0,       // input, determines buffer sizes
      id_depth, 1, // guide
      0, 0,        // not interested in internal node ids
      radius, edges);
  const int id_dehaze = dt_node_add(graph, module, "dehaze", "dehaze", wd, ht, 1, 0, 0, 3,
      "input",  "read",  "rgba", "*",    dt_no_roi,
      "depth",  "read",  "rgba", "*",    dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  CONN(dt_node_connect_named(graph, id_guided, "output", id_dehaze, "depth"));
  // CONN(dt_node_connect(graph, id_guided, 2, id_dehaze, 1));
  dt_connector_copy(graph, module, 0, id_dehaze, 0);
  dt_connector_copy(graph, module, 1, id_dehaze, 2);
  // TODO route out upscaled depth buffer?
  // dt_connector_copy(graph, module, 1, id_depth, 1); // XXX debug
  // dt_connector_copy(graph, module, 1, id_guided, 2); // XXX debug
}
