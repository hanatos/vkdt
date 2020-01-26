#include "modules/api.h"

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // request the full thing, we want the borders
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.x = 0.0f;
  module->connector[0].roi.y = 0.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // copy to output
  const uint32_t *b = module->img_param.crop_aabb;
  module->connector[1].roi = module->connector[0].roi;
  module->connector[1].roi.full_wd = b[2] - b[0];
  module->connector[1].roi.full_ht = b[3] - b[1];
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  uint32_t *i = (uint32_t *)module->committed_param;
  const uint32_t *b = module->img_param.crop_aabb;
  for(int k=0;k<4;k++) i[k] = b[k];
  float *f = (float *)module->committed_param;
  for(int k=0;k<4;k++)
    f[k+4] = module->img_param.black[k]/65535.0f;
  for(int k=0;k<4;k++)
    f[k+8] = module->img_param.white[k]/65535.0f;
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(uint32_t)*12;
  return 0;
}
