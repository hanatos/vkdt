#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if(parid == 0)
  { // radius
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(module, 0)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // radius is the first parameter in our params file
  const float radius = dt_module_param_float(module, 0)[0];

  // number of iterations in our pipeline configuration:
  const int num_it = 12;

  // init node to compute developer concentration from initial image
  assert(graph->num_nodes < graph->max_nodes);
  const int id_init = graph->num_nodes++;
  graph->node[id_init] = (dt_node_t) {
    .name   = dt_token("filmsim"),
    .kernel = dt_token("init"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("aux"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };

  int id_dev = id_init;
  int id_last = id_init;
  int cn_last = 1;
  for(int i=0;i<num_it;i++)
  {
    assert(graph->num_nodes < graph->max_nodes);
    const int id_proc = graph->num_nodes++;
    graph->node[id_proc] = (dt_node_t) {
      .name   = dt_token("filmsim"),
      .kernel = dt_token("main"),
      .module = module,
      .wd     = module->connector[0].roi.wd,
      .ht     = module->connector[0].roi.ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("aux"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = module->connector[0].roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("dev"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .flags  = s_conn_smooth,
        .roi    = graph->node[id_dev].connector[1].roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = module->connector[0].roi,
      }},
    };
    CONN(dt_node_connect(graph, id_last, cn_last, id_proc, 0));
    CONN(dt_node_connect(graph, id_dev,  1,       id_proc, 1));
    // luckily the output connector of blur is also number 1,
    // so we only store the node id:
    id_dev = dt_api_blur_sub(graph, module, id_proc, 2, 0, 0, radius);
    id_last = id_proc;
    cn_last = 2;
  }

  // output node to convert densities to colour
  assert(graph->num_nodes < graph->max_nodes);
  const int id_final = graph->num_nodes++;
  graph->node[id_final] = (dt_node_t) {
    .name   = dt_token("filmsim"),
    .kernel = dt_token("final"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("aux"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = graph->node[id_last].connector[cn_last].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };
  CONN(dt_node_connect(graph, id_last, cn_last, id_final, 0));
  dt_connector_copy(graph, module, 0, id_init,  0); // rgb input
  dt_connector_copy(graph, module, 1, id_final, 1); // rgb output
}

