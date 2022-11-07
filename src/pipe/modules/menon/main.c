#include "modules/api.h"
#include "config.h"
#include "modules/localsize.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_roi_t *roi = &module->connector[0].roi;
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_menon = graph->num_nodes++;
  // for dimensions, reverse
  // (wd + DT_LOCAL_SIZE_X - 1) / DT_LOCAL_SIZE_X
  // such that it'll result in our required number of thread blocks.
  const uint32_t tile_size_x = DT_MENON_TILE_WD - 2*DT_MENON_BORDER;
  const uint32_t tile_size_y = DT_MENON_TILE_HT - 2*DT_MENON_BORDER;
  // how many tiles will we process?
  const uint32_t num_tiles_x = (roi->wd + tile_size_x - 1)/tile_size_x;
  const uint32_t num_tiles_y = (roi->ht + tile_size_y - 1)/tile_size_y;
  const uint32_t wd = num_tiles_x * DT_LOCAL_SIZE_X;
  const uint32_t ht = num_tiles_y * DT_LOCAL_SIZE_Y;
  graph->node[id_menon] = (dt_node_t) {
    .name   = dt_token("menon"),
    .kernel = dt_token("menon"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rggb"),
      .format = dt_token("*"),
      .roi    = *roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
    }},
  };
  dt_connector_copy(graph, module, 0, id_menon, 0);
  dt_connector_copy(graph, module, 1, id_menon, 1);
}
