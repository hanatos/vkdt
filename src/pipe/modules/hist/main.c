#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // always request constant histogram size:
  module->connector[1].roi = module->connector[0].roi;
  // smaller sizes are slower (at least on intel), probably more atomic contention:
  module->connector[1].roi.full_wd = 1000;//490;
  module->connector[1].roi.full_ht =  600;//300;
}

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
    .format = dt_token("f16"),
    .roi    = module->connector[0].roi,
    .connected_mi = -1,
  };
  dt_connector_t co_r = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("r"),
    .format = dt_token("ui32"),
    .roi    = module->connector[1].roi,
    .flags  = s_conn_clear,
  };
  dt_connector_t co_g = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("r"),
    .format = dt_token("ui32"),
    .roi    = module->connector[1].roi,
    .flags  = s_conn_clear,
  };
  dt_connector_t co_b = {
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
    .num_connectors = 4,
    .connector = {
      ci, co_r, co_g, co_b,
    },
  };
  dt_connector_t ci_r = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("r"),
    .format = dt_token("ui32"),
    .roi    = co_r.roi,
    .flags  = s_conn_clear,
    .connected_mi = -1,
  };
  dt_connector_t ci_g = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("r"),
    .format = dt_token("ui32"),
    .roi    = co_g.roi,
    .flags  = s_conn_clear,
    .connected_mi = -1,
  };
  dt_connector_t ci_b = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("r"),
    .format = dt_token("ui32"),
    .roi    = co_b.roi,
    .flags  = s_conn_clear,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = module->connector[1].roi,
    .flags  = 0,
  };
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
    .num_connectors = 4,
    .connector = {
      ci_r, ci_g, ci_b, co,
    },
  };

  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);
  dt_node_connect  (graph, id_collect, 2, id_map, 1);
  dt_node_connect  (graph, id_collect, 3, id_map, 2);
  dt_connector_copy(graph, module, 1, id_map, 3);
}
