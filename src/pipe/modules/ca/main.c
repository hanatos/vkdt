#include "modules/localsize.h"
#include "modules/api.h"
#include "connector.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_fit = graph->num_nodes++;
  graph->node[id_fit] = (dt_node_t) {
    .name   = dt_token("ca"),
    .kernel = dt_token("fit"),
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
      .name   = dt_token("corr"),
      .type   = dt_token("write"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    },{
      .name   = dt_token("coef"),
      .type   = dt_token("write"),
      .chan   = dt_token("r"),
      .format = dt_token("atom"),
      .flags  = s_conn_clear,
      .roi    = module->connector[0].roi,
    }},
  };
  const uint32_t id_warp = graph->num_nodes++;
  graph->node[id_warp] = (dt_node_t) {
    .name   = dt_token("ca"),
    .kernel = dt_token("warp"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 4,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("corr"),
      .type   = dt_token("read"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("coef"),
      .type   = dt_token("read"),
      .chan   = dt_token("r"),
      .format = dt_token("atom"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };
  dt_connector_copy(graph, module, 0, id_fit, 0);
  dt_connector_copy(graph, module, 0, id_warp, 0);
  dt_connector_copy(graph, module, 1, id_warp, 3);
  CONN(dt_node_connect(graph, id_fit, 1, id_warp, 1));
  CONN(dt_node_connect(graph, id_fit, 2, id_warp, 2));
}
