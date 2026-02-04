#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t rf = module->connector[0].roi;
  dt_roi_t rc = module->connector[0].roi;
  rc.wd = (rc.wd-1)/2+1;
  rc.ht = (rc.ht-1)/2+1;
  rc.full_wd = (rc.full_wd-1)/2+1;
  rc.full_ht = (rc.full_ht-1)/2+1;

  int node_in = -1;
  int conn_in = 1;
  int node_up = -1;
  int conn_up = 1;

  const int max_nl = 9;
  int nl = max_nl;
  for(int l=0;l<nl;l++)
  { // for all coarseness levels
    // add a reduce and an assemble node:
    int pc[] = { l };
    const int id_reduce = dt_node_add(graph, module, "inpaint", "reduce", rc.wd, rc.ht, 1,
        sizeof(pc), pc, 3,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "mask",   "read",  "r",    "*",   dt_no_roi,
        "output", "write", "rgba", "f16", &rc);
    const int id_assemble = dt_node_add(graph, module, "inpaint", "assemble", rf.wd, rf.ht, 1,
        sizeof(pc), pc, 4,
        "fine",   "read",  "rgba", "f16", dt_no_roi,
        "coarse", "read",  "rgba", "f16", dt_no_roi,
        "mask",   "read",  "r",    "*",   dt_no_roi,
        "output", "write", "rgba", "f16", &rf);

    // wire node connections:
    if(node_in == -1) dt_connector_copy(graph, module, 0, id_reduce, 0);
    else    CONN(dt_node_connect(graph, node_in, conn_in, id_reduce, 0));
    if(node_in == -1) dt_connector_copy(graph, module, 0, id_assemble, 0);
    else    CONN(dt_node_connect(graph, node_in, conn_in, id_assemble, 0));
    node_in = id_reduce;
    conn_in = 2;
    if(node_up == -1) dt_connector_copy(graph, module, 2, id_assemble, 3);
    else CONN(dt_node_connect(graph, id_assemble, 3, node_up, conn_up));
    node_up = id_assemble;
    conn_up = 1;
    dt_connector_copy(graph, module, 1, id_reduce,   1);
    dt_connector_copy(graph, module, 1, id_assemble, 2);

    rf = rc;
    rc.wd = (rc.wd-1)/2+1;
    rc.ht = (rc.ht-1)/2+1;
    rc.full_wd = (rc.full_wd-1)/2+1;
    rc.full_ht = (rc.full_ht-1)/2+1;
    if(rc.wd <= 1 || rc.ht <= 1 || l+1 == max_nl)
    { // make sure we have enough resolution
      // connect last reduce to last assemble:
      CONN(dt_node_connect(graph, id_reduce, 2, id_assemble, 1));
      break;
    }
  }
}

