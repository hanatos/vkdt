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

int binom(inout uint seed, int n, float p)
{ // gaussian approximation, good for large n*p
  float u = n*p;
  float s = sqrt(n*p*(1.0-p));
  vec2 r = vec2(mrand(seed), mrand(seed));
  return max(0, int(u + s * warp_gaussian(r).x));
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

vec2 hash(in ivec2 p)
{
  // Optimized PCG-style hash to minimize math instruction overhead
  uvec2 q = uvec2(p);
  q *= uvec2(1597334677U, 3812015801U);
  q = (q.x ^ q.y) * uvec2(1597334677U, 3812015801U);
  return -1.0 + 2.0 * vec2(q) * (1.0 / 4294967296.0);
}

float noise(in vec2 p)
{
  float c = 0.98006, s = 0.198669;
  mat2 R = mat2(c,s,-s,c);
  ivec2 i = ivec2(floor(R*p));
  vec2 f = fract(R*p);

  // quintic interpolation
  vec2 u = f*f*f*(f*(f*6.0-15.0)+10.0);
  vec2 du = 30.0*f*f*(f*(f-2.0)+1.0);

  vec2 ga = hash(i + ivec2(0,0));
  vec2 gb = hash(i + ivec2(1,0));
  vec2 gc = hash(i + ivec2(0,1));
  vec2 gd = hash(i + ivec2(1,1));

  float va = dot(ga, f - vec2(0.0,0.0));
  float vb = dot(gb, f - vec2(1.0,0.0));
  float vc = dot(gc, f - vec2(0.0,1.0));
  float vd = dot(gd, f - vec2(1.0,1.0));

  return va + u.x*(vb-va) + u.y*(vc-va) + u.x*u.y*(va-vb-vc+vd);   // value
}

float noisef(in vec2 p)
{
  return noise(p) + .57*noise(0.61*p);
}

// separate density into three layers of grains (coarse, mid, fine) that sum up
// to the max density. lower density values are taken mostly by coarse grains.
// use perlin gradient noise / correlated to simulate the three layers (represents non-uniformity of grain counts/pixel)
// simulate a poisson distribution x3: are the grains turned?
// expectation should be =density, i.e. n*p + n*p + n*p = density for the three layers
vec3 add_grain(ivec2 ipos, vec3 density, float scale)
{
  ipos = ivec2(vec2(ipos)*scale); // if not processing full size (scale = 1), adjust here
  int n_grains_per_pixel = 50; //1000; // from artic's python. starts to look very good!
  int grain_non_uniformity = int((1.0-params.grain_uniformity)*n_grains_per_pixel);
  float grain_size = params.grain_size;
  float density_max = 3.3;
  vec3 res = vec3(0.0);
  uint seed = 123456789*ipos.x + 1333337*ipos.y + (global.frame+global.hash)*100000;
  
  // Precomputed 1.0 / (particle_scale_col[col] * particle_scale_lay[layer])
  const float c_table[3][3] = {
    {0.5, 1.25, 2.5},  // col = 0
    {0.4, 1.0,  2.0},  // col = 1
    {0.2, 0.5,  1.0}   // col = 2
  };

  for(int col=0;col<3;col++)
  {
    float np = density[col] / density_max; // expectation of normalised number of developed grains
    float sat = clamp(1.5*np, 0, 1);
    vec3 doff = vec3(0.10, 0.20, 0.7);
    for(int layer=0;layer<3;layer++)
    { // from coarse to fine layers
      float npl = min(doff[layer], np);
      np -= npl;
      float c = c_table[col][layer];
      vec2 tc = c/grain_size * 0.3*vec2(ipos + global.hash*0.000001 + global.frame*133.7 + 1000*col);
      int n = int(n_grains_per_pixel*c*scale*scale);
      // float sat = clamp(npl / doff[layer], 0, 1);
      // int r = int(grain_non_uniformity*c*noise(tc)*1*sat);
      int r = int(grain_non_uniformity*c*noisef(tc)*4.0*sat*(1.0-sat));
      // int nr = binom(seed, int(n/sat), sat);
      int nr = max(0, n+r);
      float p = npl;
      // if(layer!=2) res[col] += density_max * npl; else
      res[col] += density_max * nr*p/float(n); // this is using the expected value of developed grains directly
      // now simulate whether these grains actually turn:
      // res[col] += density_max * binom(seed, nr, p)/float(n);
    }
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
// Since spectral variables are uniform across all pixels, we load them once per workgroup
// to bypass redundant texture lookups and transcendental function evaluation.
shared vec3 shared_expose_factor[41];
shared vec3 shared_enlarger_factor[41];
shared vec4 shared_enlarger_dye_density[41];
shared vec4 shared_scan_dye_density[41];
shared vec3 shared_scan_illuminant_cmf[41];

void init_expose_film_shared(int film)
{
  int tid = int(gl_LocalInvocationID.y * 8 + gl_LocalInvocationID.x);
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_sensitivity, film));
    vec3 sensitivity = get_sensitivity(tc);
    float env = envelope(lambda);
    float pdf = 2.0 * 41.0;
    shared_expose_factor[tid] = sensitivity * env / pdf;
  }
  barrier();
}

