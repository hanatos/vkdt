#include "modules/api.h"

// for better resampling, we request 4x the output resolution,
// if we can that is (if it's not already maxed out)
void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  float w = module->connector[1].roi.wd;
  float h = module->connector[1].roi.ht;
  module->connector[0].roi.wd = MIN(4*w, module->connector[0].roi.full_wd);
  module->connector[0].roi.ht = MIN(4*h, module->connector[0].roi.full_ht);
  module->connector[0].roi.scale = module->connector[0].roi.full_wd/w;
}
