#include "../crop/gaussian_elimination.h" // TODO: clean up and move to core/api headers
#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void ui_callback(
    dt_module_t *mod,
    dt_token_t   param)
{ // callback for import from cc24/colour picker button:
  // retrieve instance token from our params and corresponding colour picker by that instance name:
  const int   parid   = dt_module_get_param(mod->so, param);
  const char *inst    = dt_module_param_string(mod, parid); if(!inst) return;
  const int   pick_id = dt_module_get(mod->graph, dt_token("pick"), dt_token(inst));
  if(pick_id < 0 || pick_id > mod->graph->num_modules) return;
  const dt_module_t *mp = mod->graph->module+pick_id;
  // retrieve rbmap data from colour picker
  const int   pck_pid = dt_module_get_param(mp->so, dt_token("picked"));
  const int   ref_pid = dt_module_get_param(mp->so, dt_token("ref"));
  const int   cnt_pid = dt_module_get_param(mp->so, dt_token("nspots"));
  // only take 18 colour patches + 1 white
  const int   cnt = CLAMP(dt_module_param_int(mp, cnt_pid)[0], 0, 19);
  const float *ref = dt_module_param_float(mp, ref_pid);
  const float *pck = dt_module_param_float(mp, pck_pid);
  const int   rbm_pid = dt_module_get_param(mod->so, dt_token("rbmap"));
  const int   rbc_pid = dt_module_get_param(mod->so, dt_token("cnt"));
  float *rbmap = (float *)dt_module_param_float(mod, rbm_pid);
  int   *rbcnt = (int   *)dt_module_param_int  (mod, rbc_pid);
  if(!rbmap || !rbcnt)     return;
  if(!pck || !ref || !cnt) return;
  for(int i=0;i<cnt;i++)
  { // init source -> destination
    const float pckb = pck[3*i+0] + pck[3*i+1] + pck[3*i+2];
    const float refb = ref[3*i+0] + ref[3*i+1] + ref[3*i+2];
    rbmap[4*i+0] = pck[3*i+0] / pckb;
    rbmap[4*i+1] = pck[3*i+2] / pckb;
    rbmap[4*i+2] = ref[3*i+0] / refb;
    rbmap[4*i+3] = ref[3*i+2] / refb;
  }
  rbcnt[0] = cnt;
}


static inline void
clip_poly(
    const float *p,        // pointer to polygon points
    uint32_t     p_cnt,    // number of corners
    const float *w,        // constant white
    float       *v)        // point potentially outside, will clip this to p
{
  for(int i=0;i<p_cnt;i++)
  {
    float vp0 [] = {v[0]-p[2*i+0], v[1]-p[2*i+1]};
    float p1p0[] = {p[2*i+2+0]-p[2*i+0], p[2*i+2+1]-p[2*i+1]};
    float n[] = {p1p0[1], -p1p0[0]};
    float dotv = n[0]*vp0[0] + n[1]*vp0[1];
    if(dotv <= 0) continue; // inside

    // project along line in rb space
    float dotvw = n[0]*(v[0]-w[0]) + n[1]*(v[1]-w[1]);
    float dotpw = n[0]*(p[2*i]-w[0]) + n[1]*(p[2*i+1]-w[1]);
    float t = dotpw/dotvw;
    if(t > 0.0f && t < 1.0f)
    {
      v[0] = w[0] + t * (v[0] - w[0]);
      v[1] = w[1] + t * (v[1] - w[1]);
    }
  }
}

// clamp to spectral locus:
static inline void
clip_spectral_locus(float *v, const float *white)
{
  // manually picked subset of points on the spectral locus transformed to
  // rec2020 r/(r+g+b), b/(r+g+b). the points have been picked by walking the
  // wavelength domain in 5nm intervals and computing XYZ chromaticity
  // coordinates by integrating the 1931 2deg CMF.
  static const float gamut_rec2020_rb[] =
  {
    0.119264, 1.0047,
    0.0352311, 1.01002,
    -0.0587504, 0.957361,
    -0.166419, 0.817868,
    -0.269602, 0.55565,
    -0.300098, 0.281734,
    -0.228864, 0.0937406,
    -0.0263976, 0.00459292,
    0.230836, -0.0158645,
    1.051, 0.00138612,
    0.119264, 1.0047,        // wrap around point
  };
  const int cnt = sizeof(gamut_rec2020_rb) / sizeof(gamut_rec2020_rb[0]) / 2 - 1;
  clip_poly(gamut_rec2020_rb, cnt, white, v);
}

