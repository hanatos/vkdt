#include "modules/api.h"
#include "gaussian_elimination.h"

#include <math.h>

static inline int
processed_point_outside(
    const int    wd, // input roi
    const int    ht,
    const float *T,
    const float *H,
    const float *crop,
    int corner)
{
  float xy[2] = {
    corner & 1 ? (crop[1]-crop[0])*wd : crop[0]*wd,
    corner & 2 ? (crop[3]-crop[2])*ht : crop[2]*ht};
  xy[0] -= wd/2.0;
  xy[1] -= ht/2.0;
  float tmp[3] = {0.0f};
  for(int i=0;i<2;i++)
    for(int j=0;j<2;j++)
      tmp[i] += T[2*j+i] * xy[j];
  tmp[0] += wd/2.0;
  tmp[1] += ht/2.0;
  tmp[2] = 1.0f;
  float tmp2[3] = {0.0f};
  for(int i=0;i<3;i++)
    for(int j=0;j<3;j++)
      tmp2[i] += H[4*j+i] * tmp[j];
  tmp2[0] /= tmp2[2];
  tmp2[1] /= tmp2[2];

  if(tmp2[0] >= wd) return 1;
  if(tmp2[1] >= ht) return 1;
  if(tmp2[0] < 0)   return 1;
  if(tmp2[1] < 0)   return 1;
  return 0;
}

void ui_callback(
    dt_module_t *module,
    dt_token_t   param)
{ // auto-crop away black borders
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  float H[16], T[4], crop[4];
  float *f = (float*)module->committed_param;
  for(int k=0;k<12;k++) H[k] = f[k];   // perspective matrix H
  f += 12;
  for(int k=0;k<4;k++) T[k] = f[k];    // rotation matrix T
  f += 4;
  for(int k=0;k<4;k++) crop[k] = f[k]; // crop window
  float xy[4][2];
  for(int c=0;c<4;c++)
  { // create corners: (0,0)..(wd, ht)
    xy[c][0] = c & 1 ? wd : 0;
    xy[c][1] = c & 2 ? ht : 0;
  }

  float crop2[4], scale0 = 0.1f, scale1 = 1.0f;
  for(int i=0;i<20;i++)
  {
    float scale = (scale0 + scale1)/2.0f;
    for(int k=0;k<2;k++)
    { // aspect ratio preserving scale of crop window
      float cn[2] = {wd * crop[k & 1 ? 1 : 0], ht * crop[k & 1 ? 3 : 2]};
      cn[0] = 0.5f * wd + scale * (cn[0] - 0.5f * wd); // is this a good center?
      cn[1] = 0.5f * ht + scale * (cn[1] - 0.5f * ht);
      crop2[k & 1 ? 1 : 0] = cn[0] / wd;
      crop2[k & 1 ? 3 : 2] = cn[1] / ht;
    }
    int out = 0;
    for(int c=0;c<4;c++)
      if((out |= processed_point_outside(wd, ht, T, H, crop2, c)))
        break;
    if(out) scale1 = scale;
    else    scale0 = scale;
  }
  float *p_crop = (float *)dt_module_param_float(module, 1);
  p_crop[0] = 1.01 * crop2[0];
  p_crop[1] = 0.99 * crop2[1];
  p_crop[2] = 1.01 * crop2[2];
  p_crop[3] = 0.99 * crop2[3];
}

