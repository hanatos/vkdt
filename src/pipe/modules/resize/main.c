#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

// request everything
void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // request the full thing, we'll rescale
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.x = 0.0f;
  module->connector[0].roi.y = 0.0f;
  module->connector[0].roi.scale = 1.0f;
}

#if 0
// XXX do we need this? i think only for magnification
void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // copy to output
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  assert(img_param);
  const uint32_t *b = img_param->crop_aabb;
  module->connector[1].roi = module->connector[0].roi;
  module->connector[1].roi.full_wd = b[2] - b[0];
  module->connector[1].roi.full_ht = b[3] - b[1];
}
#endif

#if 0 // TODO: pass through if nothing is happening
void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
}
#endif

