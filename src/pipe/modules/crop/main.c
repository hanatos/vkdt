#include "modules/api.h"
#include "core/gaussian_elimination.h"

#include <math.h>
#include <float.h>

static inline int
processed_point_outside(
    const int    wd, // input roi
    const int    ht,
    const float *T,
    const float *H,
    const float *crop,
    const float *x) // position in pixels
{
  float xy[2] = { x[0], x[1] };
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

static inline float
line_search(
    const int    wd,  // input roi
    const int    ht,
    const float *T,
    const float *H,
    const float *crop,
    const float *x,   // point in pixel coordinates, on the inside
    const float *y)   // the other point where to aim at (but don't go farther)
{ // ray trace from the inside until intersection with the boundary, return point that is still inside
  float m = 0.0f, M = 1.0f;
  if(!processed_point_outside(wd, ht, T, H, crop, y)) return 1.0f; // early out
  for(int it=0;it<10;it++)
  { // binary search for a bit
    float t = (M+m)/2.0f;
    float z[2];
    for(int k=0;k<2;k++) z[k] = (1.0f-t)*x[k] + t*y[k];
    if(processed_point_outside(wd, ht, T, H, crop, z))
      M = t;
    else
      m = t;
  }
  return m; // be conservative, return the inside distance
}

void ui_callback(
    dt_module_t *module,
    dt_token_t   param)
{ // auto-crop away black borders
  // really this code is shit. it can fail and it is hardly ever optimal.
  // there are a couple of publications on finding the maximum inscribed axis aligned rectangle
  // of a convex polygon, but starting from a (sorted) list of vertices adn sounds like too much trouble.
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  float H[16], T[4], crop[4] = {0, 1, 0, 1};
  float *f = (float*)module->committed_param;
  for(int k=0;k<12;k++) H[k] = f[k];   // perspective matrix H
  f += 12;
  for(int k=0;k<4;k++) T[k] = f[k];    // rotation matrix T

  float A_max = 0.0f, aabb_max[4];
  // 1) ray trace to obtain a list of <= 12 vertices
  float center[2] = {wd/2.0, ht/2.0};
  float aabb[] = {FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX}; // xmin ymin xmax ymax
  float edgep[4][2]; // xmin ymin xmax ymax
  for(int e=0;e<4;e++)
  { // trace 4-star outside to find coarse aabb
    const float dir[4][2] = {{-1,0},{0,-1},{1,0},{0,1}};
    float y[] = {
      2.0*wd*dir[e][0] + center[0],
      2.0*wd*dir[e][1] + center[1]};
    float t = line_search(wd, ht, T, H, crop, center, y);
    edgep[e][0] = center[0] + t*2.0*wd*dir[e][0];
    edgep[e][1] = center[1] + t*2.0*wd*dir[e][1];
    aabb[0] = MIN(edgep[e][0], aabb[0]);
    aabb[1] = MIN(edgep[e][1], aabb[1]);
    aabb[2] = MAX(edgep[e][0], aabb[2]);
    aabb[3] = MAX(edgep[e][1], aabb[3]);
  }
  int all_good = 1;
  for(int c=0;c<4;c++)
  { // check if all four corners are inside and if so return this immediately (fast path for axis aligned orientation flips)
    float y[] = { aabb[2*(c&1)], aabb[1+(c&2)] };
    if(processed_point_outside(wd, ht, T, H, crop, y)) all_good = 0;
  }
  if(all_good)
  {
    memcpy(aabb_max, aabb, sizeof(aabb_max));
    goto out;
  }
  float xy[4][12][2], xyc[4][3][2];
  int nxy[4] = {0}, nxyc[4] = {0};
  for(int c=0;c<4;c++)
  { // trace 3 rays towards corners of this aabb (from two edges + center)
    float y[] = { aabb[2*(c&1)], aabb[1+(c&2)] }; // trace towards corner c
    int xl = (c&1) ? 2 : 0, yl = (c&2) ? 3 : 1;
    for(int k=0;k<3;k++)
    { // record results in the lists to be considered for xmax/min and ymax/min:
      float *x = k == 0 ? center : (k == 1 ? edgep[xl] : edgep[yl]);
      float t = line_search(wd, ht, T, H, crop, x, y);
      int i = nxy[xl]++, j = nxy[yl]++, l = nxyc[c]++;
      xyc[c][l][0] = xy[yl][j][0] = xy[xl][i][0] = (1.0f - t) * x[0] + t * y[0];
      xyc[c][l][1] = xy[yl][j][1] = xy[xl][i][1] = (1.0f - t) * x[1] + t * y[1];
    }
  }

  // 2) pick one of the <=12 as starting vertex
  for(int c=0;c<4;c++) for(int k=0;k<nxyc[c];k++)
  { // select fixed corner for first two aabb dimensions
    const int xm = c&1, ym = (c&2)>>1; // xmin(0) or max(1), ymin(0) or max(1)
    float aabb[4];
    aabb[0+2*xm] = xyc[c][k][0];
    aabb[1+2*ym] = xyc[c][k][1];
    for(int i=0;i<nxy[2*(1-xm)];i++)
    { // find other x coordinate
      float x = xy[2*(1-xm)][i][0], y = xy[2*(1-xm)][i][1];
      if( xm && x >= aabb[2]) continue; // candidate x is on the wrong side of what we already have
      if(!xm && x <= aabb[0]) continue;
      if( ym && y <= aabb[3]) continue; // y needs to be outside
      if(!ym && y >= aabb[1]) continue;
      aabb[2*(1-xm)] = x;
      for(int j=0;j<nxy[1+2*(1-ym)];j++)
      { // find last y coordinate
        float x = xy[1+2*(1-ym)][j][0], y = xy[1+2*(1-ym)][j][1];
        if(x >= aabb[0] && x <= aabb[2]) continue; // x needs to be outside
        if( ym && y >= aabb[3]) continue;
        if(!ym && y <= aabb[1]) continue;
        aabb[1+2*(1-ym)] = y;
        int cc = 0;
          for(;cc<4;cc++)
          {
            float y[] = { aabb[2*(cc&1)], aabb[1+(cc&2)] };
            if(processed_point_outside(wd, ht, T, H, crop, y)) break;
          }
          if(cc < 4) continue;
        float A = (aabb[2]-aabb[0])*(aabb[3]-aabb[1]);
        if(A > A_max)
        {
          A_max = A;
          memcpy(aabb_max, aabb, sizeof(aabb_max));
        }
      }
    }
  }
  // fallback if all else failed:
  if(A_max <= 0.0f) memcpy(aabb, aabb_max, sizeof(aabb_max));
out:;
  float *p_crop = (float *)dt_module_param_float(module, 1);
  p_crop[0] = 1.01 * aabb_max[0] / wd;
  p_crop[1] = 0.99 * aabb_max[2] / wd;
  p_crop[2] = 1.01 * aabb_max[1] / ht;
  p_crop[3] = 0.99 * aabb_max[3] / ht;
}

// fill crop and rotation if auto-rotate by exif data has been requested
static inline void
get_crop_rot(uint32_t or, double wd, double ht,
    const float *p_crop, const float *p_rot, float *crop, float *rot)
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
    double crw = wd > 400 ? 3.0 / wd : 0.0, crh = ht > 400 ? 3.0 / ht : 0.0;
    if(rot[0] >= 45 && rot[0] < 135)
    { // almost 90
      crop[0] = 0.5 - (.5 - crh) * ht / wd;
      crop[2] = 0.5 - (.5 - crw) * wd / ht;
      crop[1] = 0.5 + (.5 - crh) * ht / wd;
      crop[3] = 0.5 + (.5 - crw) * wd / ht;
    }
    else if(rot[0] < 225)
    { // almost 180
      crop[0] = crw;
      crop[2] = crh;
      crop[1] = 1.0-crw;
      crop[3] = 1.0-crh;
    }
    else if(rot[0] < 315)
    { // almost 270
      crop[0] = 0.5 - (.5 - crh) * ht / wd;
      crop[2] = 0.5 - (.5 - crw) * wd / ht;
      crop[1] = 0.5 + (.5 - crh) * ht / wd;
      crop[3] = 0.5 + (.5 - crw) * wd / ht;
    }
    else
    { // almost 0
      crop[0] = crw;
      crop[2] = crh;
      crop[1] = 1.0-crw;
      crop[3] = 1.0-crh;
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

  float wd = crop[1] - crop[0];
  float ht = crop[3] - crop[2];

  if(module->connector[1].roi.full_wd == module->connector[1].roi.wd)
  { // keep pixel accuracy
    module->connector[0].roi.wd = module->connector[0].roi.full_wd;
    module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  }
  else
  {
    module->connector[0].roi.wd = module->connector[1].roi.wd / wd;
    module->connector[0].roi.ht = module->connector[1].roi.ht / ht;
  }
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
  // clip to typical max vk frame buffer dimensions
  module->connector[1].roi.full_wd = MIN(32768, module->connector[0].roi.full_wd * wd);
  module->connector[1].roi.full_ht = MIN(32768, module->connector[0].roi.full_ht * ht);
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
