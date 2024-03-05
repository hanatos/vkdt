#include "modules/api.h"
#include <stdio.h>

void modify_roi_out(dt_graph_t *graph, dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // must have disconnected input somewhere
  int *p_prim = (int*)dt_module_param_int(module, dt_module_get_param(module->so, dt_token("prim")));
  int *p_trc  = (int*)dt_module_param_int(module, dt_module_get_param(module->so, dt_token("trc" )));
  // don't overwrite user choice, but if they don't know/leave at defaults we do:
  if(*p_prim == dt_colour_primaries_unknown) *p_prim = img_param->colour_primaries;
  if(*p_trc  == dt_colour_trc_unknown)       *p_trc  = img_param->colour_trc;
  // after this module we'll be in working space:
  module->img_param.colour_primaries = dt_colour_primaries_2020;
  module->img_param.colour_trc       = dt_colour_trc_linear;
  module->connector[1].roi.full_wd = module->connector[0].roi.full_wd;
  module->connector[1].roi.full_ht = module->connector[0].roi.full_ht;
}
