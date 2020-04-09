#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  uint32_t filters = module->img_param.filters;
  // reinterpret to int bits:
  const uint32_t *wb = (uint32_t *)module->img_param.whitebalance;

  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;

  dt_roi_t roic = module->connector[0].roi;
  int block = 2;
  if(filters == 9) block = 3;
  roic.wd /= block;
  roic.ht /= block;

  assert(graph->num_nodes < graph->max_nodes);
  const int id_half = graph->num_nodes++;
  graph->node[id_half] = (dt_node_t) {
    .name   = dt_token("hilite"),
    .kernel = dt_token("half"),
    .module = module,
    .wd     = wd/block,
    .ht     = ht/block,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rggb"),
      .format = dt_token("ui16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgb"),
      .format = dt_token("f16"),
      .roi    = roic,
    }},
    .push_constant_size = 5*sizeof(float),
    .push_constant = { wb[0], wb[1], wb[2], wb[3], filters },
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_doub = graph->num_nodes++;
  graph->node[id_doub] = (dt_node_t) {
    .name   = dt_token("hilite"),
    .kernel = dt_token("doub"),
    .module = module,
    .wd     = wd/block,
    .ht     = ht/block,
    .dp     = 1,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rggb"),
      .format = dt_token("ui16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("coarse"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgb"),
      .format = dt_token("f16"),
      .roi    = roic,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rggb"),
      .format = dt_token("ui16"),
      .roi    = module->connector[0].roi,
    }},
    .push_constant_size = 5*sizeof(float),
    .push_constant = { wb[0], wb[1], wb[2], wb[3], filters },
  };

  // wire module i/o connectors to nodes:
  dt_connector_copy(graph, module, 0, id_half, 0);
  dt_connector_copy(graph, module, 0, id_doub, 0);
  dt_connector_copy(graph, module, 1, id_doub, 2);

#if 0 // XXX DEBUG
  CONN(dt_node_connect(graph, id_half, 1, id_doub, 1));
  return;
#endif

  dt_roi_t rf = roic;
  dt_roi_t rc = roic;
  rc.wd = (rc.wd-1)/2+1;
  rc.ht = (rc.ht-1)/2+1;
  rc.full_wd = (rc.full_wd-1)/2+1;
  rc.full_ht = (rc.full_ht-1)/2+1;

  int node_in = id_half;
  int conn_in = 1;
  int node_up = id_doub;
  int conn_up = 1;

  const int max_nl = 9;//8;
  int nl = max_nl;
  for(int l=1;l<nl;l++)
  { // for all coarseness levels
    // add a reduce and an assemble node:
    assert(graph->num_nodes < graph->max_nodes);
    int id_reduce = graph->num_nodes++;
    graph->node[id_reduce] = (dt_node_t) {
      .name   = dt_token("hilite"),
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
      .push_constant_size = 5*sizeof(float),
      .push_constant = { wb[0], wb[1], wb[2], wb[3], filters },
    };
    assert(graph->num_nodes < graph->max_nodes);
    int id_assemble = graph->num_nodes++;
    graph->node[id_assemble] = (dt_node_t) {
      .name   = dt_token("hilite"),
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
      .push_constant_size = 6*sizeof(float),
      .push_constant = { wb[0], wb[1], wb[2], wb[3], filters, l },
    };

    // wire node connections:
    CONN(dt_node_connect(graph, node_in, conn_in, id_reduce, 0));
    CONN(dt_node_connect(graph, node_in, conn_in, id_assemble, 0));
    node_in = id_reduce;
    conn_in = 1;
    CONN(dt_node_connect(graph, id_assemble, 2, node_up, conn_up));
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

