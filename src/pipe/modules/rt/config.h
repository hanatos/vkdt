// average number of vertices on a path, to determine path vertex cache size
#define AVG_V 4

#ifndef GL_core_profile // c version
#include <stdint.h>
typedef struct vec2 { float x,y; } vec2;
typedef struct vec3 { float x,y,z; } vec3;
typedef struct vec4 { float x,y,z,w; } vec4;
typedef uint32_t uint;
typedef struct f16vec3 { uint16_t x,y,z; } f16vec3;
#endif

// glsl version
struct pc_vtx_t
{
  vec3 v;
  // TODO: stuff to id the geo/motion vectors
  uint flags; // scattermode flags s_reflect etc above
  uint next;  // pointing to next vertex or -1u
};

struct pc_path_t
{
  vec4  lambda; // wavelengths
  uint  v;      // points to first vertex. this is in general different to the path index, since we may sort/resample
  float f;      // measurement contribution function
  float pdf;    // probability density function as sampled
};

struct pc_gauss_t
{ // gaussian covariance cache point
  vec3 p; // x  y  z
  vec3 s; // xx yy zz
  vec3 m; // xy yz zx
  float sum_w; // weighted ewa estimate, sum of previous weights decays exponentially
};
// gbuffer/material stuff. we have this separately from the distance/normal buffer since svgf
// requires only the latter and should be independent of other renderer needs
struct gbuf_t {
  uint m0, m1, m2;  // material ids and flags
  uint st;          // texture st coordinates as 2x16 unorm
  uint ng;          // geometry/triangle normal, encoded
};

// general
#define MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL 0
#define MERIAN_QUAKE_GRID_TYPE_QUADRATIC 1
struct MCState {
  vec3 w_tgt;
  float sum_w;
  uint N;
  float w_cos;

  vec3 mv;
  float T;

  uint hash; // grid_idx and level
};

struct LightCacheVertex {
  uint hash; // grid_idx and level
  uint lock;
  f16vec3 irr;
  uint16_t N;
};

struct DistanceMCState {
  float sum_w;
  uint N;
  vec2 moments;
};

#ifdef GL_core_profile // glsl version
#define DISTANCE_MC_VERTEX_STATE_COUNT 1
struct DistanceMCVertex {
  DistanceMCState states[DISTANCE_MC_VERTEX_STATE_COUNT];
};
#endif

// shoot only through pixel centers or random jitter a b/h pixel filter:
#define RT_GBUF_JITTER 1

// light cache
#define LIGHT_CACHE_MAX_N 128s
#define LIGHT_CACHE_MIN_ALPHA .01
#define LIGHT_CACHE_BUFFER_SIZE 4000037

#define MERIAN_QUAKE_LC_GRID_TYPE MERIAN_QUAKE_GRID_TYPE_QUADRATIC
#define LC_GRID_STEPS_PER_UNIT_SIZE 6.0
#define LC_GRID_TAN_ALPHA_HALF 0.005
#define LC_GRID_MIN_WIDTH 0.01
#define LC_GRID_POWER 2

// mc
#define ML_MAX_N 1024
#define ML_MIN_ALPHA .01

#define MERIAN_QUAKE_ADAPTIVE_GRID_TYPE MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL
#define DIR_GUIDE_PRIOR 0.3
#define MC_ADAPTIVE_GRID_TAN_ALPHA_HALF 0.002
#define MC_ADAPTIVE_GRID_MIN_WIDTH 0.01
#define MC_ADAPTIVE_GRID_POWER 4.0
#define MC_ADAPTIVE_GRID_STEPS_PER_UNIT_SIZE 4.743416490252569
#define MC_ADAPTIVE_BUFFER_SIZE 32777259

#define MC_STATIC_BUFFER_SIZE 800009
#define MC_STATIC_GRID_WIDTH 25.3

// mcpg
#define MAX_PATH_LENGTH 1
#define MC_SAMPLES 5
#define MC_SAMPLES_ADAPTIVE_PROB 0.7
#define SURF_BSDF_P 0.1
#define SURFACE_SPP 1
#define USE_LIGHT_CACHE_TAIL 1
#define MCPG_REFERENCE_MODE 0
#define MC_FAST_RECOVERY 1
