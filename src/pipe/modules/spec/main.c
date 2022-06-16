#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float    *f = (float    *)module->committed_param;
  uint32_t *i = (uint32_t *)module->committed_param;

  // grab params by name:
  const float  p_exp = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("exposure")))[0];
  const float *p_wht = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("white")));
  const float  p_spl = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("split")))[0];
  const float  p_des = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("desat")))[0];
  const float  p_sat = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("sat")))[0];
  const int    p_mat = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("matrix")))[0];
  const int    p_gam = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("gamut")))[0];
  if(p_mat == 0)
  { // the one that comes with the image from the source node:
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4*i+j] = module->img_param.cam_to_rec2020[3*j+i];
  }
#if 0
  else if(p_mat == 2)
  { // CIE XYZ
    const float xyz_to_rec2020[] = {
      1.7166511880, -0.3556707838, -0.2533662814,
     -0.6666843518,  1.6164812366,  0.0157685458,
      0.0176398574, -0.0427706133,  0.9421031212};
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4*i+j] = xyz_to_rec2020[3*j+i];
  }
  else
  { // p_mat == 0 (or default) rec2020, identity matrix
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4*j+i] = i==j ? 1.0f : 0.0f;
  }
#endif
  f[12+0] = p_wht[0];
  f[12+1] = p_wht[1];
  f[12+2] = p_wht[2];
  f[12+3] = powf(2.0f, p_exp);
  f[12+4] = p_spl;
  f[12+5] = p_des;
  f[12+6] = p_sat;
  i[12+7] = p_gam;
  i[12+8] = p_mat;
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float) * 21;
  return 0;
}

