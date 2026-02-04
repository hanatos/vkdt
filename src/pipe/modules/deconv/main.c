#include "modules/api.h"
#include "config.h"
#include "modules/localsize.h"

// this is an initial implementation of some simplistic iterative, non-blind R/L deconvolution.
// it has a lot of problems because i didn't spend time on it:
// - hardcoded iteration count
// - the division kernel has an epsilon safeguard num/max(den, eps) which i'm not sure about.

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  // yea sorry we're stupid and need push constants which we diligently don't update
  // until we recreate the nodes. should change that i suppose.
  // options:
  // - secretly pick up parameter in the api blur call
  // - make blurs a custom node without api wrapping and access params
  // - implement commit_params() to alter the push constant
  return s_graph_run_all;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_roi_t *roi = &module->connector[0].roi;
  // for dimensions, reverse
  // (wd + DT_LOCAL_SIZE_X - 1) / DT_LOCAL_SIZE_X
  // such that it'll result in our required number of thread blocks.
  const uint32_t tile_size_x = DT_DECONV_TILE_WD - 2*DT_DECONV_BORDER;
  const uint32_t tile_size_y = DT_DECONV_TILE_HT - 2*DT_DECONV_BORDER;
  // how many tiles will we process?
  const uint32_t num_tiles_x = (roi->wd + tile_size_x - 1)/tile_size_x;
  const uint32_t num_tiles_y = (roi->ht + tile_size_y - 1)/tile_size_y;
  const uint32_t wd = num_tiles_x * DT_LOCAL_SIZE_X;
  const uint32_t ht = num_tiles_y * DT_LOCAL_SIZE_Y;
  const int id_deconv = dt_node_add(graph, module, "deconv", "deconv",
      wd, ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &roi);
  dt_connector_copy(graph, module, 0, id_deconv, 0);
  dt_connector_copy(graph, module, 1, id_deconv, 1);
}
