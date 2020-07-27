#include "../crop/gaussian_elimination.h" // TODO: clean up and move to core/api headers
#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

// thinplate spline kernel phi(r)
static inline double kernel(const float *x, const float *y)
{
  const double r
      = sqrt((x[0] - y[0]) * (x[0] - y[0]) + (x[1] - y[1]) * (x[1] - y[1]));
  return r * r * logf(r > 1e-8f ? r: 1e-8f);
}

static inline void compute_coefficients(
  const int    N,        // number of patches
  const float *source,   // N 2d source coordinates
  const float *target,   // N 2d target coordinates
  float       *coef)     // will write the 2*(N+2) coefficient vector here, assumed 0 inited
{
  /*
      K. Anjyo, J. P. Lewis, and F. Pighin, "Scattered data
      interpolation for computer graphics," ACM SIGGRAPH 2014 Courses
      on - SIGGRAPH ’14, 2014.
      http://dx.doi.org/10.1145/2614028.2615425
      http://scribblethink.org/Courses/ScatteredInterpolation/scatteredinterpcoursenotes.pdf

      construct the system matrix and the vector of function values and
      solve the set of linear equations

      / R   P \  / c \   / f \
      |       |  |   | = |   |
      \ P^t 0 /  \ d /   \ 0 /

      for the coefficient vector (c d)^t.
      we work in 2D xy chromaticity space.
      our P is a 2x2 matrix (no constant part, we want to keep black black)
  */
  switch(N) // N is the number of patches, i.e. we have N+2 constraints in the matrix
  {
  case 0: // identity
    coef[0+2] = coef[4+3] = 1.0;
    break;
  case 1: // one patch: white balance (we could move, but i don't think we want that)
    coef[4*N+0+2] = target[0] / source[0];
    coef[4*N+4+3] = target[1] / source[1];
    break;
  default: // fully generic case, N patches
  {
    // setup linear system of equations
    int N2 = N+2;
    double *A = malloc(sizeof(*A) * N2 * N2);
    double *b = malloc(sizeof(*b) * N2);
    // coefficients from nonlinear radial kernel functions
    for(int j=0;j<N;j++)
      for(int i=j;i<N;i++)
        A[j*N2+i] = A[i*N2+j] = kernel(source + 2*i, source + 2*j);
    // coefficients from constant and linear functions
    for(int i=0;i<N2;i++) A[i*N2+N+0] = A[(N+0)*N2+i] = source[2*i+0];
    for(int i=0;i<N2;i++) A[i*N2+N+1] = A[(N+1)*N2+i] = source[2*i+1];
    // lower-right zero block
    for(int j=N;j<N2;j++)
      for(int i=N;i<N2;i++)
        A[j*N2+i] = 0;
    // make coefficient matrix triangular
    int *pivot = malloc(sizeof(*pivot)*N2);
    if (gauss_make_triangular(A, pivot, N2))
    {
      // calculate coefficients for x channel
      for(int i=0;i<N; i++) b[i] = target[2*i];
      for(int i=N;i<N2;i++) b[i] = 0;
      gauss_solve_triangular(A, pivot, b, N2);
      for(int i=0;i<N2;i++) coef[4*i+2] = b[i];
      // calculate coefficients for y channel
      for(int i=0;i<N; i++) b[i] = target[2*i+1];
      for(int i=N;i<N2;i++) b[i] = 0;
      gauss_solve_triangular(A, pivot, b, N2);
      for(int i=0;i<N2;i++) coef[4*i+3] = b[i];
    }
    free(pivot);
    free(b);
    free(A);
  }
  }
}


void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  // params:
  // exposure:float:1:0.0
  // cnt:int:0
  // wb:float:4:1.0
  // mat:float:9:-1.0
  // source:float:40:0.0
  // target:float:40:0.0
  float *f = (float *)module->committed_param;
  uint32_t *i = (uint32_t *)module->committed_param;
  if(module->img_param.whitebalance[0] > 0)
  {
    for(int k=0;k<4;k++)
      f[k] = powf(2.0f, ((float*)module->param)[0]) *
        module->img_param.whitebalance[k];
  }
  else
    for(int k=0;k<4;k++)
      f[k] = powf(2.0f, ((float*)module->param)[0]);
  for(int k=0;k<12;k++) f[4+k] = 0.0f;
  if(module->img_param.cam_to_rec2020[0] > 0.0f)
  { // camera to rec2020 matrix
    // mat3 in glsl is an array of 3 vec4 column vectors:
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4+4*j+i] = module->img_param.cam_to_rec2020[3*j+i];
  }
  else
  { // identity
    f[4+0] = f[4+5] = f[4+10] = 1.0f;
  }
  // init i[16]..i[17] (uvec2 num patches + pad)
  const int N = ((uint32_t*)module->param)[1];
  i[16] = N;
  i[18] = i[19] = i[17] = 0;
  // init f[20]..
  memset(f + 20, 0, sizeof(float)*44);
  for(int k=0;k<N;k++)
  {
    f[20 + 4*k + 0] = ((float*)module->param + 15)[2*k+0];
    f[20 + 4*k + 1] = ((float*)module->param + 15)[2*k+1];
  }
  compute_coefficients(N,
      ((float*)module->param) + 15,
      ((float*)module->param) + 55,
      f + 20);
}

int init(dt_module_t *mod)
{
  // wb, matrix, cnt, source, coef
  mod->committed_param_size = sizeof(float)*(4+12+2+40+44);
  return 0;
}

#if 0
// params:
// black and white points (img)
// white balance coeffs (img)
// colour matrix (img)
// user defined post processing:
// - move white point
// - change saturation
// - clip which gamut
// - how much to move points with low saturation

// clamp to gamut:
{
  // spectral locus polygon clipping:
  // hand picked polygon with 8 line segments/vertices?
}

// clamp to gamut:
{
  // triangle coordinates, rec2020 for instance
  // use rec2020 to xyz matrix to compute xy coords
}

{
  // p_0..p_k polygon
  // v0 v1 line segment to clip, v0 is white and assumed to be inside
  // for i=0..k (and wrap around i+1=0 in last iteration)
  //   dot(n(p[i+1] - p[i]), v1-p[i]) <= 0 continue
  //   else t = dot / dot(n,v1-v0), v = v0 + t * (v1-v0)
  //   v1 <-- v
}

// create 2d pattern of points:
// outer ring: [01]^3 in rgb transformed by wb + cmatrix
// inner ring: [1/4 3/4]^3 transformed by the same
{
  // 6+6+1 points in 2D RBF
  // outer, inner, white
  // how to scale inner ring / saturation?
  // inner ring at (0.5-t/2, 0.5+t/2) and keep fixed (after gamut mapping)
}
#endif


#if 0
// processing:
// 1) multiply cam wb coeff or d65 wb coeff to cam rgb
// 2) multiply cam rgb -> XYZ matrix
// 3) keep Y channel (or X+Y+Z) for later
// 4) apply 2d -> 2d rbf on xy:
vec2 coef[N+2]; // RBF coefficients
vec2 vert[N+2]; // RBF vertex position (source of vertex pairs)
out += coef[N+0] * in; // 2x2 matrix, the
out += coef[N+1] * in; // polynomial part
for(int i=0;i<N;i++)
  out += coef[i+2] * kernel(in, vert[i]);
#endif
