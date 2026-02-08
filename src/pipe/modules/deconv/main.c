#include "modules/api.h"
#include "config.h"
#include "modules/localsize.h"

// this is an initial implementation of some simplistic iterative, non-blind R/L deconvolution.

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // for dimensions, reverse
  // (wd + DT_LOCAL_SIZE_X - 1) / DT_LOCAL_SIZE_X
  // such that it'll result in our required number of thread blocks.
  const uint32_t tile_size_x = DT_DECONV_TILE_WD - 2*DT_DECONV_BORDER;
  const uint32_t tile_size_y = DT_DECONV_TILE_HT - 2*DT_DECONV_BORDER;
  // how many tiles will we process?
  const uint32_t num_tiles_x = (module->connector[0].roi.wd + tile_size_x - 1)/tile_size_x;
  const uint32_t num_tiles_y = (module->connector[0].roi.ht + tile_size_y - 1)/tile_size_y;
  const uint32_t wd = num_tiles_x * DT_LOCAL_SIZE_X;
  const uint32_t ht = num_tiles_y * DT_LOCAL_SIZE_Y;
  const int id_deconv = dt_node_add(graph, module, "deconv", "deconv",
      wd, ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id_deconv, 0);
  dt_connector_copy(graph, module, 1, id_deconv, 1);
}
