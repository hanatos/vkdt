#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void commit_params(dt_graph_t *graph, dt_node_t *node)
{
  float *f = (float *)node->module->committed_param;
  for(int k=0;k<4;k++)
    f[k] = node->module->img_param.black[k]/65535.0f;
  if(node->module->img_param.whitebalance[0] > 0 &&
     node->module->img_param.white[0] > 0)
  {
    for(int k=0;k<4;k++)
      f[4+k] = powf(2.0f, ((float*)node->module->param)[0]) *
        node->module->img_param.whitebalance[k] *
        65535.0f /
        (node->module->img_param.white[k]-node->module->img_param.black[k]);
  }
  else
    for(int k=0;k<4;k++)
      f[4+k] = powf(2.0f, ((float*)node->module->param)[0]);
  for(int k=0;k<12;k++) f[8+k] = 0.0f;
  if(node->module->img_param.cam_to_rec2020[0] > 0.0f)
  { // camera to rec2020 matrix
    // mat3 in glsl is an array of 3 vec4 column vectors:
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[8+4*j+i] = node->module->img_param.cam_to_rec2020[3*j+i];
  }
  else
  { // identity
    f[8+0] = f[8+5] = f[8+10] = 1.0f;
  }
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*20;
  return 0;
}
