#include "modules/api.h"

// TODO: roi in roi out

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // input -> collect -> map -> output

  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f32"),
    .roi    = module->connector[0].roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("r"),
    .format = dt_token("ui32"),
    .roi    = module->connector[1].roi,
    .flags  = s_conn_clear,
  };

  assert(graph->num_nodes < graph->max_nodes);
  const int id_collect = graph->num_nodes++;
  dt_node_t *node_collect = graph->node + id_collect;
  *node_collect = (dt_node_t) {
    .name   = dt_token("hist"),
    .kernel = dt_token("collect"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };
  ci.roi    = co.roi;
  ci.chan   = dt_token("r");
  ci.format = dt_token("ui32");
  co.chan   = dt_token("rgba");
  co.format = dt_token("f32");
  co.flags  = 0;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_map = graph->num_nodes++;
  dt_node_t *node_map = graph->node + id_map;
  *node_map = (dt_node_t) {
    .name   = dt_token("hist"),
    .kernel = dt_token("map"),
    .module = module,
    .wd     = module->connector[1].roi.wd,
    .ht     = module->connector[1].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  // interconnect nodes:
  dt_connector_copy(graph, module, id_collect, 0, 0);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);
  dt_connector_copy(graph, module, id_map, 1, 1);
}

