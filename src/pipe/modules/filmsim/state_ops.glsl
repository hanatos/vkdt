// state_ops.glsl: operations over the explicitly declared filmsim state SSBO.

vec3 eval_density(vec3 y, vec3 scale[3], vec3 bias[3], vec3 amps[3], float mix_ex)
{
  mix_ex = clamp(mix_ex, 0.0, 1.0);
  vec3 density_cmy = vec3(0.0);
  [[unroll]] for(int l = 0; l < 3; l++)
  {
    vec3 z = clamp(y * scale[l] + bias[l], vec3(-10.0), vec3(10.0));
    vec3 cdf = mix_ex <= 0.0 ? norm_cdf(z) : mix(norm_cdf(z), gumbel_cdf(z), mix_ex);
    density_cmy += amps[l] * cdf;
  }
  return density_cmy;
}

vec3 eval_film_density(vec3 y)
{
  return eval_density(y, prep.film.model_scale_film, prep.film.model_bias_film, prep.film.model_amps_film, params.exhaust);
}

vec3 eval_paper_density(vec3 y)
{
  return eval_density(y, prep.paper.model_scale, prep.paper.model_bias, prep.paper.model_amps, params.p_exh);
}

vec3 coupler_inhibit_pre_blur(vec3 d)
{
  if(prep.film.model_positive_film != 0) return prep.film.M * max(prep.film.model_dmax - d, vec3(0.0));
  vec3 L_dmax = d * prep.film.langmuir_num_dmax / max(prep.film.langmuir_k_dmax + d, vec3(1e-6));
  return prep.film.M * L_dmax;
}

vec3 coupler_saturate_post_blur(vec3 c)
{
  if(prep.film.model_positive_film == 0) return c;
  return c * (prep.film.Kr + prep.film.c_ref) / max(prep.film.Kr + c, vec3(1e-6));
}
