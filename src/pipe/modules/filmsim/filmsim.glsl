// helper functions for film simulation.
// this is all more or less a straight port of agx-emulsion
// https://github.com/andreavolpato/agx-emulsion
// so as derivative work i suppose this makes it GPL-v3

// number of spectral samples/wavelengths when integrating
#define SN 40

vec3 get_sensitivity(vec2 tc)
{
  vec3 log_sensitivity = texture(img_filmsim, tc).rgb;
  // vec3 log_sensitivity = sample_catmull_rom_1d(img_filmsim, tc).rgb;
  vec3 sensitivity = exp2(log_sensitivity * 3.32192809489);
  return mix(sensitivity, vec3(0.0), isnan(sensitivity));
}

float envelope(float w)
{
  return 1000.0*smoothstep(380.0, 400.0, w)*(1.0-smoothstep(700.0, 730.0, w));
}

vec3 thorlabs_filters(float w)
{
  // this is what looks like it might be a good fit to the thorlabs filters in a plot.
  // the lines marked with XXX smooth out the yellow/magenta transition at 500nm such that the two
  // filters sum to one. as a result i can fit the neutral filter configuration for supra and portra
  // without using negative weights (!!).
  // makes me think that somewhere else in the pipeline there might be something off.
  float cyan    = 0.93*smoothstep(345.0, 380.0, w)*(1.0-smoothstep(545.0, 590.0, w))+0.7*smoothstep(775.0,810,w);
  float magenta = 0.9*smoothstep(355.0, 380.0, w)*(1.0-smoothstep(475.0, 505.0, w));
  // float magenta = 0.9*smoothstep(355.0, 380.0, w)*(1.0-smoothstep(475.0, 525.0, w)); // XXX
  if(w > 550.0)
    magenta = 0.9*smoothstep(595.0, 645.0, w);
  float yellow  = 0.92*smoothstep(492.0, 542.0, w) + 0.2*(1.0-smoothstep(370, 390, w));
  // float yellow  = 0.92*smoothstep(475.0, 525.0, w) + 0.2*(1.0-smoothstep(370, 390, w)); // XXX
  return vec3(cyan, magenta, yellow);
}

float noise(in vec2 Rp)
{
  ivec2 i = ivec2(floor(Rp));
  vec2 f = fract(Rp);

  // quintic interpolation
  vec2 u = f*f*f*(f*(f*6.0-15.0)+10.0);

  uint q1x = uint(i.x) * 1597334677U;
  uint q1y = uint(i.y) * 3812015801U;

  uint q2x = q1x + 1597334677U;
  uint q3y = q1y + 3812015801U;

  uint xor1 = q1x ^ q1y;
  uint xor2 = q2x ^ q1y;
  uint xor3 = q1x ^ q3y;
  uint xor4 = q2x ^ q3y;

  uvec4 xors = uvec4(xor1, xor2, xor3, xor4);
  uvec4 h_x = xors * 1597334677U;
  uvec4 h_y = xors * 3812015801U;

  const float scale_factor = 2.0 / 4294967296.0;
  vec4 hx_f = vec4(h_x) * scale_factor - 1.0;
  vec4 hy_f = vec4(h_y) * scale_factor - 1.0;

  vec4 dx = f.x - vec4(0.0, 1.0, 0.0, 1.0);
  vec4 dy = f.y - vec4(0.0, 0.0, 1.0, 1.0);
  vec4 v = hx_f * dx + hy_f * dy;

  return mix(mix(v.x, v.y, u.x), mix(v.z, v.w, u.x), u.y);
}

