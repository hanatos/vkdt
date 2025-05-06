// average number of vertices on a path, to determine path vertex cache size
#define AVG_V 4

#ifndef GL_core_profile // c version
#include <stdint.h>
typedef struct vec3 { float x,y,z; } vec3;
typedef struct vec4 { float x,y,z,w; } vec4;
typedef uint32_t uint;
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
