#include "modules/api.h"

void
modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[1].roi.full_wd = module->connector[0].roi.full_wd;
  module->connector[1].roi.full_ht = module->connector[0].roi.full_ht;
  module->img_param.colour_trc       = s_colour_trc_srgb;
  module->img_param.colour_primaries = s_colour_primaries_srgb;
  module->img_param.cam_to_rec2020[0] = 0.0/0.0;
}