vec3 noise_v3(in vec3 Rp_x, in vec3 Rp_y)
{
  ivec3 i_x = ivec3(floor(Rp_x));
  ivec3 i_y = ivec3(floor(Rp_y));
  vec3 f_x = fract(Rp_x);
  vec3 f_y = fract(Rp_y);

  // quintic interpolation
  vec3 u_x = f_x*f_x*f_x*(f_x*(f_x*6.0-15.0)+10.0);
  vec3 u_y = f_y*f_y*f_y*(f_y*(f_y*6.0-15.0)+10.0);

  uvec3 q1x = uvec3(i_x) * 1597334677U;
  uvec3 q1y = uvec3(i_y) * 3812015801U;

  uvec3 q2x = q1x + 1597334677U;
  uvec3 q3y = q1y + 3812015801U;

  uvec3 xor1 = q1x ^ q1y;
  uvec3 xor2 = q2x ^ q1y;
  uvec3 xor3 = q1x ^ q3y;
  uvec3 xor4 = q2x ^ q3y;

  const float scale_factor = 2.0 / 4294967296.0;

  uvec3 h1x = xor1 * 1597334677U;
  uvec3 h1y = xor1 * 3812015801U;
  uvec3 h2x = xor2 * 1597334677U;
  uvec3 h2y = xor2 * 3812015801U;
  uvec3 h3x = xor3 * 1597334677U;
  uvec3 h3y = xor3 * 3812015801U;
  uvec3 h4x = xor4 * 1597334677U;
  uvec3 h4y = xor4 * 3812015801U;

  vec3 v1 = (vec3(h1x) * scale_factor - 1.0) * f_x       + (vec3(h1y) * scale_factor - 1.0) * f_y;
  vec3 v2 = (vec3(h2x) * scale_factor - 1.0) * (f_x-1.0) + (vec3(h2y) * scale_factor - 1.0) * f_y;
  vec3 v3 = (vec3(h3x) * scale_factor - 1.0) * f_x       + (vec3(h3y) * scale_factor - 1.0) * (f_y-1.0);
  vec3 v4 = (vec3(h4x) * scale_factor - 1.0) * (f_x-1.0) + (vec3(h4y) * scale_factor - 1.0) * (f_y-1.0);

  return mix(mix(v1, v2, u_x), mix(v3, v4, u_x), u_y);
}

float noisef(in vec2 Rp)
{
  return noise(Rp) + .57*noise(0.61*Rp);
}

vec3 noisef_v3(in vec3 Rp_x, in vec3 Rp_y)
{
  return noise_v3(Rp_x, Rp_y) + 0.57 * noise_v3(0.61 * Rp_x, 0.61 * Rp_y);
}

// separate density into three layers of grains (coarse, mid, fine) that sum up
// to the max density. lower density values are taken mostly by coarse grains.
// use perlin gradient noise / correlated to simulate the three layers (represents non-uniformity of grain counts/pixel)
// simulate a poisson distribution x3: are the grains turned?
// expectation should be =density, i.e. n*p + n*p + n*p = density for the three layers
vec3 add_grain(ivec2 ipos, vec3 density, float scale)
{
  ipos = ivec2(vec2(ipos)*scale); // if not processing full size (scale = 1), adjust here
  float grain_size = params.grain_size;
  float density_max = 3.3;

  vec3 np = density / density_max;
  vec3 sat = clamp(1.5 * np, 0.0, 1.0);
  vec3 factor_base = (1.0 - params.grain_uniformity) * 4.0 * sat * (1.0 - sat) / (scale * scale);

  // piecewise layer densities from coarse to fine layers; density_max folded in upfront
  vec3 npl0 = density_max * clamp(np, 0.0, 0.1);
  vec3 npl1 = density_max * clamp(np - 0.1, 0.0, 0.2);
  vec3 npl2 = density_max * max(vec3(0.0), np - 0.3);

  vec2 base_ipos = vec2(ipos) + global.hash*0.000001 + global.frame*133.7;
  float rot_c = 0.98006, rot_s = 0.198669;
  vec2 rot_base_ipos = vec2(rot_c*base_ipos.x + rot_s*base_ipos.y, -rot_s*base_ipos.x + rot_c*base_ipos.y);
  float k = 0.3 / grain_size;

  vec3 res = vec3(0.0);
  vec3 base_pos_x = rot_base_ipos.x + vec3(0.0, 1000.0, 2000.0);
  vec3 base_pos_y = rot_base_ipos.y + vec3(0.0, 1000.0, 2000.0);
  vec3 c0 = vec3(0.5, 0.4, 0.2);

  // Layer 0: Coarse
  if (any(greaterThan(npl0, vec3(0.0))))
  {
    vec3 c = c0 * 1.0;
    vec3 n = noisef_v3(base_pos_x * (c * k), base_pos_y * (c * k));
    res += npl0 * max(vec3(0.0), 1.0 + factor_base * n);
  }

  // Layer 1: Mid
  if (any(greaterThan(npl1, vec3(0.0))))
  {
    vec3 c = c0 * 2.5;
    vec3 n = noisef_v3(base_pos_x * (c * k), base_pos_y * (c * k));
    res += npl1 * max(vec3(0.0), 1.0 + factor_base * n);
  }

  // Layer 2: Fine
  if (any(greaterThan(npl2, vec3(0.0))))
  {
    vec3 c = c0 * 5.0;
    vec3 n = noisef_v3(base_pos_x * (c * k), base_pos_y * (c * k));
    res += npl2 * max(vec3(0.0), 1.0 + factor_base * n);
  }

  return res;
}

