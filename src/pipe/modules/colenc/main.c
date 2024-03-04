#include "modules/api.h"
#include <stdio.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  // const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  // if(!img_param) return;
  const int32_t p_prim = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("prim")))[0];
  const int32_t p_trc  = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("trc" )))[0];
  // these are in sync with the enums in module.h:
  module->img_param.colour_primaries = p_prim;
  module->img_param.colour_trc       = p_trc;
}
