#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t roic = module->connector[0].roi;

  dt_roi_t rf = roic;
  dt_roi_t rc = roic;
  rc.wd = (rc.wd-1)/2+1;
  rc.ht = (rc.ht-1)/2+1;
  rc.full_wd = (rc.full_wd-1)/2+1;
  rc.full_ht = (rc.full_ht-1)/2+1;

  int node_in = -1;
  int conn_in = 0;
  int node_up = -1;
  int conn_up = 1;

  const int max_nl = 5;
  int nl = max_nl;
  for(int l=1;l<nl;l++)
  { // for all coarseness levels
    // add a reduce and an assemble node:
    assert(graph->num_nodes < graph->max_nodes);
    int id_reduce = graph->num_nodes++;
    graph->node[id_reduce] = (dt_node_t) {
      .name   = dt_token("ca"),
      .kernel = dt_token("reduce"),
      .module = module,
      .wd     = rc.wd,
      .ht     = rc.ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgb"),
        .format = dt_token("f16"),
        .roi    = rf,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgb"),
        .format = dt_token("f16"),
        .roi    = rc,
      }},
      .push_constant_size = 1*sizeof(int32_t),
      .push_constant = { l },
    };
    assert(graph->num_nodes < graph->max_nodes);
    int id_assemble = graph->num_nodes++;
    graph->node[id_assemble] = (dt_node_t) {
      .name   = dt_token("ca"),
      .kernel = dt_token("assemble"),
      .module = module,
      .wd     = rf.wd,
      .ht     = rf.ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("fine"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgb"),
        .format = dt_token("f16"),
        .roi    = rf,
        .connected_mi = -1,
      },{
        .name   = dt_token("coarse"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgb"),
        .format = dt_token("f16"),
        .roi    = rc,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgb"),
        .format = dt_token("f16"),
        .roi    = rf,
      }},
      .push_constant_size = 1*sizeof(int32_t),
      .push_constant = { l },
    };

    // wire node connections:
    if(node_in == -1)
    {
      dt_connector_copy(graph, module, 0, id_reduce, 0);
      dt_connector_copy(graph, module, 0, id_assemble, 0);
    }
    else
    {
      CONN(dt_node_connect(graph, node_in, conn_in, id_reduce, 0));
      CONN(dt_node_connect(graph, node_in, conn_in, id_assemble, 0));
    }
    node_in = id_reduce;
    conn_in = 1;
    if(node_up == -1)
      dt_connector_copy(graph, module, 1, id_assemble, 2);
    else CONN(dt_node_connect(graph, id_assemble, 2, node_up, conn_up));
    node_up = id_assemble;
    conn_up = 1;

    rf = rc;
    rc.wd = (rc.wd-1)/2+1;
    rc.ht = (rc.ht-1)/2+1;
    rc.full_wd = (rc.full_wd-1)/2+1;
    rc.full_ht = (rc.full_ht-1)/2+1;
    if(rc.wd <= 1 || rc.ht <= 1 || l+1 == max_nl)
    { // make sure we have enough resolution
      // connect last reduce to last assemble:
      CONN(dt_node_connect(graph, id_reduce, 1, id_assemble, 1));
      break;
    }
  }
}

