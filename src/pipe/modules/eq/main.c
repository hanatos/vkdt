#include "modules/api.h"
#include <math.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t roif = module->connector[0].roi;
  dt_roi_t roic = roif;
  const int maxnuml = 6;
  const int loff = log2f(roif.full_wd / roif.wd);
  const int numl = 6-loff;
  int id_down[maxnuml], id_up[maxnuml];
  for(int l=0;l<numl;l++)
  {
    roic.wd = (roif.wd+1)/2;
    roic.ht = (roif.ht+1)/2;
    assert(graph->num_nodes < graph->max_nodes);
    id_down[l] = graph->num_nodes++;
    graph->node[id_down[l]] = (dt_node_t) {
      .name   = dt_token("eq"),
      .kernel = dt_token("down"),
      .module = module,
      .wd     = roic.wd,
      .ht     = roic.ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roif,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roic,
      }},
    };
    assert(graph->num_nodes < graph->max_nodes);
    id_up[l] = graph->num_nodes++;
    graph->node[id_up[l]] = (dt_node_t) {
      .name   = dt_token("eq"),
      .kernel = dt_token("up"),
      .module = module,
      .wd     = roif.wd,
      .ht     = roif.ht,
      .dp     = 1,
      .num_connectors = 4,
      .connector = {{
        .name   = dt_token("coarse0"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roic,
        .connected_mi = -1,
      },{
        .name   = dt_token("coarse1"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roic,
        .connected_mi = -1,
      },{
        .name   = dt_token("fine"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roif,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roif,
      }},
      .push_constant_size = sizeof(uint32_t),
      .push_constant = { l + loff },
    };
    roif = roic;
  }
  dt_connector_copy(graph, module, 0, id_down[0], 0);
  dt_connector_copy(graph, module, 0, id_up[0],   2);
  dt_connector_copy(graph, module, 1, id_up[0],   3); // output

  for(int l=1;l<numl;l++)
  {
    dt_node_connect(graph, id_down[l-1], 1, id_down[l], 0); // downsize more
    dt_node_connect(graph, id_down[l-1], 1, id_up[l],   2); // fine for details during upsizing
    dt_node_connect(graph, id_down[l-1], 1, id_up[l-1], 0); // unaltered coarse
    dt_node_connect(graph, id_up[l],     3, id_up[l-1], 1); // pass on to next level
  }
  dt_node_connect(graph, id_down[numl-1], 1, id_up[numl-1], 0); // coarsest level
  dt_node_connect(graph, id_down[numl-1], 1, id_up[numl-1], 1);
}

