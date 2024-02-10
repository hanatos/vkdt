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
    id_down[l] = dt_node_add(graph, module, "eq", "down",
        roic.wd, roic.ht, 1, 0, 0, 2,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &roic);
    int32_t pc[] = { l + loff };
    id_up[l] = dt_node_add(graph, module, "eq", "up",
        roif.wd, roif.ht, 1, sizeof(int32_t), pc, 4,
        "coarse0", "read",  "rgba", "f16", dt_no_roi,
        "coarse1", "read",  "rgba", "f16", dt_no_roi,
        "fine",    "read",  "rgba", "f16", dt_no_roi,
        "output",  "write", "rgba", "f16", &roif);
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

