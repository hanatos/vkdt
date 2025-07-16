#include "modules/api.h"
#include <inttypes.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int id_diffusion = dt_node_add(graph, module, "physarum", "diffuse", wd, ht, 1, 0, 0, 2,
      "input",  "read",  "*",  "*",    dt_no_roi,
      "output", "write", "rg", "f16", &module->connector[0].roi);
  int id_move = dt_node_add(graph, module, "physarum", "move",
      // XXX run on particles
      // wd, ht, 1, 0, 0, 2,
      "trails", "read", "*", "*", dt_no_roi,
      "part-cnt", "write", "r", "ui32", &module->connector[0].roi, // particle count per pixel
      "part", "write", "r", "ui32", ROI// uhm really f16 2d position, 2d heading, 2d velocity
      );
  // TODO always clear part-cnt
  |= s_conn_clear;

  int id_deposit = dt_node_add(graph, module, "physarum", "deposit", wd, ht, 1, 0, 0, 2,
      "part-cnt", "read", "r", "ui32", &module->connector[0].roi, // particle count per pixel
      "trail read"
      "trail write"
      "output"

}
