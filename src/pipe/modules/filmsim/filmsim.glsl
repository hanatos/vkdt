// helper functions for film simulation.
// this is all more or less a straight port of agx-emulsion
// https://github.com/andreavolpato/agx-emulsion
// so as derivative work i suppose this makes it GPL-v3

// number of spectral samples/wavelengths when integrating
#define SN 40

const vec4 lambda_arr[11] = vec4[](
  vec4(380.0, 390.0, 400.0, 410.0),
  vec4(420.0, 430.0, 440.0, 450.0),
  vec4(460.0, 470.0, 480.0, 490.0),
  vec4(500.0, 510.0, 520.0, 530.0),
  vec4(540.0, 550.0, 560.0, 570.0),
  vec4(580.0, 590.0, 600.0, 610.0),
  vec4(620.0, 630.0, 640.0, 650.0),
  vec4(660.0, 670.0, 680.0, 690.0),
  vec4(700.0, 710.0, 720.0, 730.0),
  vec4(740.0, 750.0, 760.0, 770.0),
  vec4(780.0, 790.0, 800.0, 810.0)
);

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

vec3 simplex3_2d(vec2 P)
{
  // Skewing factors for 2D
  const float F2 = 0.366025403784439;  // 0.5*(sqrt(3.0)-1.0)
  const float G2 = 0.211324865405187;  // (3.0-sqrt(3.0))/6.0
  
  // Skew the input space to determine which simplex cell we're in
  vec2 s = P + dot(P, vec2(F2));
  ivec2 i = ivec2(floor(s));
  vec2 p0 = P - (vec2(i) - dot(vec2(i), vec2(G2)));
  
  // Determine simplex offset
  ivec2 i1 = (p0.x > p0.y) ? ivec2(1, 0) : ivec2(0, 1);
  
  // Offsets for middle and last corners
  vec2 p1 = p0 - vec2(i1) + G2;
  vec2 p2 = p0 - 1.0 + 2.0 * G2;
  
  // Hash function to get 3 random gradients per corner
  uint kx = 1103515245U;
  uint ky = 1234567891U;
  uint m = 2654435769U;
  
  // Hashing corners
  uvec3 cx = uvec3(i.x, i.x + i1.x, i.x + 1) * kx;
  uvec3 cy = uvec3(i.y, i.y + i1.y, i.y + 1) * ky;
  uvec3 c = cx ^ cy;
  
  // Mix
  c ^= (c >> 16u); c *= m; c ^= (c >> 15u);
  
  uvec3 c_r = c;
  uvec3 c_g = c * 1664525U + 1013904223U;
  uvec3 c_b = c_g * 1664525U + 1013904223U;
  
  // Helper macro to get vec2 gradients in [-1, 1]
  #define GA(h) (vec2(uintBitsToFloat((h >> 9u) | 0x3f800000u), uintBitsToFloat(((h << 16u) >> 9u) | 0x3f800000u)) * 2.0 - 3.0)
  #define NORM(g) (g * inversesqrt(dot(g, g) + 1e-6))
  
  vec2 g0_r = NORM(GA(c_r.x)); vec2 g0_g = NORM(GA(c_g.x)); vec2 g0_b = NORM(GA(c_b.x));
  vec2 g1_r = NORM(GA(c_r.y)); vec2 g1_g = NORM(GA(c_g.y)); vec2 g1_b = NORM(GA(c_b.y));
  vec2 g2_r = NORM(GA(c_r.z)); vec2 g2_g = NORM(GA(c_g.z)); vec2 g2_b = NORM(GA(c_b.z));
  
  #undef NORM
  #undef GA
  
  // Calculate unscaled influence of each corner (using 0.6 to avoid flat zero boundaries)
  vec3 w = max(0.6 - vec3(dot(p0, p0), dot(p1, p1), dot(p2, p2)), 0.0);
  vec3 w4 = w * w; w4 *= w4;
  
  // Dot products
  vec3 d_r = vec3(dot(g0_r, p0), dot(g1_r, p1), dot(g2_r, p2));
  vec3 d_g = vec3(dot(g0_g, p0), dot(g1_g, p1), dot(g2_g, p2));
  vec3 d_b = vec3(dot(g0_b, p0), dot(g1_b, p1), dot(g2_b, p2));
  
  // Final noise (99.0 scales the normalized gradients + 0.6 weight to [-1.0, 1.0])
  return vec3(dot(w4, d_r), dot(w4, d_g), dot(w4, d_b)) * 99.0;
}

