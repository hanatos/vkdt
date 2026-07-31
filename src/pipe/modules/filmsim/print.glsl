// print.glsl: enlarger exposure of film or scanned negatives onto paper.
vec3 enlarger_expose_film(vec3 density_cmy)
{
  vec3 raw;
  SPECTRAL_DYE_LIGHT(raw, density_cmy, prep.paper.enlarger_dye_r, prep.paper.enlarger_dye_g, prep.paper.enlarger_dye_b, prep.paper.enlarger_factor_r, prep.paper.enlarger_factor_g, prep.paper.enlarger_factor_b);
  return log2((raw + prep.paper.preflash) / prep.paper.enlarger_autoexp_norm + 1e-10) * log10_2;
}

vec3 enlarger_expose_scanned_neg(sampler2D coeff_img, vec3 rgb)
{
  vec4 coeff = fetch_coeff(coeff_img, max(vec3(5e-4), rgb));
  vec3 raw;
  SPECTRAL_COEFF_LIGHT(raw, coeff, prep.paper.enlarger_factor_r, prep.paper.enlarger_factor_g, prep.paper.enlarger_factor_b, n_spectral_groups + 1);
  return log2((raw + prep.paper.preflash) / prep.paper.enlarger_autoexp_norm + 1e-10) * log10_2;
}
