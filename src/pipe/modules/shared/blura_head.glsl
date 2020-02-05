#include "shared.glsl"

layout(local_size_x = DT_LOCAL_SIZE_X, local_size_y = DT_LOCAL_SIZE_Y, local_size_z = 1) in;

layout( // input
    set = 1, binding = 0
) uniform sampler2D img_in[];

layout( // output
    set = 1, binding = 1
) uniform writeonly image2D img_out[];

layout(push_constant, std140) uniform push_t
{
  int step;
} push;
