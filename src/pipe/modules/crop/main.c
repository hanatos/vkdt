#include "modules/api.h"
#include "gaussian_elimination.h"

// TODO ROI
void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // TODO: set smooth flag on input connector only if we have any distortion parameter set
  module->connector[0].flags = s_conn_smooth;
  const float *p_crop = dt_module_param_float(module, 1);

  // TODO: consider crop/distortion
  // copy to input
  // module->connector[0].roi = module->connector[1].roi;

  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.x = 0.0f;
  module->connector[0].roi.y = 0.0f;
  return; // XXX

  // TODO: need full roi support or else rounding kills the scanline
  // float x = module->connector[1].roi.full_wd * p_crop[0];
  // float y = module->connector[1].roi.full_ht * p_crop[2];
  float w = module->connector[1].roi.full_wd / (p_crop[1] - p_crop[0]);
  float h = module->connector[1].roi.full_ht / (p_crop[3] - p_crop[2]);
  float s = module->connector[1].roi.scale;
  // XXX TODO: the pipeline does not currently really support this
  module->connector[0].roi.x = 0;//module->connector[1].roi.x + x / s;
  module->connector[0].roi.y = 0;//module->connector[1].roi.y + y / s;
  module->connector[0].roi.wd = w / s;
  module->connector[0].roi.ht = h / s;
  module->connector[0].roi.scale = s;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const float *p_crop = dt_module_param_float(module, 1);
  // copy to output
  // TODO: consider distortion!
  module->connector[1].roi = module->connector[0].roi;
  module->connector[1].roi.full_wd = module->connector[0].roi.full_wd * (p_crop[1] - p_crop[0]);
  module->connector[1].roi.full_ht = module->connector[0].roi.full_ht * (p_crop[3] - p_crop[2]);
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

  // XXX padding + column major!
  for(int i=0;i<9;i++)
    ((float*)module->committed_param)[i] = r[i];
  const float *p_crop = dt_module_param_float(module, 1);
  for(int i=0;i<4;i++)
    ((float*)module->committed_param)[9+i] = p_crop[i];
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*13;
  return 0;
}
