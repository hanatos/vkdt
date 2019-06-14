#include "module.h"
#include <math.h>
#include <stdlib.h>

// XXX this is a major fuckup.
// XXX we need:
// * so->params (ui)
// * copy thereof on module (or at least the values) so the sliders can change them
// * representation for gpu uniform buffer
// XXX we also need:
// * a place to execute commit_params on module level and only when needed
void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  for(int k=0;k<4;k++)
    f[k] = module->img_param.black[k];
  for(int k=0;k<4;k++)
    f[4+k] = powf(2.0f, ((float*)module->param)[0]) *
       module->img_param.whitebalance[k] /
      (module->img_param.white[k]-module->img_param.black[k]);
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*8;
  mod->committed_param = malloc(mod->committed_param_size);
  return 0;
}

void cleanup(dt_module_t *mod)
{
  free(mod->committed_param);
}
