#pragma once
#include "core/core.h"

// utility functions to deal with drawn raster masks
typedef struct dt_draw_vert_t
{
  uint16_t x, y; // position
  uint16_t r;    // radius
  uint8_t o;     // opacity
  uint8_t h;     // hardness
}
dt_draw_vert_t;

// create vertex
static inline dt_draw_vert_t
dt_draw_vertex(
    float x,        // position in ndc -1..1 (overscan -2..2 is allowed)
    float y,
    float radius,   // brush size in 0..2
    float opacity,  // in 0..1
    float hardness) // in 0..1
{
  return (dt_draw_vert_t){
    .x = CLAMP((int32_t)((x+2.0f)/4.0f*0xffff), 0, 0xffff),
    .y = CLAMP((int32_t)((y+2.0f)/4.0f*0xffff), 0, 0xffff),
    .r = CLAMP((int32_t)(0.5*radius*0xffff), 0, 0xffff),
    .o = CLAMP((int32_t)(opacity *0xff), 0, 0xff),
    .h = CLAMP((int32_t)(hardness*0xff), 0, 0xff),
  };
}

// create end marker (all zero)
static inline dt_draw_vert_t
dt_draw_endmarker()
{
  return (dt_draw_vert_t){0};
}

// interpolate two vertices
// static inline

// length of array vs number of vertices, write paramsub style stuff?
