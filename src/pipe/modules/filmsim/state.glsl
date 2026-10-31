// state.glsl: type of the prepared-state SSBO. Each pass declares its binding.
struct filmsim_film_state_t
{
  vec4  expose_factor_r[11], expose_factor_g[11], expose_factor_b[11];
  float hl_boost_k_gain;

  vec3  model_scale_film[3], model_bias_film[3], model_amps_film[3];
  int   model_positive_film;
  vec3  model_dmax;
  mat3  M;
  vec3  c_ref, Kr, langmuir_k_dmax, langmuir_num_dmax;
};

struct filmsim_paper_state_t
{
  vec3  model_scale[3], model_bias[3], model_amps[3];

  vec4  enlarger_dye_r[11], enlarger_dye_g[11], enlarger_dye_b[11];
  vec4  enlarger_factor_r[11], enlarger_factor_g[11], enlarger_factor_b[11];
  vec3  preflash;
  float enlarger_autoexp_norm;
};

struct filmsim_grain_state_t
{
  vec3  grain_dmin;
  vec3  inv_density_range, layer_fraction_0, layer_fraction_01;
  vec3  inv_layer_fraction_0, inv_layer_fraction_1, inv_layer_fraction_2;
  vec3  variance_weight_fast, variance_weight_mid, variance_weight_slow, uniformity;
  vec2  lattice_seed;
  float noise_kernel_scale, lattice_scale;
  int   particles_per_cell;
  uint  random_stream;
};

struct filmsim_halation_state_t
{
  vec3  hs_base;
  float rho, inv_weight_sum, inv_strength_sum;
  int   n_bounces;
};

struct filmsim_scan_state_t
{
  vec4  scan_dye_r[11], scan_dye_g[11], scan_dye_b[11];
  vec4  scan_factor_r[11], scan_factor_g[11], scan_factor_b[11];
  vec3  scan_wp;
  mat3  scan_adapt;
  float scan_autoexp_norm;
  float glare_mean;
  uint  glare_seed;

};

struct filmsim_state_t
{
  filmsim_film_state_t film;
  filmsim_paper_state_t paper;
  filmsim_grain_state_t grain;
  filmsim_halation_state_t halation;
  filmsim_scan_state_t scan;
};
