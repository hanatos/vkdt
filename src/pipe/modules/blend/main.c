#include "modules/api.h"
#include <stdio.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  uint32_t *i = (uint32_t *)module->committed_param;
  float *g = (float*)module->param;
  i[0] = module->connector[0].chan == dt_token("rggb") ? module->img_param.filters : (graph->frame_cnt > 1 ? 0u : 1u);
  f[1] = graph->frame == 0 && graph->frame_cnt > 1 ? 0.0f : g[0];
  f[2] = module->img_param.black[0]/(float)0xffff;
  f[3] = module->img_param.white[0]/(float)0xffff;
  f[4] = module->img_param.noise_a;
  f[5] = module->img_param.noise_b;
  f[6] = g[1];
#if 0
  fprintf(stderr, "blend found noise params %g %g\n", f[4], f[5]);
  if(f[4] == 0.0f && f[5] == 0.0f)
  {
    f[4] = 800.0f;
    f[5] = 2.0f;
  }
#endif
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*7;
  return 0;
}

#if 1
void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  // TODO: only if mode param is focus stack
  dt_roi_t roif = module->connector[0].roi;
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
#endif
