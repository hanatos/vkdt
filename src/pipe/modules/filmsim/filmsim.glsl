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

vec3 hash32(vec2 p)
{
  vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
  p3 += dot(p3, p3.yxz + 33.33);
  return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 add_grain(ivec2 ipos, vec3 density, float scale)
{
  const float density_max = 3.3;
  vec3 np = clamp(density / density_max, 0.0, 1.0);

  // Sublayer probability of development (density fractions: Fast 10%, Mid 20%, Slow 70%)
  vec3 p_dev_0 = clamp(np * 10.0, 0.0, 1.0);
  vec3 p_dev_1 = clamp(np * 5.0 - 0.5, 0.0, 1.0);
  vec3 p_dev_2 = clamp(np * 1.42857 - 0.42857, 0.0, 1.0);

  // Uniformity mapped to spektrafilm defaults (0.97, 0.97, 0.99)
  float u_scalar = pow(max(0.0, params.grain_uniformity), 0.333333);
  vec3 u = clamp(vec3(0.97, 0.97, 0.99) * u_scalar, 0.0, 1.0);
  
  // spektrafilm agx_particle_scale = (0.8, 1.0, 2.0)
  vec3 ch_scale = vec3(0.8, 1.0, 2.0);

  // Exact spektrafilm layer variances: Var = p * (1 - p * u)
  vec3 var_0 = p_dev_0 * max(1.0 - p_dev_0 * u, 0.0);
  vec3 var_1 = p_dev_1 * max(1.0 - p_dev_1 * u, 0.0);
  vec3 var_2 = p_dev_2 * max(1.0 - p_dev_2 * u, 0.0);

  // Optical Density of a single particle
  // od_particle = density_max_total * agx_particle_area_um2 * ch_scale * sl_scale / pixel_area_um2
  // 3.3 * 0.2 * ch_scale * sl_scale / 100.0 = 0.0066 * ch_scale * sl_scale
  vec3 od_0 = 0.0066 * ch_scale * 2.5;
  vec3 od_1 = 0.0066 * ch_scale * 1.0;
  vec3 od_2 = 0.0066 * ch_scale * 0.5;

  // The true standard deviation in density space is sqrt(od_particle * density_max_fraction * Var)
  // Spektrafilm fractions: Fast=0.1, Mid=0.2, Slow=0.7
  // density_max_fraction = 3.3 * fraction
  vec3 std_0 = sqrt(od_0 * (3.3 * 0.1) * var_0);
  vec3 std_1 = sqrt(od_1 * (3.3 * 0.2) * var_1);
  vec3 std_2 = sqrt(od_2 * (3.3 * 0.7) * var_2);

  // Blur Sigma calculations
  // spektrafilm per-layer dye cloud blur: sigma_layer = blur_dye_clouds_um * sqrt(od_particle)
  // blur_dye_clouds_um defaults to 1.0
  vec3 sigma_sq_layer_0 = 1.0 * 1.0 * od_0;
  vec3 sigma_sq_layer_1 = 1.0 * 1.0 * od_1;
  vec3 sigma_sq_layer_2 = 1.0 * 1.0 * od_2;

  // Global blur (defaults to 0.65)
  float global_blur_sq = 0.65 * 0.65;

  // Total variance of the combined Gaussian blurs: sigma_total^2 = sigma_layer^2 + sigma_global^2
  vec3 sigma_sq_0 = sigma_sq_layer_0 + global_blur_sq;
  vec3 sigma_sq_1 = sigma_sq_layer_1 + global_blur_sq;
  vec3 sigma_sq_2 = sigma_sq_layer_2 + global_blur_sq;

  // Inverse variance for spatial Gaussian weights: 1 / (2 * sigma^2)
  // Multiply by grain_size^2 to allow UI scaling of the blur radius
  float g_scale_sq = params.grain_size * params.grain_size;
  // We use coordinate scaling to allow large grain sizes with a constant 4x4 loop.
  // The noise coordinates are scaled by 1/grain_size, effectively making the "particles" larger.
  // We cap the scaling at 1.0 to preserve the original 1:1 pixel grain behavior for small sizes.
  float coord_scale = max(1.0, params.grain_size);
  float inv_coord_scale = 1.0 / coord_scale;

  vec3 inv_2sq_0 = 1.0 / (2.0 * sigma_sq_0 * g_scale_sq * inv_coord_scale * inv_coord_scale);
  vec3 inv_2sq_1 = 1.0 / (2.0 * sigma_sq_1 * g_scale_sq * inv_coord_scale * inv_coord_scale);
  vec3 inv_2sq_2 = 1.0 / (2.0 * sigma_sq_2 * g_scale_sq * inv_coord_scale * inv_coord_scale);

  vec2 seed_offset = vec2(global.hash*1e-6 + fract(float(global.frame)*0.1337)*1000.0);
  // Scale the noise coordinates. 'scale' is for resolution/downsampling.
  vec2 pos = mat2(0.98006, -0.198669, 0.198669, 0.98006) * (vec2(ipos) * scale * inv_coord_scale + seed_offset);
  
  vec2 p = floor(pos);
  vec2 f = fract(pos);

  vec3 acc0 = vec3(0.0), acc1 = vec3(0.0), acc2 = vec3(0.0);
  vec3 wsq0 = vec3(0.0), wsq1 = vec3(0.0), wsq2 = vec3(0.0);

  vec3 inv_2sq_0_l2e = inv_2sq_0 * 1.44269504;
  vec3 inv_2sq_1_l2e = inv_2sq_1 * 1.44269504;
  vec3 inv_2sq_2_l2e = inv_2sq_2 * 1.44269504;

  [[loop]]
  for (int y = -1; y <= 2; y++) {
    float dy = float(y) - f.y;
    float dy2 = dy * dy;
    [[loop]]
    for (int x = -1; x <= 2; x++) {
      float dx = float(x) - f.x;
      float dist_sq = dx * dx + dy2;
      float window = max(0.0, 1.0 - dist_sq * 0.125);
      if (window <= 0.0) continue;

      vec2 hash_pos = p + vec2(float(x), float(y));
      vec3 n0 = hash32(hash_pos) * 2.0 - 1.0;
      vec3 n1 = hash32(hash_pos + 13.37) * 2.0 - 1.0;
      vec3 n2 = hash32(hash_pos + 42.0) * 2.0 - 1.0;

      vec3 w0 = exp2(-dist_sq * inv_2sq_0_l2e) * window;
      vec3 w1 = exp2(-dist_sq * inv_2sq_1_l2e) * window;
      vec3 w2 = exp2(-dist_sq * inv_2sq_2_l2e) * window;

      acc0 += n0 * w0; acc1 += n1 * w1; acc2 += n2 * w2;
      wsq0 += w0 * w0; wsq1 += w1 * w1; wsq2 += w2 * w2;
    }
  }

  // Multiply by sqrt(3.0) (~1.732) to map uniform noise [-1, 1] variance to 1.0
  // Then divide by sqrt(sum(w^2)) to mathematically preserve standard deviation through the blur
  vec3 blur_n0 = acc0 * 1.73205 * inversesqrt(max(wsq0, 1e-6));
  vec3 blur_n1 = acc1 * 1.73205 * inversesqrt(max(wsq1, 1e-6));
  vec3 blur_n2 = acc2 * 1.73205 * inversesqrt(max(wsq2, 1e-6));

  // Combine layers (each noise field is standard normal N(0,1), so they scale exactly by std_X)
  vec3 final_noise = blur_n0 * std_0 + blur_n1 * std_1 + blur_n2 * std_2;

  return clamp(density + final_noise, vec3(0.0), vec3(2.0));
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
shared vec3 shared_preflash;
shared vec3 shared_pf_acc[64];

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
    // TODO this shall include skin spectra! * scene illuminant, D65 say
    vec2 tc = vec2((tid * 2.0 + 0.5) / 256.0, get_tcy(s_sensitivity, film));
    vec3 sensitivity = get_sensitivity(tc);
    // avoid some metameric madness film vs cie cmf: cut off wavelength ranges
    // that the spectral upsampling doesn't care about (outside the XYZ support)
    float env = envelope(lambda);
    float pdf = 2.0 * 41.0; // our integration is off by -1ev from agx, so 2.0 here
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
    float base_density = dye_density.w * dye_density_min_factor_film;
    float base_light = exp2(-base_density * 3.32192809489);
    float common_light = illuminant * base_light * exp2(params.ev_paper) * 1000000.0;

    // pretty coarse manual fit to thorlabs filters:
    // (lamp filters are transmittances 0..100%)
    vec3 enlarger = mix(vec3(1.0), thorlabs_filters(lambda), neutral);
    float print_illuminant = enlarger.x * enlarger.y * enlarger.z * common_light;
    vec3 factor = sensitivity * print_illuminant;

    vec3 pf_factor = vec3(0.0);
    if (params.preflash > 0)
    {
      vec3 pf_neutral = vec3(params.filter_c,
          clamp(params.filter_m, 0, 1) + 0.1*params.tune_m + params.pf_m,
          clamp(params.filter_y, 0, 1) + 0.1*params.tune_y + params.pf_y);
      pf_neutral = clamp(pf_neutral, vec3(0.0), vec3(1.0));
      vec3 pf_enlarger = mix(vec3(1.0), thorlabs_filters(lambda), pf_neutral);
      float pf_print_illuminant = pf_enlarger.x * pf_enlarger.y * pf_enlarger.z * common_light * exp2(params.pf_ev);
      pf_factor = sensitivity * pf_print_illuminant;
    }
    shared_pf_acc[tid] = pf_factor;

    shared_enlarger_dye_r[tid/4][tid%4] = dye_density.x;
    shared_enlarger_dye_g[tid/4][tid%4] = dye_density.y;
    shared_enlarger_dye_b[tid/4][tid%4] = dye_density.z;
    shared_enlarger_factor_r[tid/4][tid%4] = factor.r;
    shared_enlarger_factor_g[tid/4][tid%4] = factor.g;
    shared_enlarger_factor_b[tid/4][tid%4] = factor.b;
  }
  else if (tid < 64) shared_pf_acc[tid] = vec3(0.0);
  barrier();
  // reduction for preflash
  if (tid < 32) shared_pf_acc[tid] += shared_pf_acc[tid + 32];
  barrier();
  if (tid < 16) shared_pf_acc[tid] += shared_pf_acc[tid + 16];
  barrier();
  if (tid < 8)  shared_pf_acc[tid] += shared_pf_acc[tid + 8];
  barrier();
  if (tid < 4)  shared_pf_acc[tid] += shared_pf_acc[tid + 4];
  barrier();
  if (tid < 2)  shared_pf_acc[tid] += shared_pf_acc[tid + 2];
  barrier();
  if (tid < 1)  shared_preflash = (shared_pf_acc[0] + shared_pf_acc[1]);
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
    // TODO should this use reflectances and then multiply the illuminant?
    // XXX does it contain D65??
    vec2 tc = vec2((tid * 2.0 + 0.5) / 256.0, get_tcy(s_sensitivity, paper));
    vec3 sensitivity = get_sensitivity(tc);

    vec3 neutral = vec3(params.filter_c,
        clamp(params.filter_m, 0, 1) + 0.1*params.tune_m,
        clamp(params.filter_y, 0, 1) + 0.1*params.tune_y);
    neutral = clamp(neutral, vec3(0.0), vec3(1.0));
    
    float illuminant = (0.002 * 1.0) * 1000000.0 * colour_blackbody(vec4(lambda), 2856.0).x;
    float common_light = illuminant * exp2(params.ev_paper);

    // pretty coarse manual fit to thorlabs filters:
    // (lamp filters are transmittances 0..100%)
    vec3 enlarger = mix(vec3(1.0), thorlabs_filters(lambda), neutral);
    float print_illuminant = enlarger.x * enlarger.y * enlarger.z * common_light;
    vec3 factor = sensitivity * print_illuminant;

    vec3 pf_factor = vec3(0.0);
    if (params.preflash > 0)
    {
      vec3 pf_neutral = vec3(params.filter_c,
          clamp(params.filter_m, 0, 1) + 0.1*params.tune_m + params.pf_m,
          clamp(params.filter_y, 0, 1) + 0.1*params.tune_y + params.pf_y);
      pf_neutral = clamp(pf_neutral, vec3(0.0), vec3(1.0));
      vec3 pf_enlarger = mix(vec3(1.0), thorlabs_filters(lambda), pf_neutral);
      float pf_print_illuminant = pf_enlarger.x * pf_enlarger.y * pf_enlarger.z * common_light * exp2(params.pf_ev);
      pf_factor = sensitivity * pf_print_illuminant;
    }
    shared_pf_acc[tid] = pf_factor;

    shared_enlarger_factor_r[tid/4][tid%4] = factor.r;
    shared_enlarger_factor_g[tid/4][tid%4] = factor.g;
    shared_enlarger_factor_b[tid/4][tid%4] = factor.b;
  }
  else if (tid < 64) shared_pf_acc[tid] = vec3(0.0);
  barrier();
  // reduction for preflash
  if (tid < 32) shared_pf_acc[tid] += shared_pf_acc[tid + 32];
  barrier();
  if (tid < 16) shared_pf_acc[tid] += shared_pf_acc[tid + 16];
  barrier();
  if (tid < 8)  shared_pf_acc[tid] += shared_pf_acc[tid + 8];
  barrier();
  if (tid < 4)  shared_pf_acc[tid] += shared_pf_acc[tid + 4];
  barrier();
  if (tid < 2)  shared_pf_acc[tid] += shared_pf_acc[tid + 2];
  barrier();
  if (tid < 1)  shared_preflash = (shared_pf_acc[0] + shared_pf_acc[1]);
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
    dye_density = mix(dye_density, vec4(1000.0), isnan(dye_density));
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
  rgb = max(vec3(5e-4), rgb); // clamp dark noise to avoid nan in the process
  // vec4 d65cf = fetch_coeff(vec3(1));
  // this upsamples *reflectances*, i.e. 111 is equal energy not D65
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

  [[branch]] if(scale < 10.0 && params.grain > 0) density_cmy = add_grain(ipos, density_cmy, scale);
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
  return log2(raw + shared_preflash + 1e-10)*log2_log10;
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
  return log2(raw + shared_preflash + 1e-10)*log2_log10;
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