const int s_sensitivity = 0;
const int s_dye_density = 1;
const int s_density_curve = 2;
float get_tcy(int type, int stock)
{
  const float s = textureSize(img_filmsim, 0).y;
  const int off = type;
  return (stock * 3 + off + 0.5)/s;
}

// --- Shared Memory Buffers for caching spectral integration inputs ---
// Optimized for vectorized spectral loops (4 wavelengths at once)
shared vec4 shared_expose_factor_r[11];
shared vec4 shared_expose_factor_g[11];
shared vec4 shared_expose_factor_b[11];

shared vec4 shared_enlarger_dye_r[11];
shared vec4 shared_enlarger_dye_g[11];
shared vec4 shared_enlarger_dye_b[11];
shared vec4 shared_enlarger_factor_r[11];
shared vec4 shared_enlarger_factor_g[11];
shared vec4 shared_enlarger_factor_b[11];

shared vec4 shared_scan_dye_r[11];
shared vec4 shared_scan_dye_g[11];
shared vec4 shared_scan_dye_b[11];
shared vec4 shared_scan_factor_r[11];
shared vec4 shared_scan_factor_g[11];
shared vec4 shared_scan_factor_b[11];

shared mat3 shared_M;
shared vec3 shared_M_sum;

void init_expose_film_shared(int film)
{
  int tid = int(gl_LocalInvocationIndex);
  if (tid < 44)
  {
    shared_expose_factor_r[tid/4][tid%4] = 0.0;
    shared_expose_factor_g[tid/4][tid%4] = 0.0;
    shared_expose_factor_b[tid/4][tid%4] = 0.0;
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_sensitivity, film));
    vec3 sensitivity = get_sensitivity(tc);
    float env = envelope(lambda);
    float pdf = 2.0 * 41.0;
    vec3 factor = sensitivity * env / pdf;
    shared_expose_factor_r[tid/4][tid%4] = factor.r;
    shared_expose_factor_g[tid/4][tid%4] = factor.g;
    shared_expose_factor_b[tid/4][tid%4] = factor.b;
  }
  barrier();
}

void init_enlarger_shared(int film, int paper)
{
  int tid = int(gl_LocalInvocationIndex);
  if (tid < 44)
  {
    shared_enlarger_dye_r[tid/4][tid%4] = 0.0;
    shared_enlarger_dye_g[tid/4][tid%4] = 0.0;
    shared_enlarger_dye_b[tid/4][tid%4] = 0.0;
    shared_enlarger_factor_r[tid/4][tid%4] = 0.0;
    shared_enlarger_factor_g[tid/4][tid%4] = 0.0;
    shared_enlarger_factor_b[tid/4][tid%4] = 0.0;
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_sensitivity, paper));
    vec3 sensitivity = get_sensitivity(tc);
    
    tc.y = get_tcy(s_dye_density, film);
    vec4 dye_density = texture(img_filmsim, tc);
    dye_density = mix(dye_density, vec4(1000000.0), isnan(dye_density));
    dye_density.xyz *= 3.32192809489;

    vec3 neutral = vec3(params.filter_c,
        clamp(params.filter_m, 0, 1) + 0.1*params.tune_m,
        clamp(params.filter_y, 0, 1) + 0.1*params.tune_y);
    neutral = clamp(neutral, vec3(0.0), vec3(1.0));
    
    float illuminant = (0.002 * 1.0) * colour_blackbody(vec4(lambda), 2856.0).x;
    vec3 enlarger = 100.0 * mix(
      vec3(1.0),
      thorlabs_filters(lambda),
      neutral);
    float print_illuminant = enlarger.x * enlarger.y * enlarger.z * illuminant;
    float base_density = dye_density.w * dye_density_min_factor_film;
    float base_light = exp2(-base_density * 3.32192809489);
    vec3 factor = sensitivity * print_illuminant * exp2(params.ev_paper) * base_light;

    shared_enlarger_dye_r[tid/4][tid%4] = dye_density.x;
    shared_enlarger_dye_g[tid/4][tid%4] = dye_density.y;
    shared_enlarger_dye_b[tid/4][tid%4] = dye_density.z;
    shared_enlarger_factor_r[tid/4][tid%4] = factor.r;
    shared_enlarger_factor_g[tid/4][tid%4] = factor.g;
    shared_enlarger_factor_b[tid/4][tid%4] = factor.b;
  }
  barrier();
}

