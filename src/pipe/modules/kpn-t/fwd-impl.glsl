// fully connected multi-layer perceptron, following closely thomas mueller and
// nikolaus binder's implementation in tiny-cuda-nn.
#extension GL_EXT_nonuniform_qualifier      : enable
#extension GL_KHR_shader_subgroup_basic     : enable
#extension GL_EXT_scalar_block_layout       : enable
#extension GL_KHR_memory_scope_semantics    : enable
#extension GL_EXT_buffer_reference          : enable
#extension GL_EXT_control_flow_attributes   : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8    : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int32   : enable

#include "shared/coopmat.glsl"
#include "config.h"
#include "shared.glsl"
#include "input.glsl"

layout(local_size_x = 32, local_size_y = N_BLOCKS, local_size_z = 1) in;
layout(push_constant, std140) uniform push_t
{
  uint32_t in_width;
  uint32_t batch_size;
  uint32_t output_stride;
} push;
layout(set = 1, binding = 0) uniform sampler2D img_in; // 'M'
layout(set = 1, binding = 1) readonly  buffer ssbo_wgt_t { uvec4 v[]; } ssbo_weights; // 'w'
layout(set = 1, binding = 2) writeonly buffer ssbo_res_t { uvec4 v[]; } ssbo_res;     // output/result of the network 'K'
layout(set = 1, binding = 3) writeonly buffer ssbo_aux_t { uvec4 v[]; } ssbo_out;     // intermediate activations written here 'A'

shared uvec4 shm_act[((16 + 16*N_ITERS) * (WIDTH + SKEW)) / EL_PER_UVEC4];

#include "mlp.h"
#include "fwd.h"

void main()
{
  // each block computes exactly one 16-element chunk of the batch.
  const uint32_t elem_idx = 16 * gl_WorkGroupID.x * N_ITERS;

  // first layer
  threadblock_load_input_static(0, elem_idx * WIDTH);
  threadblock_layer(0, 0, elem_idx * WIDTH, 0);

  const uint32_t first_weights_stride = WIDTH * push.in_width;
  const uint32_t weights_stride = WIDTH * WIDTH;
  const uint32_t layer_stride = WIDTH * push.batch_size;

  // hidden layers
  for (int k = 0; k < N_HIDDEN_LAYERS-1; ++k)
    threadblock_layer(0, first_weights_stride + weights_stride * k, layer_stride * (k + 1) + elem_idx * WIDTH, 0);

  // last layer
  // if output layout == row major:
  threadblock_last_layer_forward(0, first_weights_stride + weights_stride * (N_HIDDEN_LAYERS-1), elem_idx * push.output_stride, push.output_stride, /*colmajor*/false);
  // else
  // threadblock_last_layer_forward(0, first_weights_stride + weights_stride * (N_HIDDEN_LAYERS-1), elem_idx, push.output_stride, true);
}
