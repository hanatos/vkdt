#include "shared.glsl"
#include "colourspaces.glsl"
#include "../rt/colour.glsl"
layout(local_size_x = DT_LOCAL_SIZE_X, local_size_y = DT_LOCAL_SIZE_Y, local_size_z = 1) in;
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
  float couplers;
  float couplers_radius;
  int   grain;
  float grain_size;
  float grain_uniformity;
  int   paper;
  float ev_paper;
  float gamma_paper;
  int   enlarge;
  float filter_c;
  float filter_m;
  float filter_y;
  float tune_m;
  float tune_y;
  int   halation;
  float halation_radius;
  float halation_scale;
  vec4  halation_strength;
} params;
layout(set = 1, binding = 0) uniform sampler2D img_in;
layout(set = 1, binding = 1) uniform writeonly image2D img_out;
layout(set = 1, binding = 2) uniform sampler2D img_filmsim;
layout(set = 1, binding = 3) uniform sampler2D img_coeff;   // spectral upsampling for emission
#include "shared/upsample.glsl"

const int s_paper_offset = 15; // first paper in data list/lut
const float dye_density_min_factor_film  = 1.0;
const float dye_density_min_factor_paper = 0.4;
#include "filmsim.glsl"
