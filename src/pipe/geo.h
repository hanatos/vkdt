#pragma once
// simplistic raw triangle data, suitable for streaming as single array (no indices)
#include "core/core.h"
#include <math.h>
#include <stdint.h>

// size = 6*sizeof(float)
typedef struct geo_vtx_t
{
  float x, y, z;     // float cartesian vertex coordinate
  uint16_t tex0;     // texture ids. we store 6 extra things on a triangle:
  uint16_t tex1;     // id: base, emit, roughness, normals. directly: surface flags (lava, slime, ..), alpha
  uint32_t n;        // encoded normal
  uint16_t s, t;     // half float texture st coordinates
}
geo_vtx_t;

// texture ids on the vertices are used as follows:
// base, flags, emit, alpha, normals, roughness
typedef struct geo_tri_t
{ // plain flat triangle. each vertex stores a different texture for the triangle: albedo, normals, extra.
  geo_vtx_t v0, v1, v2;
}
geo_tri_t;

// flags are:
typedef enum geo_flags_t
{
  s_geo_none   = 0, // nothing special, ordinary geo
  s_geo_lava   = 1, // lava
  s_geo_slime  = 2, // slime
  s_geo_tele   = 3, // teleporter
  s_geo_water  = 4, // water front surf
  s_geo_waterb = 5, // water back surf
  s_geo_watere = 6, // emissive water
  s_geo_sky    = 7, // quake style sky surface, a window to the infinite
  s_geo_nonorm = 8, // actually a flag, signifies there are no vertex normals
}
geo_flags_t;

static inline uint32_t
geo_encode_normal(float *vec)
{
  const float invL1Norm = 1.0f / (fabsf(vec[0]) + fabsf(vec[1]) + fabsf(vec[2]));
  // first find floating point values of octahedral map in [-1,1]:
  float enc0, enc1;
  if (vec[2] < 0.0f)
  {
    enc0 = (1.0f - fabsf(vec[1] * invL1Norm)) * ((vec[0] < 0.0f) ? -1.0f : 1.0f);
    enc1 = (1.0f - fabsf(vec[0] * invL1Norm)) * ((vec[1] < 0.0f) ? -1.0f : 1.0f);
  }
  else
  {
    enc0 = vec[0] * invL1Norm;
    enc1 = vec[1] * invL1Norm;
  }
  return  // don't use CLAMP because quake re-defines it with different argument order
    ((uint16_t)roundf(MIN(MAX(-32768.0f, enc1 * 32768.0f), 32767.0f))<<16) |
     (uint16_t)roundf(MIN(MAX(-32768.0f, enc0 * 32768.0f), 32767.0f));
}