// clamp to rec2020 gamut:
static inline void
clip_rec2020(float *v, const float *white)
{
  const float gamut_rec2020_rb[] = {
    1.0f, 0.0f, // red
    0.0f, 1.0f, // blue
    0.0f, 0.0f, // green
    1.0f, 0.0f, // red again
  };
  const int cnt = sizeof(gamut_rec2020_rb) / sizeof(gamut_rec2020_rb[0]) / 2 - 1;
  clip_poly(gamut_rec2020_rb, cnt, white, v);
}

// clamp to rec709 gamut:
static inline void
clip_rec709(float *v, const float *white)
{
  const float gamut_rec709_rb[] = {
    0.88008f, 0.02299f,   // red
    0.04558f, 0.94246f,   // blue
    0.24632f, 0.06584f,   // green
    0.88008f, 0.02299f,   // red (need to have correct winding order)
  };
  const int cnt = sizeof(gamut_rec709_rb) / sizeof(gamut_rec709_rb[0]) / 2 - 1;

  // used this matrix: rec709 -> rec2020
  // 0.62750375, 0.32927542, 0.04330267,
  // 0.06910828, 0.91951917, 0.0113596,
  // 0.01639406, 0.08801128, 0.89538036
  clip_poly(gamut_rec709_rb, cnt, white, v);
}

// create 2d pattern of points:
// outer ring: to push back into gamut
// inner ring: to protect inner values
static inline void
create_ring(
    const float *wb,          // camera white balance coefs
    const float  saturation,  // saturate the inner ring
    const float *white,       // adjust white point
    float       *source,      // generated list of source points
    float       *target,      // generated list of target points
    int          gamut)       // gamut mapping mode: 0 none, 1 spectral locus, 2 rec2020, 3 rec709
{
  // 6+6+1 points in 2D RBF: outer, inner, white.
  const int N = 13;
  const float S = 0.20f; // larger S means more white means less saturated
  float out[21] = {
    S, 1, 1,  // dim red
    S, S, 1,  // dim yellow
    1, S, 1,  // dim green
    1, S, S,  // dim cyan
    1, 1, S,  // dim blue
    S, 1, S,  // dim magenta
    1, 1, 1   // white
  };

  for(int k=0;k<7;k++)
  {
    const float b = out[3*k+0] + out[3*k+1] + out[3*k+2];
    out[k*3+0] /= b;
    out[k*3+2] /= b;
  }
  const float w2[] = {1.0/3.0, 1.0/3.0};
  for(int k=0;k<N;k++)
  {
    float x = k > 6 ? 4.0*(out[3*(k-7)+0]-w2[0])+w2[0] : out[3*k+0];
    float y = k > 6 ? 4.0*(out[3*(k-7)+2]-w2[1])+w2[1] : out[3*k+2];
    source[2*k+0] = target[2*k+0] = x;
    source[2*k+1] = target[2*k+1] = y;
    if     (gamut == 1) clip_spectral_locus(target + 2*k, w2);
    else if(gamut == 2) clip_rec2020(target + 2*k, w2);
    else if(gamut == 3) clip_rec709(target + 2*k, w2);
  }
  for(int k=0;k<6;k++)
  { // saturation:
    if(saturation < 0.5f)
    { // desaturate outer ring, too:
      float s = saturation * 2.0f;
      target[2*k+0] = (1.0f-s)*white[0] + s*target[2*k+0];
      target[2*k+1] = (1.0f-s)*white[1] + s*target[2*k+1];
      target[2*(k+6)+0] = white[0];
      target[2*(k+6)+1] = white[1];
    }
    else
    { // desaturate inner ring only:
      float s = (saturation - 0.5f)*2.0f;
      target[2*(k+6)+0] = (1.0f-s)*white[0] + s*target[2*(k+6)+0];
      target[2*(k+6)+1] = (1.0f-s)*white[1] + s*target[2*(k+6)+1];
    }
  }
  // move white
  // target[2*12+0] = white[0];
  // target[2*12+1] = white[1];
  // source[2*12+0] = white[0];
  // source[2*12+1] = white[1];
  // TODO: further parametric manipulations?
  // rotate hue?
  // move individual colours
  // etc
}