void init_enlarger_negative_shared(int paper)
{
  int tid = int(gl_LocalInvocationIndex);
  if (tid < 44)
  {
    shared_enlarger_factor_r[tid/4][tid%4] = 0.0;
    shared_enlarger_factor_g[tid/4][tid%4] = 0.0;
    shared_enlarger_factor_b[tid/4][tid%4] = 0.0;
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_sensitivity, paper));
    vec3 sensitivity = get_sensitivity(tc);

    vec3 neutral = vec3(params.filter_c,
        clamp(params.filter_m, 0, 1) + 0.1*params.tune_m,
        clamp(params.filter_y, 0, 1) + 0.1*params.tune_y);
    neutral = clamp(neutral, vec3(0.0), vec3(1.0));
    
    float illuminant = (0.002 * 1.0) * colour_blackbody(vec4(lambda), 2856.0).x;
    vec3 enlarger = 100.0 * mix(
      vec3(1.0),
      thorlabs_filters(lambda),
      neutral);
    float print_illuminant = enlarger.x * enlarger.y * enlarger.z * illuminant;
    vec3 factor = sensitivity * print_illuminant * exp2(params.ev_paper);
    shared_enlarger_factor_r[tid/4][tid%4] = factor.r;
    shared_enlarger_factor_g[tid/4][tid%4] = factor.g;
    shared_enlarger_factor_b[tid/4][tid%4] = factor.b;
  }
  barrier();
}

void init_scan_shared()
{
  int tid = int(gl_LocalInvocationIndex);
  if (tid < 44)
  {
    shared_scan_dye_r[tid/4][tid%4] = 0.0;
    shared_scan_dye_g[tid/4][tid%4] = 0.0;
    shared_scan_dye_b[tid/4][tid%4] = 0.0;
    shared_scan_factor_r[tid/4][tid%4] = 0.0;
    shared_scan_factor_g[tid/4][tid%4] = 0.0;
    shared_scan_factor_b[tid/4][tid%4] = 0.0;
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    const int paper = s_paper_offset + params.paper;
    const int film  = params.film;
    int dye_stock = (params.process != 1) ? paper : film;
    
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_dye_density, dye_stock));
    vec4 dye_density = texture(img_filmsim, tc);
    dye_density = mix(dye_density, vec4(1000000.0), isnan(dye_density));
    dye_density.xyz *= 3.32192809489;

    vec3 d50 = vec3(0.9642, 1.0000, 0.8251);
    vec4 coeff = fetch_coeff(d50);
    float scan_illuminant = (4.0 / 41.0) * sigmoid_eval(coeff, lambda);
    vec3 cmf = cmf_1931(lambda);
    
    float factor = (params.process != 1) ? dye_density_min_factor_paper : dye_density_min_factor_film;
    float base_density = dye_density.w * factor;
    float base_light = exp2(-base_density * 3.32192809489);
    vec3 factor_vec = scan_illuminant * cmf * base_light;

    shared_scan_dye_r[tid/4][tid%4] = dye_density.x;
    shared_scan_dye_g[tid/4][tid%4] = dye_density.y;
    shared_scan_dye_b[tid/4][tid%4] = dye_density.z;
    shared_scan_factor_r[tid/4][tid%4] = factor_vec.r;
    shared_scan_factor_g[tid/4][tid%4] = factor_vec.g;
    shared_scan_factor_b[tid/4][tid%4] = factor_vec.b;
  }
  barrier();
}

