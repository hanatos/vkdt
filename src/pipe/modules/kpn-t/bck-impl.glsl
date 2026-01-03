#extension GL_EXT_nonuniform_qualifier      : enable
#extension GL_KHR_shader_subgroup_basic     : enable
#extension GL_EXT_scalar_block_layout       : enable
#extension GL_KHR_memory_scope_semantics    : enable
#extension GL_EXT_control_flow_attributes   : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8    : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int32   : enable
#include "shared/coopmat.glsl"

layout(local_size_x = 32, local_size_y = N_BLOCKS, local_size_z = 1) in;
// layout(std140, set = 0, binding = 1) uniform params_t { } params;
layout(push_constant, std140) uniform push_t
{
  uint32_t out_width;     // = dL/do rows = 16
  uint32_t batch_size;    // = dL/do columns/pixels
  uint32_t output_stride; // = dL/do stride
} push;
layout(set = 1, binding = 0) readonly  buffer ssbo_wgt_t { uvec4 v[]; } ssbo_weights;
layout(set = 1, binding = 1) writeonly buffer ssbo_out_t { uvec4 v[]; } ssbo_out;     // d loss / d I intermediates
layout(set = 1, binding = 2) readonly  buffer ssbo_aux_t { uvec4 v[]; } ssbo_aux;     // intermediate layer activations from fwd
layout(set = 1, binding = 3) readonly  buffer ssbo_dLO_t { uvec4 v[]; } ssbo_dLdO;    // d loss / d output

shared uvec4 shm_act[((16 + 16*N_ITERS) * (WIDTH + SKEW)) / EL_PER_UVEC4];

#define BACKWARD
#include "mlp.h"

// compute dE/dA, the gradient of the incoming activation from the previous layer.
// this is done in the threadblock_layer function as
//   dE/dA = w^T . dE/dO
// threadblock_layer has facilities to transpose the weights in case BACKWARD is defined.
// this also takes care of the reverse activation to yield dE/dO of prev layer
void main()
{
  const uint32_t li = gl_LocalInvocationID.x; // index in warp  ("lane index")
  const uint32_t wi = gl_LocalInvocationID.y; // index in block ("warp index")
  const uint32_t bi = gl_WorkGroupID.x;       // block index

  const uint32_t lane_offset = (8 * li) % WIDTH;
  const uint32_t row = (8 * li + wi * 8 * 32) / WIDTH;

  // Multipying one 16-row chunk of intermediate activations with the weight matrix requires all warps of the block.
  // Thus, each block computes exactly one 16-row chunk of the next layer's intermediate activations.
  const uint32_t elem_idx = 16 * bi * N_ITERS;
  const uint32_t weights_stride = WIDTH * WIDTH;
  const uint32_t layer_stride = WIDTH * push.batch_size;

  // Backprop through last layer
  coopmat_A_t act_frag;     // output layout: row major?
  coopmat_B_t weights_frag; // weights layout: row major
  coopmat_C_t result_frag[N_ITERS];
  // Load the relevant chunk of the last layer's weight matrix from global memory into registers
  const uint32_t weights_col = 16 * wi;

  // backpropagate the initial dEdK -> dEdA[hidden layers-1] 16x32 block. this reads the weights w[hidden layers]
  coopmat_load(weights_frag, ssbo_weights.v, (weights_stride * (N_HIDDEN_LAYERS) + weights_col)/EL_PER_UVEC4, WIDTH/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);

  [[unroll]] for (int l = 0; l < N_ITERS; ++l)
  {
    result_frag[l] = coopmat_C_new(0.0);
    coopmat_load(act_frag, ssbo_dLdO.v, (elem_idx + 16 * l) * push.output_stride/EL_PER_UVEC4, push.output_stride/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
    // coopmat_load(act_frag, ssbo_dLdO.v, (elem_idx + 16 * l)/EL_PER_UVEC4, push.output_stride/EL_PER_UVEC4, /*colmajor*/true);

    result_frag[l] = coopmat_madd(act_frag, weights_frag, result_frag[l]);

    // load activation A[hidden layers-1] to reverse the activation function gradient
    coopmat_A_t forward_frag;
    coopmat_load(forward_frag, ssbo_aux.v, (layer_stride * (N_HIDDEN_LAYERS-1) + weights_col + (elem_idx + l * 16) * WIDTH)/EL_PER_UVEC4, WIDTH/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
    warp_activation_backward(result_frag[l], forward_frag, result_frag[l]);
  }
  barrier();

  [[unroll]] for (int l = 0; l < N_ITERS; ++l)
    coopmat_store(result_frag[l], shm_act, (weights_col + (16*l) * (WIDTH+SKEW))/EL_PER_UVEC4, (WIDTH+SKEW)/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
  barrier();

  [[unroll]] for (int i = 0; i < N_ITERS; ++i)
    ssbo_out.v[nonuniformEXT((lane_offset + (row + elem_idx + i * 16) * WIDTH)/EL_PER_UVEC4)] =
      shm_act[nonuniformEXT((lane_offset + (row + 16*i) * (WIDTH + SKEW))/EL_PER_UVEC4)];

  // we read up to w[1] the weights of the second hidden layer, transforming A0 to A1, since
  // we need to backpropagate dEdA1 -> dEdA0 (the final layer would tranform dEdI which is only
  // needed for nerf and etc, see comment below)
  for (uint32_t k = 0; k < N_HIDDEN_LAYERS-1; ++k)
    threadblock_layer(0, weights_stride * (N_HIDDEN_LAYERS - k - 1), layer_stride * (k + 1) + elem_idx * WIDTH, layer_stride * (N_HIDDEN_LAYERS-1 - k - 1) + elem_idx * WIDTH);

#if 0 // XXX i think we don't need this (nor can we currently support it) (needed for NeRF and similar)
  // Compute loss gradients w.r.t. input if desired.
  // the input width has to be the same as the nework width and the layouts need to match too.
  // XXX no activation! weights in different buffer! write to dL_dinput instead!
  if (dL_dinput) threadblock_layer(0, weights_first_layer_idx, dL_dinput + elem_idx * WIDTH);
#endif
}
