#include "modules/api.h"
#include <math.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const float *p_border = dt_module_param_float(module, 2);
  const float bx = CLAMP(p_border[0], 0.0, 1.0);
  const float by = CLAMP(p_border[1], 0.0, 1.0);
  module->connector[0].roi.wd = module->connector[1].roi.wd / (1.0 + bx);
  module->connector[0].roi.ht = module->connector[1].roi.ht - module->connector[0].roi.wd * by;
  module->connector[0].roi.scale = module->connector[1].roi.scale;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const float *p_border = dt_module_param_float(module, 2);
  const float bx = CLAMP(p_border[0], 0.0, 1.0);
  const float by = CLAMP(p_border[1], 0.0, 1.0);
  module->connector[1].roi.full_wd = module->connector[0].roi.full_wd * (1.0 + bx);
  module->connector[1].roi.full_ht = module->connector[0].roi.full_ht + module->connector[0].roi.full_wd * by;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 2)
  { // border size
    return s_graph_run_all; // output image size changed
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}
