#include "shared.glsl"

layout(local_size_x = DT_LOCAL_SIZE_X, local_size_y = DT_LOCAL_SIZE_Y, local_size_z = 1) in;

// global uniform stuff about image and roi
layout(std140, set = 0, binding = 0) uniform params_t
{
  roi_t ri;
  roi_t ro;
} params;

layout( // input f16 buffer rg
    set = 1, binding = 0
) uniform sampler2D img_in[];

layout( // output f16 buffer rg
    set = 1, binding = 1
) uniform writeonly image2D img_out[];

layout(push_constant, std140) uniform push_t
{
  uint step;
} push;
