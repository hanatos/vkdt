#include "modules/api.h"
#include <stdio.h>

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if(parid == 2)
  { // mode
    int oldmode = *(int*)oldval;
    int newmode = dt_module_param_int(module, parid)[0];
    if(oldmode != newmode && (oldmode == 3 || newmode == 3))
      return s_graph_run_all;
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  const int32_t p_mode = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("mode")))[0];
  dt_roi_t roif = module->connector[0].roi;
  if(p_mode == 3)
  { // focus stack blend mode
    dt_roi_t roic = roif;
    const int maxnuml = 14;
    const int loff = log2f(roif.full_wd / roif.wd);
    const int numl = maxnuml-loff;
    int id_down0[maxnuml], id_down1[maxnuml], id_up[maxnuml];
    for(int l=0;l<numl;l++)
    {
      roic.wd = (roif.wd+1)/2;
      roic.ht = (roif.ht+1)/2;
      id_down0[l] = dt_node_add(graph, module, "eq", "down",
          roic.wd, roic.ht, 1, 0, 0, 2,
          "input",  "read",  "rgba", "f16", &roif,
          "output", "write", "rgba", "f16", &roic);
      id_down1[l] = dt_node_add(graph, module, "eq", "down",
          roic.wd, roic.ht, 1, 0, 0, 2,
          "input",  "read",  "rgba", "f16", &roif,
          "output", "write", "rgba", "f16", &roic);
      id_up[l] = dt_node_add(graph, module, "blend", "up",
          roif.wd, roif.ht, 1, 0, 0, 6,
          "coarse0", "read",  "rgba", "f16", &roic,
          "fine0",   "read",  "rgba", "f16", &roif,
          "coarse1", "read",  "rgba", "f16", &roic,
          "fine1",   "read",  "rgba", "f16", &roif,
          "co",      "read",  "rgba", "f16", &roic,
          "output",  "write", "rgba", "f16", &roif);
      roif = roic;
    }
    dt_connector_copy(graph, module, 0, id_down0[0], 0);
    dt_connector_copy(graph, module, 1, id_down1[0], 0);
    dt_connector_copy(graph, module, 3, id_up[0],    5); // output
    dt_connector_copy(graph, module, 0, id_up[0],    1);
    dt_connector_copy(graph, module, 1, id_up[0],    3);
    for(int l=1;l<numl;l++)
    {
      dt_node_connect(graph, id_down0[l-1], 1, id_down0[l], 0); // downsize more
      dt_node_connect(graph, id_down1[l-1], 1, id_down1[l], 0);
      dt_node_connect(graph, id_down0[l-1], 1, id_up[l],    1); // fine for details during upsizing
      dt_node_connect(graph, id_down1[l-1], 1, id_up[l],    3);
      dt_node_connect(graph, id_down0[l-1], 1, id_up[l-1],  0); // coarse
      dt_node_connect(graph, id_down1[l-1], 1, id_up[l-1],  2);
      dt_node_connect(graph, id_up[l],      5, id_up[l-1],  4); // pass on to next level
    }
    dt_node_connect(graph, id_down0[numl-1], 1, id_up[numl-1], 0); // coarsest level
    dt_node_connect(graph, id_down1[numl-1], 1, id_up[numl-1], 2);
    dt_node_connect(graph, id_down0[numl-1], 1, id_up[numl-1], 4); // use coarsest from back buffer
  }
  else
  { // per pixel blending, all the same kernel
    int id_main = dt_node_add(graph, module, "blend", "main",
        roif.wd, roif.ht, 1, 0, 0, 4,
        "back",   "read",  "rgba", "f16", &roif,
        "top",    "read",  "rgba", "f16", &roif,
        "mask",   "read",  "r",    "f16", &roif,
        "output", "write", "rgba", "f16", &roif);
    for(int k=0;k<4;k++) dt_connector_copy(graph, module, k, id_main, k);
  }
}
