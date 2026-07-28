#ifndef GL_core_profile
#define GL_core_profile 1
#endif
#extension GL_EXT_control_flow_attributes : enable
#include "shared.glsl"
#include "colourspaces.glsl"
#include "../rt/colour.glsl"
#include "constants.h"
// Nodes may override this workgroup shape.
#ifndef FILMSIM_LOCAL_SIZE_X
#define FILMSIM_LOCAL_SIZE_X DT_LOCAL_SIZE_X
#define FILMSIM_LOCAL_SIZE_Y DT_LOCAL_SIZE_Y
#endif
layout(local_size_x = FILMSIM_LOCAL_SIZE_X, local_size_y = FILMSIM_LOCAL_SIZE_Y, local_size_z = 1) in;
layout(std140, set = 0, binding = 0) uniform global_t
{
  int frame;
  int frame_cnt;
  int hash;
} global;
layout(std140, set = 0, binding = 1) uniform params_t
{
  int   process;
  int   film;
  float ev_film;
  float gamma_film;
  float g_fast;
  float g_slow;
  float exhaust;
  float hl_boost_ev;
  int   paper;
  float p_base;
  float ev_paper;
  float gamma_paper;
  float g_fast_p;
  float g_slow_p;
  float p_exh;
  float glare;
  float filter_c;
  float filter_m;
  float filter_y;
  float tune_m;
  float tune_y;
  int   preflash;
  float pf_ev;
  float pf_m;
  float pf_y;
  int   couplers;
  float cp_amt;
  float langmuir_r;
  float langmuir_g;
  float langmuir_b;
  float couplers_radius;
  int   halation;
  float halation_radius;
  float halation_amount;
  float halation_midtones;
  int   hal_bnc;
  float hal_dec;
  float scat_amt;
  float pad_cp0;
  float pad_cp1;
  vec4  halation_strength;
  int   grain;
  float grain_size;
  float grain_uniformity;
  int   enlarge;
  float scan_ill;
  float scene_ill;
  float film_ill;
  float pad0; // pads the block out to a whole number of vec4s
} params;
const int s_paper_offset = 22; // first paper in data list/lut; == len(film_stocks) in mklut-profiles.py

// what process selects, matching the combo order in params.ui
#define s_process_print     FILMSIM_PROCESS_PRINT
#define s_process_scan_neg  FILMSIM_PROCESS_SCAN_NEG
#define s_process_print_neg FILMSIM_PROCESS_PRINT_NEG

// 41 spectral bands, packed four per vec4.
const int n_spectral_bands = 41;
const int n_spectral_groups = 10; // full vec4 groups; band 40 is a scalar tail
const int n_expose_bands  = 36;
const int n_expose_groups = 9;    // 36/4, no tail
#define hal_max_bounces FILMSIM_MAX_HAL_BOUNCES

// In-emulsion scatter constants; blur widths live in main.c.
const vec3 scatter_tail_weight  = vec3(0.78, 0.65, 0.67);
// Three-Gaussian exponential-tail fit.
const vec3 scatter_exp_amp      = vec3(0.1633, 0.6496, 0.1870);
// Film and paper base-density scales.
const float dye_density_min_factor_film = 1.0;

#include "data.glsl"

const float log2_10 = 3.32192809489;        // base-10 density -> exp2 exponent
const float log10_2 = 0.30102999566398114;  // log2 -> log10
const float log2_e  = 1.44269504089;        // ln -> log2, for exp() via exp2()
const vec3 rec2020_luma = vec3(0.2627, 0.6780, 0.0593); // rec2020 Y row, autoexp metering
const float model_gumbel_scale = 1.66096404744; // matched Gumbel CDF scale (spektrafilm)
const float model_gumbel_loc   = 0.528765275;   // matched Gumbel CDF location offset (spektrafilm)

// Shared log-exposure LUT domain.
vec3 logexp_to_tc(vec3 x)
{
  return clamp(x * 0.125 + 0.5, vec3(0.0), vec3(1.0));
}
// Centre of a shared-LUT grid cell; 512 fits Vulkan's 16kB minimum.
#define logexp_grid FILMSIM_LOGEXP_GRID
float logexp_from_idx(float i)
{
  return (i + 0.5) * 0.015625 - 4.0;
}

// early-out past img_out's bounds, and name the in-bounds pixel position
#define FILMSIM_BOUNDS_INIT(POS) \
  ivec2 POS = ivec2(gl_GlobalInvocationID); \
  if(any(greaterThanEqual(POS, imageSize(img_out)))) return

#include "filmsim.glsl"
