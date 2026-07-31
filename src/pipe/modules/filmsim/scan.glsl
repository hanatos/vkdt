// scan.glsl: scan developed density back to Rec.2020 RGB.
vec3 add_glare(vec3 xyz, ivec2 ipos)
{
  if(prep.scan.glare_mean <= 0.0) return xyz;
  vec3 h = hash3(ipos, prep.scan.glare_seed);
  return xyz + prep.scan.glare_mean * exp2(0.61823826 * (2.0 * (h.x + h.y + h.z - 1.5)) - 0.13246692) * prep.scan.scan_wp;
}

vec3 scan_cmy_to_rgb(vec3 density_cmy, ivec2 ipos)
{
  vec3 raw;
  SPECTRAL_DYE_LIGHT(raw, density_cmy, prep.scan.scan_dye_r, prep.scan.scan_dye_g, prep.scan.scan_dye_b, prep.scan.scan_factor_r, prep.scan.scan_factor_g, prep.scan.scan_factor_b);
  vec3 xyz = add_glare(max(vec3(0.0), raw / prep.scan.scan_autoexp_norm), ipos);
  return XYZ_to_rec2020(prep.scan.scan_adapt * xyz);
}

vec3 finalize_film_density(vec3 density_cmy, ivec2 ipos)
{
  if (params.process == s_process_print)
    density_cmy = eval_paper_density(enlarger_expose_film(density_cmy));
  return scan_cmy_to_rgb(density_cmy, ipos);
}