void init_enlarger_shared(int film, int paper)
{
  int tid = int(gl_LocalInvocationID.y * 8 + gl_LocalInvocationID.x);
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_sensitivity, paper));
    vec3 sensitivity = get_sensitivity(tc);
    
    tc.y = get_tcy(s_dye_density, film);
    vec4 dye_density = texture(img_filmsim, tc);
    dye_density = mix(dye_density, vec4(1000000.0), isnan(dye_density));
    shared_enlarger_dye_density[tid] = dye_density;

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
    shared_enlarger_factor[tid] = sensitivity * print_illuminant * exp2(params.ev_paper) * base_light;
  }
  barrier();
}

void init_enlarger_negative_shared(int paper)
{
  int tid = int(gl_LocalInvocationID.y * 8 + gl_LocalInvocationID.x);
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
    shared_enlarger_factor[tid] = sensitivity * print_illuminant * exp2(params.ev_paper);
  }
  barrier();
}

void init_scan_shared()
{
  int tid = int(gl_LocalInvocationID.y * 8 + gl_LocalInvocationID.x);
  if (tid <= 40)
  {
    float lambda = 380.0 + tid * 10.0;
    const int paper = s_paper_offset + params.paper;
    const int film  = params.film;
    int dye_stock = (params.process != 1) ? paper : film;
    
    vec2 tc = vec2((tid * 80.0 / 40.0 + 0.5) / 256.0, get_tcy(s_dye_density, dye_stock));
    vec4 dye_density = texture(img_filmsim, tc);
    dye_density = mix(dye_density, vec4(1000000.0), isnan(dye_density));
    shared_scan_dye_density[tid] = dye_density;

    vec3 d50 = vec3(0.9642, 1.0000, 0.8251);
    vec4 coeff = fetch_coeff(d50);
    float scan_illuminant = (4.0 / 41.0) * sigmoid_eval(coeff, lambda);
    vec3 cmf = cmf_1931(lambda);
    
    float factor = (params.process != 1) ? dye_density_min_factor_paper : dye_density_min_factor_film;
    float base_density = dye_density.w * factor;
    float base_light = exp2(-base_density * 3.32192809489);
    shared_scan_illuminant_cmf[tid] = scan_illuminant * cmf * base_light;
  }
  barrier();
}

shared mat3 shared_M;
shared vec3 shared_M_sum;

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
{ // film exposure in camera and chemical development (Optimized to read precomputed shared_expose_factor)
  vec4 coeff = fetch_coeff(rgb);
  vec3 raw = vec3(0.0);
  for(int l=0;l<=SN;l++)
  {
    float lambda = 380.0 + l*400.0/SN;
    float val = sigmoid_eval(coeff, lambda);
    raw += shared_expose_factor[l] * val;
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
{ // enlarger: expose scanned transmittances of negative to print paper (Optimized to read precomputed shared_enlarger_factor)
  vec3 raw = vec3(0.0);
  vec4 coeff = fetch_coeff(rgb);
  for(int l=0;l<=SN;l++)
  {
    float lambda = 380.0 + l*400.0/SN;
    float transmittance = sigmoid_eval(coeff, lambda);
    raw += shared_enlarger_factor[l] * transmittance;
  }
  const float log2_log10 = 0.30102999566398114;
  return log2(raw + 1e-10)*log2_log10;
}

vec3 // returns log raw
enlarger_expose_film_to_paper(vec3 density_cmy)
{ // enlarger: expose film to print paper (Optimized to read precomputed shared_enlarger_* buffers)
  vec3 raw = vec3(0.0);
  for(int l=0;l<=SN;l++)
  {
    vec3 dye_density_xyz = shared_enlarger_dye_density[l].xyz;
    float density_spectral = dot(density_cmy, dye_density_xyz);

    float light = exp2(-density_spectral * 3.32192809489);
    raw += shared_enlarger_factor[l] * light;
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
{ // convert cmy density to spectral (Optimized to read precomputed shared_scan_* buffers)
  vec3 raw = vec3(0.0);
  for(int l=0;l<=SN;l++)
  {
    vec3 dye_density_xyz = shared_scan_dye_density[l].xyz;
    float density_spectral = dot(density_cmy, dye_density_xyz);
    float light = exp2(-density_spectral * 3.32192809489);
    raw += light * shared_scan_illuminant_cmf[l];
  }
  raw = clamp(raw, vec3(0.0), vec3(14.0));
  return XYZ_to_rec2020(raw);
}
