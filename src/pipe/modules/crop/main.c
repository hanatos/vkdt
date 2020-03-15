#include "modules/api.h"
#include "gaussian_elimination.h"

#include <math.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const float *p_crop = dt_module_param_float(module, 1);

  // copy to input
  float wd = p_crop[1] - p_crop[0];
  float ht = p_crop[3] - p_crop[2];
  module->connector[0].roi.wd = module->connector[1].roi.wd / wd;
  module->connector[0].roi.ht = module->connector[1].roi.ht / ht;
  module->connector[0].roi.x = 0.0f;
  module->connector[0].roi.y = 0.0f;
  module->connector[0].roi.scale = module->connector[1].roi.scale;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const float *p_crop = dt_module_param_float(module, 1);
  // copy to output
  module->connector[1].roi = module->connector[0].roi;

  float wd = p_crop[1] - p_crop[0];
  float ht = p_crop[3] - p_crop[2];
  module->connector[1].roi.full_wd = module->connector[0].roi.full_wd * wd;
  module->connector[1].roi.full_ht = module->connector[0].roi.full_ht * ht;
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  // see:
  // pages 17-21 of Fundamentals of Texture Mapping and Image Warping, Paul Heckbert,
  // Master’s thesis, UCB/CSD 89/516, CS Division, U.C. Berkeley, June 1989
  // we have given:
  // a set of four points in screen space defining what should be a flat quad.
  const float *inp = dt_module_param_float(module, 0);
  float p[8];
  for(int k=0;k<4;k++)
  {
    p[2*k+0] = module->connector[0].roi.full_wd * inp[2*k+0];
    p[2*k+1] = module->connector[0].roi.full_ht * inp[2*k+1];
  }
  // the approach taken here is that a 2D point is transformed by a matrix
  // H * (x, y, 1)^t
  // and then de-homogenised by dividing out the z coordinate (projection matrix).
  // these points are our given projection points p1..p3. we further constrain the
  // quad we are looking for such that the corners are (a, b), (A, b), (A, B), (a, B)
  const float a = p[0], A = p[2], b = p[1], B = p[7];
  const float u[] = {a, b, A, b, A, B, a, B};

  // this results in the following set of equations:
  double M[] = {
    u[0], u[1], 1, 0, 0, 0, -p[0]*u[0], -p[0]*u[1],
    u[2], u[3], 1, 0, 0, 0, -p[2]*u[2], -p[2]*u[3],
    u[4], u[5], 1, 0, 0, 0, -p[4]*u[4], -p[4]*u[5],
    u[6], u[7], 1, 0, 0, 0, -p[6]*u[6], -p[6]*u[7],
    0, 0, 0, u[0], u[1], 1, -p[1]*u[0], -p[1]*u[1],
    0, 0, 0, u[2], u[3], 1, -p[3]*u[2], -p[3]*u[3],
    0, 0, 0, u[4], u[5], 1, -p[5]*u[4], -p[5]*u[5],
    0, 0, 0, u[6], u[7], 1, -p[7]*u[6], -p[7]*u[7],
  };
  double r[] = {p[0], p[2], p[4], p[6], p[1], p[3], p[5], p[7], 1.0};
  gauss_solve(M, r, 8);

  // padding + column major:
  float *f = (float*)module->committed_param;
  f[ 0] = r[0]; f[ 1] = r[3]; f[ 2] = r[6]; f[ 3] = 0.0f;
  f[ 4] = r[1]; f[ 5] = r[4]; f[ 6] = r[7]; f[ 7] = 0.0f;
  f[ 8] = r[2]; f[ 9] = r[5]; f[10] = r[8]; f[11] = 0.0f;
  const float *p_rot = dt_module_param_float(module, 2);
  f += 12;
  float rad = p_rot[0] * 3.1415629 / 180.0f;
  f[0] =  cosf(rad); f[1] = sinf(rad);
  f[2] = -sinf(rad); f[3] = cosf(rad);
  f += 4;
  const float *p_crop = dt_module_param_float(module, 1);
  int flip = 0;
  // if(p_rot[0] >  45 && p_rot[0] < 135) flip = 1;
  // if(p_rot[0] > 225 && p_rot[0] < 315) flip = 1;
  if(flip) for(int i=0;i<2;i++) f[2*i] = p_crop[2*(1-i)];
  else     for(int i=0;i<2;i++) f[2*i] = p_crop[2*i];
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*20;
  return 0;
}
