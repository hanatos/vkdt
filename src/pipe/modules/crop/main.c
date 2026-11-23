#include "modules/api.h"
#include "core/gaussian_elimination.h"

#include <math.h>

#define CROP_NUM_HP 9

// safe output region as half planes hp[i] * (x, y, 1) <= 0.
static inline void
get_half_planes(
    const int     wd,
    const int     ht,
    const float  *T,
    const float  *H,
    const double  inset,
    double        hp[CROP_NUM_HP][3])
{
  const double cx = wd/2.0, cy = ht/2.0;
  const double A[6] = {
    T[0], T[2], cx - (T[0]*cx + T[2]*cy),
    T[1], T[3], cy - (T[1]*cx + T[3]*cy) };
  const double R[3][3] = {
    { H[0], H[4], H[ 8] },
    { H[1], H[5], H[ 9] },
    { H[2], H[6], H[10] } };
  double Q[3][3];
  for(int i=0;i<3;i++)
  {
    Q[i][0] = R[i][0]*A[0] + R[i][1]*A[3];
    Q[i][1] = R[i][0]*A[1] + R[i][1]*A[4];
    Q[i][2] = R[i][0]*A[2] + R[i][1]*A[5] + R[i][2];
  }
  const double w = Q[2][0]*cx + Q[2][1]*cy + Q[2][2];
  if(w < 0.0) for(int i=0;i<3;i++) for(int k=0;k<3;k++) Q[i][k] = -Q[i][k];
  for(int k=0;k<3;k++)
  {
    hp[0][k] = -Q[0][k] +       inset  * Q[2][k];
    hp[1][k] =  Q[0][k] - (wd - inset) * Q[2][k];
    hp[2][k] = -Q[1][k] +       inset  * Q[2][k];
    hp[3][k] =  Q[1][k] - (ht - inset) * Q[2][k];
    hp[4][k] = -Q[2][k];
  }
  hp[4][2] += 1e-3 * fabs(w);
  const double K = MAX(wd, ht);
  hp[5][0] = -1; hp[5][1] =  0; hp[5][2] = -K;
  hp[6][0] =  1; hp[6][1] =  0; hp[6][2] = -(wd + K);
  hp[7][0] =  0; hp[7][1] = -1; hp[7][2] = -K;
  hp[8][0] =  0; hp[8][1] =  1; hp[8][2] = -(ht + K);
  for(int i=0;i<CROP_NUM_HP;i++)
  {
    const double n = hypot(hp[i][0], hp[i][1]);
    const double s = n > 0.0 ? 1.0/n : (hp[i][2] != 0.0 ? 1.0/fabs(hp[i][2]) : 1.0);
    for(int k=0;k<3;k++) hp[i][k] *= s;
  }
}

static inline int
get_bounds(const double hp[CROP_NUM_HP][3], double *aabb)
{
  aabb[0] = aabb[1] = INFINITY;
  aabb[2] = aabb[3] = -INFINITY;
  for(int i=0;i<CROP_NUM_HP;i++) for(int j=i+1;j<CROP_NUM_HP;j++)
  {
    const double det = hp[i][0]*hp[j][1] - hp[i][1]*hp[j][0];
    const double scale = hypot(hp[i][0], hp[i][1]) * hypot(hp[j][0], hp[j][1]);
    if(fabs(det) <= 1e-12 * scale) continue;
    const double x = (hp[i][1]*hp[j][2] - hp[i][2]*hp[j][1])/det;
    const double y = (hp[i][2]*hp[j][0] - hp[i][0]*hp[j][2])/det;
    int inside = 1;
    for(int k=0;k<CROP_NUM_HP;k++)
      if(hp[k][0]*x + hp[k][1]*y + hp[k][2] > 1e-6) { inside = 0; break; }
    if(!inside) continue;
    aabb[0] = MIN(aabb[0], x); aabb[1] = MIN(aabb[1], y);
    aabb[2] = MAX(aabb[2], x); aabb[3] = MAX(aabb[3], y);
  }
  return isfinite(aabb[0]) && isfinite(aabb[1]) &&
    isfinite(aabb[2]) && isfinite(aabb[3]) && aabb[2] > aabb[0] && aabb[3] > aabb[1];
}

