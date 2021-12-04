#include "config.h"
#include "modules/localsize.h"
#include "modules/api.h"
#include "connector.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
#if 1 // shared memory
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_cnn = graph->num_nodes++;
  const uint32_t tile_size_x = DT_CNN_TILE_WD - 2*DT_CNN_BORDER;
  const uint32_t tile_size_y = DT_CNN_TILE_HT - 2*DT_CNN_BORDER;
  const uint32_t num_tiles_x = (module->connector[0].roi.wd + tile_size_x - 1)/tile_size_x;
  const uint32_t num_tiles_y = (module->connector[0].roi.ht + tile_size_y - 1)/tile_size_y;
  const uint32_t wd = num_tiles_x * DT_LOCAL_SIZE_X;
  const uint32_t ht = num_tiles_y * DT_LOCAL_SIZE_Y;
  graph->node[id_cnn] = (dt_node_t) {
    .name   = dt_token("cnn"),
    .kernel = dt_token("cnn"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = 4,
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
#else // microkernels
  // network configuration:
  // 5x5 blur appended to input
  // sum input back to result of network
  // last layer witouth relu
  const int numl = 16;
  float push[] = { 0.05, 1.0 };
  uint32_t *pushi = (uint32_t *)push;

  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_blur = graph->num_nodes++;
  graph->node[id_blur] = (dt_node_t) {
    .name   = dt_token("cnn"),
    .kernel = dt_token("blur"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
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
      .array_length = 2,
    }},
  };

  uint32_t id_C16 = id_blur;
  int cin = 3, cout = 6;
  cin = cout;

  int off = 1; // y coordinate in weights buffer for current layer
  for(int l=0;l<numl;l++)
  {
    cout = 16;
    if(l == numl-1) cout = 3;
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_conv = graph->num_nodes++;
    graph->node[id_conv] = (dt_node_t) {
      .name   = dt_token("cnn"),
      .kernel = dt_token("conv"),
      .module = module,
      .wd     = module->connector[0].roi.wd,
      .ht     = module->connector[0].roi.ht,
      .dp     = (cout+3)/4,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = module->connector[0].roi,
        .array_length = (cin+3)/4,
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
        .array_length = (cout+3)/4,
      }},
      .push_constant_size = 4*sizeof(uint32_t),
      .push_constant = { cin, cout, off, l == 15 ? pushi[1] : pushi[0] },
    };
    dt_node_connect(graph, id_C16, 2, id_conv, 0);
    dt_connector_copy(graph, module, 1, id_conv, 1);
    id_C16 = id_conv;
    off += (cout+3)/4;
    cin = cout;
  }
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_sum = graph->num_nodes++;
  graph->node[id_sum] = (dt_node_t) {
    .name   = dt_token("cnn"),
    .kernel = dt_token("sum"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("in0"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
      .array_length = 2,
    },{
      .name   = dt_token("in1"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
      .array_length = 1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };
  dt_connector_copy(graph, module, 0, id_blur, 0);
  dt_connector_copy(graph, module, 1, id_blur, 1);
  dt_node_connect(graph,   id_blur, 2, id_sum, 0);
  dt_node_connect(graph,   id_C16,  2, id_sum, 1);
  dt_connector_copy(graph, module,  2, id_sum, 2);
  // dt_connector_copy(graph, module, 2, id_blur, 2); // XXX DEBUG
  // dt_node_connect(graph, id_blur, 2, id_sum, 1); // XXX DEBUG
#endif
}

