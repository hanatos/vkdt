#include "modules/api.h"
#include <stdlib.h>

typedef struct moddata_t
{
  int id_guided;
}
moddata_t;

int init(dt_module_t *mod)
{
  moddata_t *dat = malloc(sizeof(*dat));
  mod->data = dat;
  memset(dat, 0, sizeof(*dat));
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  free(mod->data);
  mod->data = 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pid_rad = dt_module_get_param(module->so, dt_token("radius"));
  if(parid == pid_rad)
  { // guided filter blur radius
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(module, parid)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void commit_params(dt_graph_t *graph, dt_module_t *mod)
{ // let guided filter know we updated the push constants:
  moddata_t *dat = mod->data;
  const float *radius = dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("radius")));
  if(dat->id_guided >= 0 && dat->id_guided < graph->num_nodes &&
      graph->node[dat->id_guided].kernel == dt_token("guided2f"))
    memcpy(graph->node[dat->id_guided].push_constant, radius, sizeof(float)*2);
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const int id_depth = dt_node_add(graph, module, "dehaze", "depth", wd, ht, 1, 0, 0, 2,
      "input", "read",  "rgba", "*",    dt_no_roi,
      "depth", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id_depth, 0);
  const float *radius = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("radius")));
  // comes right after:
  // const float edges  = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("epsilon")))[0];
  moddata_t *dat = module->data;
  const int id_guided = dt_api_guided_filter_full(graph, module,
      -1, 0,       // input, determines buffer sizes
      id_depth, 1, // guide
      0, 0, &dat->id_guided,
      radius, 0);
  const int id_dehaze = dt_node_add(graph, module, "dehaze", "dehaze", wd, ht, 1, 0, 0, 3,
      "input",  "read",  "rgba", "*",    dt_no_roi,
      "depth",  "read",  "rgba", "*",    dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 2, id_guided, 2);
  CONN(dt_node_connect_named(graph, id_guided, "output", id_dehaze, "depth"));
  dt_connector_copy(graph, module, 0, id_dehaze, 0);
  dt_connector_copy(graph, module, 1, id_dehaze, 2);
}