static inline int
fits(const double row[CROP_NUM_HP][3], const double hp[CROP_NUM_HP][3],
     const double s, const double cx, const double cy)
{
  for(int n=0;n<CROP_NUM_HP;n++)
    if(row[n][0]*cx + row[n][1]*cy + row[n][2]*s + hp[n][2] > 1e-6) return 0;
  return 1;
}

static inline void
nearest(const double row[CROP_NUM_HP][3], const double hp[CROP_NUM_HP][3],
        const double s, const double *target, double *cx, double *cy)
{
  if(fits(row, hp, s, target[0], target[1])) { *cx = target[0]; *cy = target[1]; return; }
  double best = (*cx-target[0])*(*cx-target[0]) + (*cy-target[1])*(*cy-target[1]);
  for(int i=0;i<CROP_NUM_HP;i++)
  {
    const double g2 = row[i][0]*row[i][0] + row[i][1]*row[i][1];
    if(g2 < 1e-12) continue;
    const double r = row[i][0]*target[0] + row[i][1]*target[1] + row[i][2]*s + hp[i][2];
    const double x = target[0] - row[i][0]*r/g2, y = target[1] - row[i][1]*r/g2;
    const double d = (x-target[0])*(x-target[0]) + (y-target[1])*(y-target[1]);
    if(d < best && fits(row, hp, s, x, y)) { best = d; *cx = x; *cy = y; }
  }
  for(int i=0;i<CROP_NUM_HP;i++) for(int j=i+1;j<CROP_NUM_HP;j++)
  {
    const double det = row[i][0]*row[j][1] - row[i][1]*row[j][0];
    if(fabs(det) < 1e-9) continue;
    const double ri = -(row[i][2]*s + hp[i][2]), rj = -(row[j][2]*s + hp[j][2]);
    const double x = (ri*row[j][1] - row[i][1]*rj)/det;
    const double y = (row[i][0]*rj - ri*row[j][0])/det;
    const double d = (x-target[0])*(x-target[0]) + (y-target[1])*(y-target[1]);
    if(d < best && fits(row, hp, s, x, y)) { best = d; *cx = x; *cy = y; }
  }
}

static inline double
inscribe(
    const double  hp[CROP_NUM_HP][3],
    const double  aw,
    const double  ah,
    const double *target,
    double       *aabb)
{
  double best = 0.0, bcx = 0.0, bcy = 0.0;
  double row[CROP_NUM_HP][3];
  for(int i=0;i<CROP_NUM_HP;i++)
  {
    row[i][0] = hp[i][0];
    row[i][1] = hp[i][1];
    row[i][2] = 0.5*(fabs(hp[i][0])*aw + fabs(hp[i][1])*ah);
  }
  for(int i=0;i<CROP_NUM_HP;i++) for(int j=i+1;j<CROP_NUM_HP;j++) for(int k=j+1;k<CROP_NUM_HP;k++)
  {
    const double *a = row[i], *b = row[j], *c = row[k];
    const double det =
      a[0]*(b[1]*c[2] - b[2]*c[1]) -
      a[1]*(b[0]*c[2] - b[2]*c[0]) +
      a[2]*(b[0]*c[1] - b[1]*c[0]);
    const double scale = hypot(a[0], hypot(a[1], a[2])) *
      hypot(b[0], hypot(b[1], b[2])) * hypot(c[0], hypot(c[1], c[2]));
    if(fabs(det) <= 1e-12 * scale) continue;
    double A[9] = {a[0],a[1],a[2], b[0],b[1],b[2], c[0],c[1],c[2]};
    double x[3] = {-hp[i][2], -hp[j][2], -hp[k][2]};
    if(!gauss_solve(A, x, 3)) continue;
    if(!(x[2] > best)) continue;
    if(!fits(row, hp, x[2], x[0], x[1])) continue;
    best = x[2]; bcx = x[0]; bcy = x[1];
  }
  if(best <= 0.0) return 0.0;
  if(target) nearest(row, hp, best, target, &bcx, &bcy);
  aabb[0] = bcx - 0.5*best*aw;
  aabb[1] = bcy - 0.5*best*ah;
  aabb[2] = bcx + 0.5*best*aw;
  aabb[3] = bcy + 0.5*best*ah;
  return best;
}

