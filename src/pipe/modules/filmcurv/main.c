#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[1].roi = module->connector[0].roi;
  module->connector[2].roi.full_wd = 300;
  module->connector[2].roi.full_ht = 300;
  module->connector[2].roi.wd = 300; // for the unconnected case, init something
  module->connector[2].roi.ht = 300;
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  float *p = (float *)module->param;
  // copy x and y
  for(int k=0;k<8;k++) f[k] = p[k];
  // init tangent
  const int n=4;
  float m[5] = {0.0f}, d[5];
  for(int i=0;i<n-1;i++)
    d[i] = (p[i+5] - p[i+4])/(p[i+1] - p[i]);
  d[n-1] = d[n-2];
  for(int i=1;i<n-1;i++)
    if(d[i-1]*d[i] <= 0.0f)
      m[i] = 0.0f;
    else
      m[i] = (d[i-1] + d[i])*.5f;
  // for extrapolation: keep curvature constant
  m[n-1] = fmaxf(m[n-2] + d[n-2] - d[n-3], 0.0f);
  m[0]   = fmaxf(m[1] + d[0] - d[1], 0.0f);

  // monotone hermite clamping:
  for(int i=0;i<n;i++)
  {
    if(fabsf(d[i]) <= 1e-8f)
      m[i] = m[i+1] = 0.0f;
    else
    {
      const float alpha = m[i]   / d[i];
      const float beta  = m[i+1] / d[i];
      const float tau   = alpha * alpha + beta * beta;
      if(tau > 9.0f)
      {
        m[i]   = 3.0f * m[i]   / sqrtf(tau);
        m[i+1] = 3.0f * m[i+1] / sqrtf(tau);
      }
    }
  }
  for(int k=0;k<4;k++) f[8+k] = m[k];
  f[12] = p[8];
  f[13] = p[9];
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*14;
  return 0;
}
