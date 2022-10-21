#include "modules/api.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  assert(graph->num_nodes < graph->max_nodes);
  int id_blend = graph->num_nodes++;
  graph->node[id_blend] = (dt_node_t) {
    .name   = dt_token("svgf"),
    .kernel = dt_token("blend"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 7,
    .connector = {{
      .name   = dt_token("mv"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("prevl"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("prevb"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("light"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("albedo"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    },{
      .name   = dt_token("lightout"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
    // .push_constant_size = 9*sizeof(uint32_t),
    // .push_constant = { },
  };

  int id_eaw[4];

  for(int i=0;i<4;i++)
  {
    id_eaw[i] = graph->num_nodes++;
    graph->node[id_eaw[i]] = (dt_node_t) {
      .name   = dt_token("svgf"),
      .kernel = dt_token("atrous"),
      .module = module,
      .wd     = module->connector[0].roi.wd,
      .ht     = module->connector[0].roi.ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("*"),
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
  }

  dt_connector_copy(graph, module, 1, id_eaw[0], 0);  // noisy light
  for(int i=1;i<4;i++)
    CONN(dt_node_connect (graph, id_eaw[i-1], 1, id_eaw[i], 0));

  // CONN(dt_node_feedback(graph, id_eaw[3], 1, id_blend, 1)); // denoised light, old
  CONN(dt_node_feedback(graph, id_blend,  6, id_blend, 1)); // denoised light, old
  CONN(dt_node_feedback(graph, id_blend,  5, id_blend, 2)); // beauty frame, old
  CONN(dt_node_connect (graph, id_eaw[3], 1, id_blend, 3)); // denoised light

  dt_connector_copy(graph, module, 0, id_blend, 0);  // mv
  dt_connector_copy(graph, module, 2, id_blend, 4);  // albedo
  dt_connector_copy(graph, module, 3, id_blend, 5);  // output beauty
}