void init_coupler_matrix_shared()
{
  int tid = int(gl_LocalInvocationIndex);
  if (tid == 0)
  {
    mat3 M = mat3(6.0, 4.0, 1.0, 4.0, 6.0, 4.0, 1.0, 4.0, 6.0);
    vec3 amount = vec3(0.1, 0.2, 0.5)*3.0;
    M[0] *= 1.0/11.0 * amount.r * params.couplers;
    M[1] *= 1.0/14.0 * amount.g * params.couplers;
    M[2] *= 1.0/11.0 * amount.b * params.couplers;
    shared_M = M;
    shared_M_sum = M * vec3(1.0);
  }
  barrier();
}

vec3 // returns log_raw
expose_film(vec3 rgb, int film)
{ // film exposure in camera and chemical development (Vectorized spectral loop)
  vec4 coeff = fetch_coeff(rgb);
  vec3 raw = vec3(0.0);
  for(int i=0; i<11; i++)
  {
    vec4 lambda = 380.0 + (vec4(i*4, i*4+1, i*4+2, i*4+3)) * 10.0;
    // x = (coeff.x * lambda + coeff.y) * lambda + coeff.z
    vec4 x = (coeff.x * lambda + coeff.y) * lambda + coeff.z;
    vec4 y = inversesqrt(x * x + 1.0);
    vec4 val = (0.5 * x * y + 0.5) * coeff.w;
    raw.r += dot(val, shared_expose_factor_r[i]);
    raw.g += dot(val, shared_expose_factor_g[i]);
    raw.b += dot(val, shared_expose_factor_b[i]);
  }
  const float log2_log10 = 0.30102999566398114;
  return params.ev_film * log2_log10 + log2(raw+1e-10) * log2_log10;
}

vec3 sigmoid(vec3 x)
{
  vec3 abs_x = abs(x);
  vec3 x2 = abs_x * abs_x;
  vec3 x4 = x2 * x2;
  vec3 xb = x4 * sqrt(abs_x);
  return 0.5 + 0.5*x * exp2(-0.2222222222222222 * log2(xb + 1.0));
}

void sigmoid_both(vec3 x, out vec3 sig, out vec3 sig_d)
{
  // Combined evaluation of sigmoid and its derivative.
  // We reuse abs_x, x2, x4, and xb calculations.
  vec3 abs_x = abs(x);
  vec3 x2 = abs_x * abs_x;
  vec3 x4 = x2 * x2;
  vec3 xb = x4 * sqrt(abs_x);
  vec3 base = xb + 1.0;
  vec3 rcp_p1 = exp2(-0.2222222222222222 * log2(base));
  sig = 0.5 + 0.5 * x * rcp_p1;
  sig_d = 0.5 * rcp_p1 / base;
}


vec3 // returns density_cmy;
develop_film_compute_couplers(vec3 log_raw)
{ // develop film
  // now the couplers. try the following:
  // we have aggregate/measured density curves D(e) where e is the undiffused pixel input
  // and we want reduced ones D_0(e_0) where e_0 will be diffused + inhibited e_0 = e - K*e
  // because we observe D(e) = D_0(e - K*e) on a test strip where the spatial kernel does
  // mainly nothing,  D(e) = D_0(e - M e) where M is the inter-layer diffusion matrix.
  // so we compute D_0(e) as D((I-M)^{-1} e)
  // note that this if at all only holds for *neutral* e (i.e. same channels)
  // other than agx-emulsion, this assumes the couplers are created relative to log exp e, not
  // an initial density (in the sense of a fixed point iteration or so)
#if 0
  // the coupler/inhibitor M*log_raw should diffuse spatially, so we
  // write it out here, split the kernel, blur, and come back in part1.comp
  return shared_M * log_raw;
#else
  return shared_M * sigmoid(log_raw);
#endif
}

vec3 // returns new log_raw
develop_film_correct_exposure(vec3 log_raw, vec3 coupler)
{
  vec3 ep = log_raw - coupler;
  vec3 M_sum = shared_M_sum;

  // Vectorized initial guess for all 3 channels
  vec3 e = (ep + 0.5 * M_sum) / (1.0 - 0.5 * M_sum);

  vec3 sig, sig_d;
  for(int i=0; i<5; i++)
  {
    // Evaluates R, G, and B in parallel
    sigmoid_both(e, sig, sig_d);

    // Vectorized Newton step
    e -= (e - ep - M_sum * sig) / (1.0 - M_sum * sig_d);
  }
  return e;
}