void ui_callback(
    dt_module_t *module,
    dt_token_t   param,
    float        aspect)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  float H[16], T[4];
  float *f = (float*)module->committed_param;
  for(int k=0;k<12;k++) H[k] = f[k];
  f += 12;
  for(int k=0;k<4;k++) T[k] = f[k];

  double hp[CROP_NUM_HP][3];
  get_half_planes(wd, ht, T, H, MIN(wd, ht) > 400 ? 2.0 : 0.0, hp);

  float *p_crop = (float *)dt_module_param_float(module, 1);
  double target[2] = { wd/2.0, ht/2.0 };
  if(!(p_crop[0] == 1.0f && p_crop[1] == 3.0f && p_crop[2] == 3.0f && p_crop[3] == 7.0f))
  {
    target[0] = 0.5*(p_crop[0] + p_crop[1]) * wd;
    target[1] = 0.5*(p_crop[2] + p_crop[3]) * ht;
  }

  double aabb[4];
  if(aspect > 0.0f)
  {
    if(inscribe(hp, aspect, 1.0, target, aabb) <= 0.0) return;
  }
  else
  {
    double box[4], square[4];
    const double side = inscribe(hp, 1.0, 1.0, 0, square);
    if(side <= 0.0 || !get_bounds(hp, box)) return;
    const double floor = side*side;
    const double lo = log(floor/((box[3]-box[1])*(box[3]-box[1])));
    const double hi = log((box[2]-box[0])*(box[2]-box[0])/floor);
    if(!isfinite(lo) || !isfinite(hi) || hi < lo) return;
    const double g = 0.6180339887498949;
    const int steps = MAX(256, (int)ceil(32.0*(hi-lo)));
    double best = -1.0, t = 0.0, tmp[4];
    for(int i=0;i<=steps;i++)
    {
      const double ti = lo + (hi-lo)*i/steps, a = exp(ti), s = inscribe(hp, a, 1.0, 0, tmp);
      if(s*s*a > best) { best = s*s*a; t = ti; }
    }
    if(best <= 0.0) return;
    double m = MAX(lo, t - (hi-lo)/steps), M = MIN(hi, t + (hi-lo)/steps);
    double t0 = M - g*(M-m), t1 = m + g*(M-m), a0 = exp(t0), a1 = exp(t1);
    double s0 = inscribe(hp, a0, 1.0, 0, tmp), f0 = s0*s0*a0;
    double s1 = inscribe(hp, a1, 1.0, 0, tmp), f1 = s1*s1*a1;
    for(int it=0;it<48;it++)
    {
      if(f0 > f1) { M = t1; t1 = t0; f1 = f0; t0 = M - g*(M-m); a0 = exp(t0);
                    s0 = inscribe(hp, a0, 1.0, 0, tmp); f0 = s0*s0*a0; }
      else        { m = t0; t0 = t1; f0 = f1; t1 = m + g*(M-m); a1 = exp(t1);
                    s1 = inscribe(hp, a1, 1.0, 0, tmp); f1 = s1*s1*a1; }
    }
    if(inscribe(hp, exp(0.5*(m+M)), 1.0, target, aabb) <= 0.0) return;
  }

  p_crop[0] = aabb[0] / wd;
  p_crop[1] = aabb[2] / wd;
  p_crop[2] = aabb[1] / ht;
  p_crop[3] = aabb[3] / ht;
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
  module->connector[0].roi.marker = module->connector[1].roi.marker;
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
    dt_module_set_param_float(module, dt_token("rotate"), rot);
  if(p_crop[0] == 1.0f && p_crop[1] == 3.0f && p_crop[2] == 3.0f && p_crop[3] == 7.0f)
    dt_module_set_param_float_n(module, dt_token("crop"), crop, 4);
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*20;
  return 0;
}
