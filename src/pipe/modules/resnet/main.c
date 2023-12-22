#include "config.h"
#include "modules/localsize.h"
#include "modules/api.h"
#include "connector.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_cnn = graph->num_nodes++;
  const uint32_t tile_size_x = DT_CNN_TILE_WD - 2*DT_CNN_BORDER;
  const uint32_t tile_size_y = DT_CNN_TILE_HT - 2*DT_CNN_BORDER;
  const uint32_t num_tiles_x = (module->connector[0].roi.wd + tile_size_x - 1)/tile_size_x;
  const uint32_t num_tiles_y = (module->connector[0].roi.ht + tile_size_y - 1)/tile_size_y;
  const uint32_t wd = num_tiles_x * DT_LOCAL_SIZE_X;
  const uint32_t ht = num_tiles_y * DT_LOCAL_SIZE_Y;
  graph->node[id_cnn] = (dt_node_t) {
    .name   = dt_token("resnet"),
    .kernel = dt_token("cnn"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = 1,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("wgt"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[1].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };
  dt_connector_copy(graph, module, 0, id_cnn, 0);
  dt_connector_copy(graph, module, 1, id_cnn, 1);
  dt_connector_copy(graph, module, 2, id_cnn, 2);
}
