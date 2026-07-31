// exposure.glsl: spectral scene exposure onto film.
#include "shared/upsample.glsl"

vec3 apply_highlight_boost(vec3 raw_norm)
{
  if(params.hl_boost_ev <= 0.0) return raw_norm;
  float u = max(max(raw_norm.r, raw_norm.g), raw_norm.b);
  if(u <= 2.944) return raw_norm;
  float t = 0.1620843875 * min(u - 2.944, 61.056);
  return raw_norm * (1.0 + prep.film.hl_boost_k_gain * (exp2(t * log2_e) - t - 1.0) / u);
}

vec3 expose_scene_to_film(sampler2D coeff_img, vec3 rgb)
{
  rgb = max(vec3(5e-4), rgb);
  vec4 coeff = fetch_coeff(coeff_img, rgb);
  vec3 raw;
  SPECTRAL_COEFF_LIGHT(raw, coeff, prep.film.expose_factor_r, prep.film.expose_factor_g, prep.film.expose_factor_b, n_expose_groups);
  return (params.ev_film + log2(apply_highlight_boost(raw / prep.film.expose_autoexp_norm) + 1e-10)) * log10_2;
}
