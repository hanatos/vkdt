// helper functions for film simulation.
// this is all more or less a straight port of agx-emulsion
// https://github.com/andreavolpato/agx-emulsion
// so as derivative work i suppose this makes it GPL-v3

// number of spectral samples/wavelengths when integrating
#define SN 40

int binom(inout uint seed, int n, float p)
{ // gaussian approximation, good for large n*p
  float u = n*p;
  float s = sqrt(n*p*(1.0-p));
  vec2 r = vec2(mrand(seed), mrand(seed));
  return max(0, int(u + s * warp_gaussian(r).x));
}

float envelope(float w)
{
  return smoothstep(380.0, 400.0, w)*(1.0-smoothstep(700.0, 730.0, w));
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

vec2 hash(in ivec2 p)  // this hash is not production ready, please
{                        // replace this by something better
  ivec2 n = p.x*ivec2(3,37) + p.y*ivec2(311,113); // 2D -> 1D
  // 1D hash by Hugo Elias
  n = (n << 13) ^ n;
  n = n * (n * n * 15731 + 789221) + 1376312589;
  return -1.0+2.0*vec2( n & ivec2(0x0fffffff))/float(0x0fffffff);
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
  return noise(p) + .77*noise(0.61*p);
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
  vec3 particle_scale_col = vec3(0.8, 1.0, 2.0);
  vec3 particle_scale_lay = vec3(2.5, 1.0, 0.5);
  for(int col=0;col<3;col++)
  {
    float np = density[col] / density_max; // expectation of normalised number of developed grains
    float sat = clamp(1.5*np, 0, 1);
    vec3 doff = vec3(0.10, 0.20, 0.7);
    for(int layer=0;layer<3;layer++)
    { // from coarse to fine layers
      float npl = min(doff[layer], np);
      np -= npl;
      float c = 1.0/(particle_scale_col[col] * particle_scale_lay[layer]);
      vec2 tc = c/grain_size * 0.3*vec2(ipos + global.hash*0.000001 + global.frame*133.7 + 1000*col);
      int n = int(n_grains_per_pixel*c*scale*scale);
      // float sat = clamp(npl / doff[layer], 0, 1);
      // int r = int(grain_non_uniformity*c*noise(tc)*1*sat);
      int r = int(grain_non_uniformity*c*noisef(tc)*4*max(0,sat-0.0)*max(0,1.0-sat));
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

vec3 // returns log_raw
expose_film(vec3 rgb, int film)
{ // film exposure in camera and chemical development
  vec4 d65cf = fetch_coeff(vec3(1));
  vec4 coeff = fetch_coeff(rgb);
  // const mat3 M = mat3(
  //      1.66022677, -0.12455334, -0.01815514,
  //     -0.58754761,  1.13292605, -0.10060303,
  //     -0.07283825, -0.00834963,  1.11899817);
  // vec3 srgb = M * rgb;
  vec2 tc = vec2(0, get_tcy(s_sensitivity, film));
  vec3 raw = vec3(0.0);
  for(int l=0;l<=SN;l++)
  {
    float pdf = 2.0*(SN+1.0); // our integration is off by -1ev from agx, so 2.0 here
    float lambda = 380.0 + l*400.0/SN;
    tc.x = (l*80.0/SN+0.5)/256.0;
    float val = sigmoid_eval(coeff, lambda);
    // this upsamples *reflectances*, i.e. 111 is equal energy not D65
    // float val = colour_upsample(srgb, vec4(lambda)).x * sigmoid_eval(d65cf, lambda);
    // not sure if needed: cuts off wavelength ranges that the spectral
    // upsampling doesn't care about since it is outside the XYZ support
    float env = envelope(lambda);
    vec3 log_sensitivity = texture(img_filmsim, tc).rgb;
    // vec3 log_sensitivity = sample_catmull_rom_1d(img_filmsim, tc).rgb;
    vec3 sensitivity = pow(vec3(10.0), log_sensitivity);
    sensitivity = mix(sensitivity, vec3(0.0), isnan(sensitivity));
    raw += sensitivity * val * env / pdf;
  }
  const float one_log10 = 0.43429448190325176, log2_log10 = 0.30102999566398114;
  return params.ev_film * log2_log10 + log(raw+1e-10) * one_log10;
}

vec3 sigmoid(vec3 x)
{
  // return 0.5 + 0.5 * x / sqrt(1.0+x*x);
  const float b = 4.5;
  return 0.5 + 0.5*x / pow(pow(abs(x), vec3(b)) + 1.0, vec3(1.0/b));
}

vec3 sigmoid_ddx(vec3 x)
{
  // return 0.5*pow(x*x+1.0, vec3(-3.0/2.0));
  const float b = 4.5;
  vec3 xb = pow(abs(x), vec3(b));
  return 1.0/(pow(xb + 1.0, vec3(1.0/b)) * (xb + 1.0) * 2.0);
}

vec3 solve_f(vec3 e, vec3 ep, mat3 M)
{
  return e - ep - M * sigmoid(e);
}

vec3 solve_fp(vec3 e, vec3 ep, mat3 M)
{
  return vec3(1.0) - M * sigmoid_ddx(e);
}

vec3 newton_step(vec3 e, vec3 ep, mat3 M)
{
  return e - solve_f(e, ep, M)/solve_fp(e, ep, M);
}

mat3 coupler_matrix()
{
  mat3 M = mat3(6.0, 4.0, 1.0, 4.0, 6.0, 4.0, 1.0, 4.0, 6.0);
  vec3 amount = vec3(0.1, 0.2, 0.5)*3;
  M[0] *= 1.0/11.0 * amount.r * params.couplers;
  M[1] *= 1.0/14.0 * amount.g * params.couplers;
  M[2] *= 1.0/11.0 * amount.b * params.couplers;
  return M;
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
  mat3 M = coupler_matrix();
#if 0
  // the coupler/inhibitor M*log_raw should diffuse spatially, so we
  // write it out here, split the kernel, blur, and come back in part1.comp
  return M * log_raw;
#else
  return M * sigmoid(log_raw);
#endif
}

vec3 // returns new log_raw
develop_film_correct_exposure(vec3 log_raw, vec3 coupler)
{
  log_raw -= coupler;
  mat3 M = coupler_matrix();
#if 0
  mat3 I = mat3(1.0);
  // then we need to evaluate D_0 at this location
  log_raw.r = (inverse(I-M)*log_raw.rrr).r;
  log_raw.g = (inverse(I-M)*log_raw.ggg).g;
  log_raw.b = (inverse(I-M)*log_raw.bbb).b;
  // now we went full circle and log_raw = log_raw (except for the colour that is, which is what was wanted)
  return log_raw;
#else
  vec3 result;
  vec3 log_raw_corr = vec3(0.0);
  for(int i=0;i<5;i++)
    log_raw_corr = newton_step(log_raw_corr, log_raw.rrr, M);
  result.r = log_raw_corr.r;
  log_raw_corr = vec3(0.0);
  for(int i=0;i<5;i++)
    log_raw_corr = newton_step(log_raw_corr, log_raw.ggg, M);
  result.g = log_raw_corr.g;
  log_raw_corr = vec3(0.0);
  for(int i=0;i<5;i++)
    log_raw_corr = newton_step(log_raw_corr, log_raw.bbb, M);
  result.b = log_raw_corr.b;
  return result;
#endif
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
{ // enlarger: expose scanned transmittances of negative to print paper
  vec3 raw = vec3(0.0);
  const int paper = s_paper_offset + params.paper;
  vec3 neutral = vec3(params.filter_c,
      clamp(params.filter_m, 0, 1) + 0.1*params.tune_m,
      clamp(params.filter_y, 0, 1) + 0.1*params.tune_y);
  neutral = clamp(neutral, vec3(0.0), vec3(1.0));
  vec4 coeff = fetch_coeff(rgb);
  for(int l=0;l<=SN;l++)
  {
    float lambda = 380.0 + l*400.0/SN;
    vec2 tc = vec2(0.0, get_tcy(s_sensitivity, paper));
    tc.x = (l*(80.0/SN)+0.5)/256.0;
    vec3 log_sensitivity = texture(img_filmsim, tc).rgb;
    vec3 sensitivity = pow(vec3(10.0), log_sensitivity);
    sensitivity = mix(sensitivity, vec3(0.0), isnan(sensitivity));

    float transmittance = sigmoid_eval(coeff, lambda);

    float illuminant = (0.002*40.0/SN)*colour_blackbody(vec4(lambda), 2856.0).x;
#if 1 // pretty coarse manual fit to thorlabs filters:
    vec3 enlarger = 100.0*mix(
      vec3(1.0),
      thorlabs_filters(lambda),
      neutral);
#else
    // lamp filters are transmittances 0..100%
    vec3 enlarger = 100.0*mix(
      vec3(1.0),
      vec3(sigmoid_eval(coeff_c, lambda), sigmoid_eval(coeff_m, lambda), sigmoid_eval(coeff_y, lambda)),
      neutral);
#endif
    float print_illuminant = enlarger.x*enlarger.y*enlarger.z * illuminant;
    float light = transmittance * print_illuminant;
    raw += sensitivity * light * pow(2.0, params.ev_paper);
    // TODO and the same yet again for the preflash
  }
  const float one_log10 = 0.43429448190325176;
  return log(raw + 1e-10)*one_log10;
}

vec3 // returns log raw
enlarger_expose_film_to_paper(vec3 density_cmy)
{ // enlarger: expose film to print paper
  vec3 raw = vec3(0.0);
  const int film  = params.film;
  const int paper = s_paper_offset + params.paper;
  // vec3 tungsten = vec3(1.0985, 1.0000, 0.3558); // 2856K
  // vec4 coeff_l = fetch_coeff(XYZ_to_rec2020(tungsten));
  // sigmoidal transmission filters for cmy:
  // vec4 coeff_c = fetch_coeff(vec3(0.153, 1.0, 0.5));
  // vec4 coeff_m = fetch_coeff(vec3(1.0, -0.1, 2.0));
  // vec4 coeff_y = fetch_coeff(vec3(1.0, 0.3, 0.0));
  // coeff_c.w = coeff_m.w = coeff_y.w = 1.0; // we want reflectance/transmittance limited to 1.0 here
  vec3 neutral = vec3(params.filter_c,
      clamp(params.filter_m, 0, 1) + 0.1*params.tune_m,
      clamp(params.filter_y, 0, 1) + 0.1*params.tune_y);
  neutral = clamp(neutral, vec3(0.0), vec3(1.0));
  for(int l=0;l<=SN;l++)
  {
    float lambda = 380.0 + l*400.0/SN;
    vec2 tc = vec2(0.0, get_tcy(s_sensitivity, paper));
    tc.x = (l*(80.0/SN)+0.5)/256.0;
    vec3 log_sensitivity = texture(img_filmsim, tc).rgb;
    vec3 sensitivity = pow(vec3(10.0), log_sensitivity);
    sensitivity = mix(sensitivity, vec3(0.0), isnan(sensitivity));

    tc.y = get_tcy(s_dye_density, film);
    vec4 dye_density = texture(img_filmsim, tc);
    dye_density = mix(dye_density, vec4(1000000.0), isnan(dye_density));
    float density_spectral = dot(density_cmy, dye_density.xyz);
    density_spectral += dye_density.w * dye_density_min_factor_film;

    // float illuminant = 0.001*colour_blackbody(vec4(lambda), 3200.0).x;
    // float illuminant = 0.03*colour_blackbody(vec4(lambda), 2200.0).x;
    float illuminant = (0.002*40.0/SN)*colour_blackbody(vec4(lambda), 2856.0).x;
    // float illuminant = 5*sigmoid_eval(coeff_l, lambda);
#if 1 // pretty coarse manual fit to thorlabs filters:
    vec3 enlarger = 100.0*mix(
      // vec3 enlarger = mix(
      vec3(1.0),
      thorlabs_filters(lambda),
      neutral);
#else
    // lamp filters are transmittances 0..100%
    vec3 enlarger = 100.0*mix(
      vec3(1.0),
      vec3(sigmoid_eval(coeff_c, lambda), sigmoid_eval(coeff_m, lambda), sigmoid_eval(coeff_y, lambda)),
      neutral);
#endif
    float print_illuminant = enlarger.x*enlarger.y*enlarger.z * illuminant;
    float light = pow(10.0, -density_spectral) * print_illuminant;
    raw += sensitivity * light * pow(2.0, params.ev_paper);
    // TODO and the same yet again for the preflash
  }
  const float one_log10 = 0.43429448190325176;
  return log(raw + 1e-10)*one_log10;
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
{ // convert cmy density to spectral
  // absorption / dye density of developed film
  vec3 raw = vec3(0.0);
  vec3 d50 = vec3(0.9642, 1.0000, 0.8251); // 5002K
  vec4 coeff = fetch_coeff(d50);
  const int paper = s_paper_offset + params.paper;
  const int film  = params.film;
  for(int l=0;l<=SN;l++)
  {
    float lambda = 380.0 + l*400.0/SN;
    vec4 dye_density = texture(img_filmsim, vec2((l*(80.0/SN)+0.5)/256.0,
          get_tcy(s_dye_density, params.process != 1 ? paper : film)));
    dye_density = mix(dye_density, vec4(1000000.0), isnan(dye_density));
    float density_spectral = dot(vec3(1), density_cmy * dye_density.xyz);
    if(params.process != 1) density_spectral += dye_density.w * dye_density_min_factor_paper;
    else                    density_spectral += dye_density.w * dye_density_min_factor_film;
    float scan_illuminant = (4.0/(SN+1.0))*sigmoid_eval(coeff, lambda);
    float light = pow(10.0, -density_spectral) * scan_illuminant;
    vec3 cmf = cmf_1931(lambda); // 1931 2 deg std observer, approximate version
    raw += light * cmf;
  }
  raw = clamp(raw, vec3(0.0), vec3(14.0));
  return XYZ_to_rec2020(raw);
}
