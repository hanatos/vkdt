#include "modules/api.h"
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;

  // wire 4 scales of downsample + assembly node
  int id_down[4] = {0};

  for(int i=0;i<4;i++)
    id_down[i] = dt_node_add(graph, module, "wavelet", "down", wd, ht, 1, 0, 0, 2,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[0].roi);
  // wire inputs:
  for(int i=1;i<4;i++)
    CONN(dt_node_connect(graph, id_down[i-1], 1, id_down[i], 0));

  // assemble
  const int id_assemble = dt_node_add(graph, module, "wavelet", "assemble", wd, ht, 1, 0, 0, 7,
      "s0",     "read",  "rgba", "f16", dt_no_roi,
      "s1",     "read",  "rgba", "f16", dt_no_roi,
      "s2",     "read",  "rgba", "f16", dt_no_roi,
      "s3",     "read",  "rgba", "f16", dt_no_roi,
      "s4",     "read",  "rgba", "f16", dt_no_roi,
      "mask",   "read",  "y",    "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);

  // wire downsampled to assembly stage:
  CONN(dt_node_connect(graph, id_down[0], 1, id_assemble, 1));
  CONN(dt_node_connect(graph, id_down[1], 1, id_assemble, 2));
  CONN(dt_node_connect(graph, id_down[2], 1, id_assemble, 3));
  CONN(dt_node_connect(graph, id_down[3], 1, id_assemble, 4));

  // wire module i/o connectors to nodes:
  dt_connector_copy(graph, module, 0, id_down[0], 0);
  dt_connector_copy(graph, module, 0, id_assemble, 0);
  dt_connector_copy(graph, module, 1, id_assemble, 5);
  dt_connector_copy(graph, module, 2, id_assemble, 6);
}
