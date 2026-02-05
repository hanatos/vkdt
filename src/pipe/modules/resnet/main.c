#include "config.h"
#include "modules/localsize.h"
#include "modules/api.h"
#include "connector.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const uint32_t tile_size_x = DT_CNN_TILE_WD - 2*DT_CNN_BORDER;
  const uint32_t tile_size_y = DT_CNN_TILE_HT - 2*DT_CNN_BORDER;
  const uint32_t num_tiles_x = (module->connector[0].roi.wd + tile_size_x - 1)/tile_size_x;
  const uint32_t num_tiles_y = (module->connector[0].roi.ht + tile_size_y - 1)/tile_size_y;
  const uint32_t wd = num_tiles_x * DT_LOCAL_SIZE_X;
  const uint32_t ht = num_tiles_y * DT_LOCAL_SIZE_Y;
  const int id_cnn = dt_node_add(graph, module, "resnet", "cnn", wd, ht, 1, 0, 0, 3,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "wgt",    "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  dt_connector_copy(graph, module, 0, id_cnn, 0);
  dt_connector_copy(graph, module, 1, id_cnn, 1);
  dt_connector_copy(graph, module, 2, id_cnn, 2);
}
