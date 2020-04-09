#include "config.h"
#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const int dp = 1;

  // input
  //  |
  //  v
  // curve -> curve0.. curve5  gray
  //           |    ..  |       |
  //           v    ..  v       v
  //          reduce   reduce  reduce       (on all levels)
  //           |                |
  //           v   ..           v
  //           C                C -> resample context buf + large scale tone curve/log (one level deeper than the others)
  //
  //      assemble  on all levels, with inputs all buffers from corresponding level

  const int num_gamma = NUM_GAMMA;

  assert(graph->num_nodes < graph->max_nodes);
  const int id_curve = graph->num_nodes++;
  graph->node[id_curve] = (dt_node_t) {
    .name   = dt_token("llap"),
    .kernel = dt_token("curve"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .array_length = num_gamma + 1,
    }},
    .push_constant_size = sizeof(uint32_t),
    .push_constant = { num_gamma },
  };

  dt_roi_t rf = module->connector[0].roi;
  dt_roi_t rc = module->connector[0].roi;
  rc.wd = (rc.wd-1)/2+1;
  rc.ht = (rc.ht-1)/2+1;
  rc.full_wd = (rc.full_wd-1)/2+1;
  rc.full_ht = (rc.full_ht-1)/2+1;

  const int max_nl = 12;
  int nl = max_nl;
  int id_reduce[12] = {-1};
  id_reduce[0] = id_curve;
  int id_assemble[12] = {-1};
  for(int l=1;l<nl;l++)
  { // for all coarseness levels
    assert(graph->num_nodes < graph->max_nodes);
    id_reduce[l] = graph->num_nodes++;
    dt_node_t *node_reduce = graph->node + id_reduce[l];
    *node_reduce = (dt_node_t) {
      .name   = dt_token("llap"),
      .kernel = dt_token("reduce"),
      .module = module,
      .wd     = rc.wd,
      .ht     = rc.ht,
      .dp     = num_gamma + 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("inhi"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = dt_token("f16"),
        .roi    = rf,
        .flags  = s_conn_smooth,
        .connected_mi = -1,
        .array_length = num_gamma + 1,
      },{
        .name   = dt_token("outlo"),
        .type   = dt_token("write"),
        .chan   = dt_token("y"),
        .format = dt_token("f16"),
        .roi    = rc,
        .array_length = num_gamma + 1,
      }},
    };
    // wire input:
    CONN(dt_node_connect(graph, id_reduce[l-1], 1, id_reduce[l], 0));

    // one assemble node taking as input:
    // - coarse output pyramid (start with coarsest of gray after global tonecurve)
    // - fine level of original input colour for laplacian selection
    // - all 6x2 adjacent gamma levels (i/o to reduce)
    // output:
    // - next finer output pyramid
    // const float scale = l/(nl-1.0);
    assert(graph->num_nodes < graph->max_nodes);
    id_assemble[l] = graph->num_nodes++;
    graph->node[id_assemble[l]] = (dt_node_t) {
      .name   = dt_token("llap"),
      .kernel = dt_token("assemble"),
      .module = module,
      .wd     = rf.wd,
      .ht     = rf.ht,
      .dp     = dp,
      .num_connectors = 4,
      .connector = {{
        .name   = dt_token("coarse"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = dt_token("f16"),
        .roi    = rc,
        .flags  = s_conn_smooth,
        .connected_mi = -1,
      },{
        .name   = dt_token("currlo"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = dt_token("f16"),
        .roi    = rc,
        .flags  = s_conn_smooth,
        .connected_mi = -1,
        .array_length = num_gamma + 1,
      },{
        .name   = dt_token("currhi"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = dt_token("f16"),
        .roi    = rf,
        .flags  = s_conn_smooth,
        .connected_mi = -1,
        .array_length = num_gamma + 1,
      },{
        .name   = dt_token("fine"),
        .type   = dt_token("write"),
        .chan   = dt_token("y"),
        .format = dt_token("f16"),
        .roi    = rf,
      }},
      .push_constant_size = 2*sizeof(uint32_t),
      .push_constant = { num_gamma, 0 },
    };
    // connect fine and coarse levels of curve processed buffers:
    CONN(dt_node_connect(graph, id_reduce[l-1], 1, id_assemble[l], 1));
    CONN(dt_node_connect(graph, id_reduce[l  ], 1, id_assemble[l], 2));
    if(l > 1)
      CONN(dt_node_connect(graph, id_assemble[l], 3, id_assemble[l-1], 0));

    rf = rc;
    rc.wd = (rc.wd-1)/2+1;
    rc.ht = (rc.ht-1)/2+1;
    rc.full_wd = (rc.full_wd-1)/2+1;
    rc.full_ht = (rc.full_ht-1)/2+1;
    if(rc.wd <= 1 || rc.ht <= 1)
    { // make sure we have enough resolution
      nl = l+1;
      break;
    }
  }
  // id_assemble[l] now has unconnected [0] because no more coarser
  // bind dummy and let kernel know where to find real data:
  graph->node[id_assemble[nl-1]].push_constant[1] = 1;
  CONN(dt_node_connect(graph, id_curve, 1, id_assemble[nl-1], 0));

  // wire into recolouration node:
  assert(graph->num_nodes < graph->max_nodes);
  const int id_col = graph->num_nodes++;
  graph->node[id_col] = (dt_node_t) {
    .name   = dt_token("llap"),
    .kernel = dt_token("colour"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("lum"),
      .type   = dt_token("read"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
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
  CONN(dt_node_connect(graph, id_assemble[1], 3, id_col, 0));

  // wire module i/o connectors to nodes:
  dt_connector_copy(graph, module, 0, id_curve, 0);
  dt_connector_copy(graph, module, 0, id_col,   1);
  dt_connector_copy(graph, module, 1, id_col,   2);
}