// thinplate spline kernel phi(r).
// note that this one is different to the one used in darktable:
// okay, it's 2d to begin with. but also the threshold added to r2 is crucial,
// or else distance 0 and distance 1 will both evaluate to 0, resulting in
// interesting curves that do not pass through the control points. this is true
// at least in my tests with 5 control points, maybe the effect evens out for more
// points which do not happen to reach exactly 0 and 1 as distance.
static inline double
kernel(const float *x, const float *y)
{
  const double r2 = 1e-3 +
      .99*((x[0] - y[0]) * (x[0] - y[0]) + (x[1] - y[1]) * (x[1] - y[1]));
  // return expf(-0.5f*r2/0.0025f);
  // return expf(-0.5f*r2/0.002f);
  return r2 * logf(r2);
}

static inline void
compute_coefficients(
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
    for(int i=0;i<N;i++) A[i*N2+N+0] = A[(N+0)*N2+i] = source[2*i+0];
    for(int i=0;i<N;i++) A[i*N2+N+1] = A[(N+1)*N2+i] = source[2*i+1];
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
    else
    { // yes, really, we should have continued to use the svd/pseudoinverse for exactly such cases.
      // i might bring it back at some point.
      fprintf(stderr, "[colour] fuck, matrix was singular or something!\n");
      fprintf(stderr, "[colour] N=%d\n", N);
      for(int k=0;k<N;k++)
        fprintf(stderr, "[colour] src = %g %g --> %g %g\n",
            source[2*k+0],
            source[2*k+1],
            target[2*k+0],
            target[2*k+1]);
      fprintf(stderr, "[colour] A = \n");
      for(int j=0;j<N2;j++)
      {
        for(int i=0;i<N2;i++)
          fprintf(stderr, "%.2f\t", A[j*N2+i]);
        fprintf(stderr, "\n");
      }
    }
    // fprintf(stderr, "matrix part %g %g %g %g\n",
    //     coef[4*N+2],
    //     coef[4*N+3],
    //     coef[4*(N+1)+2],
    //     coef[4*(N+1)+3]);
    free(pivot);
    free(b);
    free(A);
  }
  }
}