vec3 add_grain(ivec2 ipos, vec3 density, float scale)
{
  const float density_max = 3.3;
  vec3 np = clamp(density / density_max, 0.0, 1.0);

  vec3 pd0 = clamp(np * 10.0, 0.0, 1.0);
  vec3 pd1 = clamp(np * 5.0 - 0.5, 0.0, 1.0);
  vec3 pd2 = clamp(np * 1.42857 - 0.42857, 0.0, 1.0);

  float u = pow(max(0.0, params.grain_uniformity), 0.333333);
  vec3 var_base = 1.6008 * pd0 * max(1.0 - pd0 * u, 0.0) + 
                  0.4356 * pd1 * max(1.0 - pd1 * u, 0.0) + 
                  0.3267 * pd2 * max(1.0 - pd2 * u, 0.0);
  vec3 amp_ch = sqrt(max(vec3(0.0), var_base * vec3(1.0, 0.8, 3.0)));

  float g2 = params.grain_size * params.grain_size;
  float k = 0.36 / g2;
  float amp = 0.04 * g2 / scale;

  vec2 pos = mat2(0.98006, -0.198669, 0.198669, 0.98006) * (vec2(ipos) * scale + (global.hash*1e-6 + global.frame*133.7));
  
  vec3 noise = simplex3_2d(pos * k) + 0.57 * simplex3_2d(pos * k * 0.61 + 12.3);
  noise = mix(vec3(noise.g), noise, 0.6) * 0.869;

  return max(vec3(0.0), density + amp * amp_ch * noise);
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
  if (tid < 11)
  {
    shared_expose_factor_r[tid] = vec4(0.0);
    shared_expose_factor_g[tid] = vec4(0.0);
    shared_expose_factor_b[tid] = vec4(0.0);
  }
  barrier();
  if (tid <= 35)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 2.0 + 0.5) / 256.0, get_tcy(s_sensitivity, film));
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
  if (tid < 11)
  {
    shared_enlarger_dye_r[tid] = vec4(0.0);
    shared_enlarger_dye_g[tid] = vec4(0.0);
    shared_enlarger_dye_b[tid] = vec4(0.0);
    shared_enlarger_factor_r[tid] = vec4(0.0);
    shared_enlarger_factor_g[tid] = vec4(0.0);
    shared_enlarger_factor_b[tid] = vec4(0.0);
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 2.0 + 0.5) / 256.0, get_tcy(s_sensitivity, paper));
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
  if (tid < 11)
  {
    shared_enlarger_factor_r[tid] = vec4(0.0);
    shared_enlarger_factor_g[tid] = vec4(0.0);
    shared_enlarger_factor_b[tid] = vec4(0.0);
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 2.0 + 0.5) / 256.0, get_tcy(s_sensitivity, paper));
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
  if (tid < 11)
  {
    shared_scan_dye_r[tid] = vec4(0.0);
    shared_scan_dye_g[tid] = vec4(0.0);
    shared_scan_dye_b[tid] = vec4(0.0);
    shared_scan_factor_r[tid] = vec4(0.0);
    shared_scan_factor_g[tid] = vec4(0.0);
    shared_scan_factor_b[tid] = vec4(0.0);
  }
  barrier();
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    const int paper = s_paper_offset + params.paper;
    const int film  = params.film;
    int dye_stock = (params.process != 1) ? paper : film;
    
    vec2 tc = vec2((tid * 2.0 + 0.5) / 256.0, get_tcy(s_dye_density, dye_stock));
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
  [[unroll]]
  // envelope(w >= 730nm) = 1000.0 * 1.0 * (1.0 - 1.0) = 0.0
  // lambda_arr[9] = vec4(740, 750, 760, 770)
  for(int i=0; i<9; i++)
  {
    vec4 lambda = lambda_arr[i];
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
  if (params.couplers <= 0.0) return log_raw;

  vec3 ep = log_raw - coupler;
  vec3 M_sum = shared_M_sum;

  // Vectorized initial guess for all 3 channels
  vec3 e = (ep + 0.5 * M_sum) / (1.0 - 0.5 * M_sum);

  vec3 sig, sig_d;
  [[unroll]]
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
  [[unroll]]
  for(int i=0; i<10; i++)
  {
    vec4 lambda = lambda_arr[i];
    vec4 x = (coeff.x * lambda + coeff.y) * lambda + coeff.z;
    vec4 y = inversesqrt(x * x + 1.0);
    vec4 transmittance = (0.5 * x * y + 0.5) * coeff.w;
    raw.r += dot(transmittance, shared_enlarger_factor_r[i]);
    raw.g += dot(transmittance, shared_enlarger_factor_g[i]);
    raw.b += dot(transmittance, shared_enlarger_factor_b[i]);
  }
  { // i=10 (lambda_arr[10] = vec4(780, 790, 800, 810))
    // tid <= 40 evaluates up to lambda=780nm
    float lambda = lambda_arr[10].x;
    float x = (coeff.x * lambda + coeff.y) * lambda + coeff.z;
    float y = inversesqrt(x * x + 1.0);
    float transmittance = (0.5 * x * y + 0.5) * coeff.w;
    raw.r += transmittance * shared_enlarger_factor_r[10].x;
    raw.g += transmittance * shared_enlarger_factor_g[10].x;
    raw.b += transmittance * shared_enlarger_factor_b[10].x;
  }
  const float log2_log10 = 0.30102999566398114;
  return log2(raw + 1e-10)*log2_log10;
}