vec3 // returns density_cmy
develop_film(vec3 log_raw, int film, ivec2 ipos, float scale)
{
  const float log_exp_min = -4.0;
  const float log_exp_max =  4.0;
  vec2 tc = vec2(0.0, get_tcy(s_density_curve, film));
  vec3 tcx = clamp((params.gamma_film*log_raw - log_exp_min)/(log_exp_max-log_exp_min), vec3(0.0), vec3(1.0));
  vec3 density_cmy = vec3(
      texture(img_filmsim, vec2(tcx.r, tc.y)).r,
      texture(img_filmsim, vec2(tcx.g, tc.y)).g,
      texture(img_filmsim, vec2(tcx.b, tc.y)).b);
  density_cmy = mix(density_cmy, vec3(0.0), isnan(density_cmy));

  if(scale < 10.0 && params.grain > 0) density_cmy = add_grain(ipos, density_cmy, scale);
  return density_cmy;
}

vec3 // returns log raw
enlarger_expose_negative_to_paper(vec3 rgb)
{ // enlarger: expose scanned transmittances of negative to print paper (Vectorized spectral loop)
  vec3 raw = vec3(0.0);
  vec4 coeff = fetch_coeff(rgb);
  for(int i=0; i<11; i++)
  {
    vec4 lambda = 380.0 + (vec4(i*4, i*4+1, i*4+2, i*4+3)) * 10.0;
    vec4 x = (coeff.x * lambda + coeff.y) * lambda + coeff.z;
    vec4 y = inversesqrt(x * x + 1.0);
    vec4 transmittance = (0.5 * x * y + 0.5) * coeff.w;
    raw.r += dot(transmittance, shared_enlarger_factor_r[i]);
    raw.g += dot(transmittance, shared_enlarger_factor_g[i]);
    raw.b += dot(transmittance, shared_enlarger_factor_b[i]);
  }
  const float log2_log10 = 0.30102999566398114;
  return log2(raw + 1e-10)*log2_log10;
}

vec3 // returns log raw
enlarger_expose_film_to_paper(vec3 density_cmy)
{ // enlarger: expose film to print paper (Vectorized spectral loop)
  vec3 raw = vec3(0.0);
  for(int i=0; i<11; i++)
  {
    vec4 ds = density_cmy.x * shared_enlarger_dye_r[i] + 
              density_cmy.y * shared_enlarger_dye_g[i] + 
              density_cmy.z * shared_enlarger_dye_b[i];
    vec4 light = exp2(-ds);
    raw.r += dot(light, shared_enlarger_factor_r[i]);
    raw.g += dot(light, shared_enlarger_factor_g[i]);
    raw.b += dot(light, shared_enlarger_factor_b[i]);
  }
  const float log2_log10 = 0.30102999566398114;
  return log2(raw + 1e-10)*log2_log10;
}

vec3 // return density_cmy
develop_print(vec3 log_raw)
{ // interpolate log exposure to density again
  const float log_exp_min = -4.0;
  const float log_exp_max =  4.0;
  const int paper = s_paper_offset + params.paper;
  vec2 tc = vec2(0.0, get_tcy(s_density_curve, paper));
  vec3 tcx = clamp((params.gamma_paper*log_raw - log_exp_min)/(log_exp_max-log_exp_min), vec3(0.0), vec3(1.0));
  return vec3(
      texture(img_filmsim, vec2(tcx.r, tc.y)).r,
      texture(img_filmsim, vec2(tcx.g, tc.y)).g,
      texture(img_filmsim, vec2(tcx.b, tc.y)).b);
}

vec3 // return rgb linear rec2020
scan(vec3 density_cmy)
{ // convert cmy density to spectral (Vectorized spectral loop)
  vec3 raw = vec3(0.0);
  for(int i=0; i<11; i++)
  {
    vec4 ds = density_cmy.x * shared_scan_dye_r[i] + 
              density_cmy.y * shared_scan_dye_g[i] + 
              density_cmy.z * shared_scan_dye_b[i];
    vec4 light = exp2(-ds);
    raw.r += dot(light, shared_scan_factor_r[i]);
    raw.g += dot(light, shared_scan_factor_g[i]);
    raw.b += dot(light, shared_scan_factor_b[i]);
  }
  raw = clamp(raw, vec3(0.0), vec3(14.0));
  return XYZ_to_rec2020(raw);
}
