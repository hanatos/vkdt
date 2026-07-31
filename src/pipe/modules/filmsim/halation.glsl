// halation.glsl: workgroup state for combining blurred exposure bounces.
shared int shared_hal_n_bounces;
shared float shared_hal_rho, shared_hal_inv_weight_sum;
shared vec3 shared_hal_hs_base;
shared float shared_hal_inv_strength_sum;

void init_halation_workgroup()
{
  if (int(gl_LocalInvocationIndex) == 0)
  {
    int n_bounces = clamp(params.hal_bnc, 1, hal_max_bounces);
    float rho = clamp(params.hal_dec, 0.0, 1.0), weight_sum = 0.0, w = 1.0;
    for (int k = 0; k < n_bounces; k++) { weight_sum += w; w *= rho; }
    shared_hal_n_bounces = n_bounces; shared_hal_rho = rho;
    shared_hal_inv_weight_sum = 1.0 / max(weight_sum, 1e-6);
    vec3 hs_base = prep.halation.halation_strength * params.halation_amount * params.halation_strength.rgb;
    shared_hal_hs_base = hs_base;
    shared_hal_inv_strength_sum = 1.0 / max(hs_base.r + hs_base.g + hs_base.b, 1e-6);
  }
  barrier();
}

vec3 combine_halation(ivec2 ipos, sampler2D exposure,
    sampler2D hal0, sampler2D hal1, sampler2D hal2, sampler2D hal3, sampler2D hal4, sampler2D hal5)
{
  ivec2 out_sz = imageSize(img_out);
  vec2 tc = (ipos + 0.5) / vec2(out_sz);
  vec3 raw = max(fetch_upsampled(exposure, ipos, tc), vec3(0.0));
  int n_bounces = shared_hal_n_bounces;
  float rho = shared_hal_rho, w = 1.0;
  vec3 hal = w * max(fetch_upsampled(hal0, ipos, tc), vec3(0.0)); w *= rho;
  if(n_bounces > 1) { hal += w * max(fetch_upsampled(hal1, ipos, tc), vec3(0.0)); w *= rho; }
  if(n_bounces > 2) { hal += w * max(fetch_upsampled(hal2, ipos, tc), vec3(0.0)); w *= rho; }
  if(n_bounces > 3) { hal += w * max(fetch_upsampled(hal3, ipos, tc), vec3(0.0)); w *= rho; }
  if(n_bounces > 4) { hal += w * max(fetch_upsampled(hal4, ipos, tc), vec3(0.0)); w *= rho; }
  if(n_bounces > 5) hal += w * max(fetch_upsampled(hal5, ipos, tc), vec3(0.0));
  hal *= shared_hal_inv_weight_sum;
  const float midtone_falloff = 0.01, midtone_gain = 3.0;
  float x = max(0.0, (hal.r + hal.g + hal.b) * shared_hal_inv_strength_sum);
  vec3 hs = shared_hal_hs_base * exp2(-midtone_gain * log2_e * params.halation_midtones /
      (1e-3 + midtone_falloff * x * x));
  return log2(max((raw + hs * hal) / (1.0 + hs), vec3(1e-10))) * log10_2;
}
