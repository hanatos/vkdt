#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // must have disconnected input somewhere
  uint32_t filters = img_param->filters;
  if(!img_param->filters) return dt_connector_bypass(graph, module, 0, 1);
  // reinterpret to int bits:
  const uint32_t *wb = (uint32_t *)module->img_param.whitebalance;

  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;

  dt_roi_t roic = module->connector[0].roi;
  int block = 2;
  if(filters == 9) block = 3;
  roic.wd /= block;
  roic.ht /= block;

  int pc[] = { wb[0], wb[1], wb[2], wb[3], filters };
  const int id_half = dt_node_add(graph, module, "hilite", "half", wd/block, ht/block, 1,
      sizeof(pc), pc, 2,
      "input",  "read",  "rggb", "ui16", dt_no_roi,
      "output", "write", "rgba", "f16",  &roic);
  const int id_doub = dt_node_add(graph, module, "hilite", "doub", wd/block, ht/block, 1,
      sizeof(pc), pc, 3,
      "input",  "read",  "rggb", "ui16", dt_no_roi,
      "coarse", "read",  "rgba", "f16",  dt_no_roi,
      "output", "write", "rggb", "ui16", &module->connector[0].roi);

  // wire module i/o connectors to nodes:
  dt_connector_copy(graph, module, 0, id_half, 0);
  dt_connector_copy(graph, module, 0, id_doub, 0);
  dt_connector_copy(graph, module, 1, id_doub, 2);

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

  const int max_nl = 15;
  int nl = max_nl;
  for(int l=1;l<nl;l++)
  { // for all coarseness levels
    // add a reduce and an assemble node:
    const int id_reduce = dt_node_add(graph, module, "hilite", "reduce", rc.wd, rc.ht, 1,
        sizeof(pc), pc, 2,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &rc);
    const int id_assemble = dt_node_add(graph, module, "hilite", "assemble", rf.wd, rf.ht, 1,
        sizeof(pc), pc, 3,
        "fine",   "read",  "rgba", "f16", dt_no_roi,
        "coarse", "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &rf);

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
