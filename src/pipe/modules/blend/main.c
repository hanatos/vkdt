#include "modules/api.h"
#include <stdio.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  float *g = (float*)module->param;
  f[0] = g[0];
  f[1] = g[1];
  f[2] = module->img_param.black[0]/(float)0xffff;
  f[3] = module->img_param.white[0]/(float)0xffff;
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*4;
  return 0;
}