vec3 // returns log raw
enlarger_expose_film_to_paper(vec3 density_cmy)
{ // enlarger: expose film to print paper (Vectorized spectral loop)
  vec3 raw = vec3(0.0);
  [[unroll]]
  for(int i=0; i<10; i++)
  {
    vec4 ds = density_cmy.x * shared_enlarger_dye_r[i] + 
              density_cmy.y * shared_enlarger_dye_g[i] + 
              density_cmy.z * shared_enlarger_dye_b[i];
    vec4 light = exp2(-ds);
    raw.r += dot(light, shared_enlarger_factor_r[i]);
    raw.g += dot(light, shared_enlarger_factor_g[i]);
    raw.b += dot(light, shared_enlarger_factor_b[i]);
  }
  { // i=10 (lambda_arr[10] = vec4(780, 790, 800, 810))
    // tid <= 40 evaluates up to lambda=780nm
    float ds = density_cmy.x * shared_enlarger_dye_r[10].x + 
               density_cmy.y * shared_enlarger_dye_g[10].x + 
               density_cmy.z * shared_enlarger_dye_b[10].x;
    float light = exp2(-ds);
    raw.r += light * shared_enlarger_factor_r[10].x;
    raw.g += light * shared_enlarger_factor_g[10].x;
    raw.b += light * shared_enlarger_factor_b[10].x;
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
  [[unroll]]
  for(int i=0; i<10; i++)
  {
    vec4 ds = density_cmy.x * shared_scan_dye_r[i] + 
              density_cmy.y * shared_scan_dye_g[i] + 
              density_cmy.z * shared_scan_dye_b[i];
    vec4 light = exp2(-ds);
    raw.r += dot(light, shared_scan_factor_r[i]);
    raw.g += dot(light, shared_scan_factor_g[i]);
    raw.b += dot(light, shared_scan_factor_b[i]);
  }
  { // i=10 (lambda_arr[10] = vec4(780, 790, 800, 810))
    // tid <= 40 evaluates up to lambda=780nm
    float ds = density_cmy.x * shared_scan_dye_r[10].x + 
               density_cmy.y * shared_scan_dye_g[10].x + 
               density_cmy.z * shared_scan_dye_b[10].x;
    float light = exp2(-ds);
    raw.r += light * shared_scan_factor_r[10].x;
    raw.g += light * shared_scan_factor_g[10].x;
    raw.b += light * shared_scan_factor_b[10].x;
  }
  raw = clamp(raw, vec3(0.0), vec3(14.0));
  return XYZ_to_rec2020(raw);
}