// fill crop and rotation if auto-rotate by exif data has been requested
static inline void
get_crop_rot(uint32_t or, float wd, float ht, const float *p_crop, const float *p_rot, float *crop, float *rot)
{
  // flip by exif orientation if we have it and it's requested:
  float rotation = p_rot[0];
  // at least do nothing 
  rot[0] = rotation;
  crop[0] = p_crop[0];
  crop[1] = p_crop[1];
  crop[2] = p_crop[2];
  crop[3] = p_crop[3];
  if(rotation == 1337.0f)
  { // auto rotation magic number
    if(or == 3)      rot[0] = 180.0f;
    else if(or == 8) rot[0] = 90.0f;
    else if(or == 6) rot[0] = 270.0f;
    else             rot[0] = 0.0f;
  }
  if(crop[0] == 1.0 && crop[1] == 3.0 && crop[2] == 3.0 && crop[3] == 7.0)
  { // more magic: microcrop by pixel safety margin for resampling:
    float crw = 3.0 / wd, crh = 3.0 / ht;
    if(rot[0] >= 45 && rot[0] < 135)
    { // almost 90
      crop[0] = 0.5f - (.5f - crh) * ht / wd;
      crop[2] = 0.5f - (.5f - crw) * wd / ht;
      crop[1] = 0.5f + (.5f - crh) * ht / wd;
      crop[3] = 0.5f + (.5f - crw) * wd / ht;
    }
    else if(rot[0] < 225)
    { // almost 180
      crop[0] = crw;
      crop[2] = crh;
      crop[1] = 1.0f-crw;
      crop[3] = 1.0f-crh;
    }
    else if(rot[0] < 315)
    { // almost 270
      crop[0] = 0.5f - (.5f - crh) * ht / wd;
      crop[2] = 0.5f - (.5f - crw) * wd / ht;
      crop[1] = 0.5f + (.5f - crh) * ht / wd;
      crop[3] = 0.5f + (.5f - crw) * wd / ht;
    }
    else
    { // almost 0
      crop[0] = crw;
      crop[2] = crh;
      crop[1] = 1.0f-crw;
      crop[3] = 1.0f-crh;
    }
  }
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  float crop[4], rot;
  const float *p_crop = dt_module_param_float(module, 1);
  const float *p_rot  = dt_module_param_float(module, 2);
  float w = module->connector[0].roi.full_wd;
  float h = module->connector[0].roi.full_ht;
  uint32_t or = module->img_param.orientation;
  get_crop_rot(or, w, h, p_crop, p_rot, crop, &rot);

  // copy to input
  float wd = crop[1] - crop[0];
  float ht = crop[3] - crop[2];
  module->connector[0].roi.wd = module->connector[1].roi.wd / wd;
  module->connector[0].roi.ht = module->connector[1].roi.ht / ht;
  module->connector[0].roi.scale = module->connector[1].roi.scale;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  float crop[4], rot;
  const float *p_crop = dt_module_param_float(module, 1);
  const float *p_rot  = dt_module_param_float(module, 2);
  float w = module->connector[0].roi.full_wd;
  float h = module->connector[0].roi.full_ht;
  uint32_t or = module->img_param.orientation;
  get_crop_rot(or, w, h, p_crop, p_rot, crop, &rot);
  // copy to output
  module->connector[1].roi = module->connector[0].roi;

  float wd = crop[1] - crop[0];
  float ht = crop[3] - crop[2];
  module->connector[1].roi.full_wd = module->connector[0].roi.full_wd * wd;
  module->connector[1].roi.full_ht = module->connector[0].roi.full_ht * ht;
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  // perspective correction. see:
  // pages 17-21 of Fundamentals of Texture Mapping and Image Warping, Paul Heckbert,
  // Master’s thesis, UCB/CSD 89/516, CS Division, U.C. Berkeley, June 1989
  // we have given:
  // a set of four points in screen space defining what should be a flat quad.
  const float *inp = dt_module_param_float(module, 0);
  float p[8];
  for(int k=0;k<4;k++)
  {
    p[2*k+0] = module->connector[0].roi.wd * inp[2*k+0];
    p[2*k+1] = module->connector[0].roi.ht * inp[2*k+1];
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
  f += 12;

  float crop[4], rot;
  const float *p_crop = dt_module_param_float(module, 1);
  const float *p_rot  = dt_module_param_float(module, 2);
  float wd = module->connector[0].roi.wd;
  float ht = module->connector[0].roi.ht;
  uint32_t or = module->img_param.orientation;
  get_crop_rot(or, wd, ht, p_crop, p_rot, crop, &rot);

  // rotation angle
  float rad = rot * 3.1415629 / 180.0f;
  f[0] =  cosf(rad); f[1] = sinf(rad);
  f[2] = -sinf(rad); f[3] = cosf(rad);
  f += 4;
  // crop window
  f[0] = crop[0];
  f[1] = crop[1];
  f[2] = crop[2];
  f[3] = crop[3];

  // and now write back actual parameters in case we were in auto-rotate mode
  if(p_rot[0] == 1337.0f)
  {
    dt_module_set_param_float_n(module, dt_token("crop"), crop, 4);
    dt_module_set_param_float(module, dt_token("rotate"), rot);
  }
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*20;
  return 0;
}
