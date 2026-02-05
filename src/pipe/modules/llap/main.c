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

  int pc[] = { num_gamma };
  const int id_curve = dt_node_add(graph, module, "llap", "curve", wd, ht, dp,
      sizeof(pc), pc, 2,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "y",    "f16", &module->connector[0].roi);
  graph->node[id_curve].connector[1].array_length = num_gamma + 1;

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
    id_reduce[l] = dt_node_add(graph, module, "llap", "reduce", rc.wd, rc.ht, num_gamma+1,
        0, 0, 2,
        "inhi",  "read",  "y", "f16", dt_no_roi,
        "outlo", "write", "y", "f16", &rc);
    graph->node[id_reduce[l]].connector[1].array_length = num_gamma + 1;
    // wire input:
    CONN(dt_node_connect(graph, id_reduce[l-1], 1, id_reduce[l], 0));

    // one assemble node taking as input:
    // - coarse output pyramid (start with coarsest of gray after global tonecurve)
    // - fine level of original input colour for laplacian selection
    // - all 6x2 adjacent gamma levels (i/o to reduce)
    // output:
    // - next finer output pyramid
    // const float scale = l/(nl-1.0);
    int pc[] = {num_gamma, 0};
    id_assemble[l] = dt_node_add(graph, module, "llap", "assemble", rf.wd, rf.ht, dp,
        sizeof(pc), pc, 4,
        "coarse", "read",  "y", "f16", dt_no_roi,
        "currlo", "read",  "y", "f16", dt_no_roi,
        "currhi", "read",  "y", "f16", dt_no_roi,
        "fine",   "write", "y", "f16", &rf);
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
  const int id_col = dt_node_add(graph, module, "llap", "colour", wd, ht, dp, 0, 0, 3,
      "lum",    "read",  "y",    "f16", dt_no_roi,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  CONN(dt_node_connect(graph, id_assemble[1], 3, id_col, 0));

  // wire module i/o connectors to nodes:
  dt_connector_copy(graph, module, 0, id_curve, 0);
  dt_connector_copy(graph, module, 0, id_col,   1);
  dt_connector_copy(graph, module, 1, id_col,   2);
}