void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  uint32_t *i = (uint32_t *)module->committed_param;

  // grab params by name:
  const float *p_wb  = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("white")));
  const float  p_tmp = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("temp")))[0];
  const int    p_cnt = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("cnt")))[0];
  const float *p_map = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("rbmap")));
        int    p_mat = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("matrix")))[0];
  // const int    p_gam = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("gamut")))[0];
  const int    p_mod = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("mode")))[0];
  const float  p_sat = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("sat")))[0];
  const int    p_pck = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("picked")))[0];

  // wb and exposure mul:
  f[0] = p_wb[0] / p_wb[1];
  f[1] = 1.0f;
  f[2] = p_wb[2] / p_wb[1];
  f[3] = powf(2.0f, ((float*)module->param)[0]);
  // linear, as the dng spec says:
  // f[4+12+4+88+0] = 1.0f - CLAMP((p_tmp - 2856.f)/(6504.f-2856.f), 0.0f, 1.0f);
  // turingbot reverses this function to more accurately blend along CCT:
  f[4+12+4+88+0] = 1.0f - CLAMP(tanf(asinhf(46.3407f+p_tmp))+(-0.0287128f*cosf(0.000798585f*(714.855f-p_tmp)))+0.942275f, 0.0f, 1.0f);
  i[4+12+4+88+1] = p_mat == 3 ? 1 : 0; // colour mode matrix or clut
  f[4+12+4+88+2] = p_sat;
  i[4+12+4+88+3] = p_pck;

  if(p_mat == 1 && !(module->img_param.cam_to_rec2020[0] > 0)) p_mat = 0; // no matrix? default to identity
  if(p_mat == 1)
  { // the one that comes with the image from the source node:
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4+4*i+j] = module->img_param.cam_to_rec2020[3*j+i];
  }
  else if(p_mat == 2)
  { // CIE XYZ
    const float xyz_to_rec2020[] = {
      1.7166511880, -0.3556707838, -0.2533662814,
     -0.6666843518,  1.6164812366,  0.0157685458,
      0.0176398574, -0.0427706133,  0.9421031212};
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4+4*i+j] = xyz_to_rec2020[3*j+i];
  }
  else
  { // p_mat == 0 (or default) rec2020, identity matrix
    for(int j=0;j<3;j++) for(int i=0;i<3;i++)
      f[4+4*j+i] = i==j ? 1.0f : 0.0f;
  }
  
  if(p_mod == 1)
  { // mode is set to "data driven", means we have explicit src/tgt points
    const int N = p_cnt;
    i[16] = N;
    i[18] = i[19] = i[17] = 0;

    float p2_src[40], p2_tgt[40];
    for(int i=0;i<p_cnt;i++)
    {
#if 1 // plain rgb
      p2_src[2*i+0] = p_map[4*i+0];
      p2_src[2*i+1] = p_map[4*i+1];
      p2_tgt[2*i+0] = p_map[4*i+2];
      p2_tgt[2*i+1] = p_map[4*i+3];
#else // log space (cannot exceed rec2020)
      p2_src[2*i+0] = log(MAX(1e-8, (1.0-p_map[4*i+0]-p_map[4*i+1])/(1e-8+p_map[4*i+0])));
      p2_src[2*i+1] = log(MAX(1e-8, (1.0-p_map[4*i+0]-p_map[4*i+1])/(1e-8+p_map[4*i+1])));
      p2_tgt[2*i+0] = log(MAX(1e-8, (1.0-p_map[4*i+2]-p_map[4*i+3])/(1e-8+p_map[4*i+2])));
      p2_tgt[2*i+1] = log(MAX(1e-8, (1.0-p_map[4*i+2]-p_map[4*i+3])/(1e-8+p_map[4*i+3])));
#endif
    }

    // init f[20]..
    memset(f + 20, 0, sizeof(float)*44);
    for(int k=0;k<N;k++)
    { // source points
      f[20 + 4*k + 0] = p2_src[2*k+0];
      f[20 + 4*k + 1] = p2_src[2*k+1];
    }
    // TODO: go through target and map to gamut!
    compute_coefficients(N, p2_src, p2_tgt, f + 20);
  }
  else
  { // mode == 0 (or default): "parametric" mode
    i[16] = 0; // disable rbf
#if 0
    float wb[] = {1, 1, 1};
    // TODO: bind some param to white and saturations!
    float white[] = {0.33333, 0.33333};
    // { p_wb[0], p_wb[1] };
    float source[26];
    float target[26];
    create_ring(wb, 1.0f, // p_wb[2],
        white, source, target, p_gam);
    const int N = 13;
    i[16] = N;
    i[18] = i[19] = i[17] = 0;
    // init f[20]..
    memset(f + 20, 0, sizeof(float)*44);
    for(int k=0;k<N;k++)
    { // source points
      f[20 + 4*k + 0] = source[2*k+0];
      f[20 + 4*k + 1] = source[2*k+1];
    }
    compute_coefficients(N, source, target, f + 20);
#endif
  }
}

int init(dt_module_t *mod)
{
  // wb, matrix, uvec4 cnt, vec4 coef[22], mode, gamut, exposure, sat
  mod->committed_param_size = sizeof(float)*(4+12+4+88+2+2);
  return 0;
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int have_clut = dt_connected(module->connector+2);
  int have_pick = dt_connected(module->connector+3);
  assert(graph->num_nodes < graph->max_nodes);
  const int nodeid = graph->num_nodes++;
  graph->node[nodeid] = (dt_node_t){
    .name   = module->name,
    .kernel = dt_token("main"), // file name
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 4,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[1].roi,
    },{
      .name   = dt_token("clut"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .connected_mi = -1,
    },{
      .name   = dt_token("picked"),
      .type   = dt_token("read"),
      .chan   = dt_token("r"),
      .format = dt_token("atom"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    }},
    .push_constant_size = 2*sizeof(uint32_t),
    .push_constant = { have_clut, have_pick },
  };
  dt_connector_copy(graph, module, 0, nodeid, 0);
  dt_connector_copy(graph, module, 1, nodeid, 1);
  if(have_clut) dt_connector_copy(graph, module, 2, nodeid, 2);
  else          dt_connector_copy(graph, module, 0, nodeid, 2); // dummy
  if(have_pick) dt_connector_copy(graph, module, 3, nodeid, 3);
  else          dt_connector_copy(graph, module, 0, nodeid, 3); // dummy
}
