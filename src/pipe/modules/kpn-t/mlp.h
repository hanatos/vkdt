// "marxist leninist party"
// following closely the implementation of tiny-cuda-nn by thomas mueller and nikolaus binder.
// helper functions for mlp inferencing and back propagation.
// if used for inferencing, define INFERENCE before including.
// if evaluated backward, define BACKWARD before including.

void warp_activation(
    in  coopmat_C_t x,
    out coopmat_C_t r)
{ // note that [.] only accesses the element owned by this invocation, and length only returns the count of these.
  // relu: for NaN, max is specified to keep the first argument
#if (MLP_ACTIVATION==MLP_ACTIVATION_RELU)
  for (int i = 0; i < x.length(); ++i) r[i] = max(float16_t(0.0), x[i]);
#elif (MLP_ACTIVATION==MLP_ACTIVATION_LEAKY_RELU)
  // leaky relu: better against dead neurons but can cause exploding gradients
  for (int i = 0; i < x.length(); ++i) r[i] = max(float16_t(0.001*x[i]), x[i]);
#elif (MLP_ACTIVATION==MLP_ACTIVATION_NONE)
  // DEBUG: no activation (useful to make network linear and debug derivatives with large epsilon)
  for (int i = 0; i < x.length(); ++i) r[i] = x[i];
#endif
  // TODO: also try elu = a(e^x-1)
}

void warp_activation_backward(
    in  coopmat_C_t x,  // fragment
    in  coopmat_C_t f,  // forward fragment
    out coopmat_C_t r)  // result
{ // inverted activation, i.e. if warp_activation: y=s(x) we compute dE/dx = act_bck(dE/dy) = dE/dy * dy/dx = x * s'(f)
#if (MLP_ACTIVATION==MLP_ACTIVATION_RELU)
  for (int i = 0; i < x.length(); ++i) r[i] = f[i] > 0.0 ? x[i] : float16_t(0.0); // relu
#elif (MLP_ACTIVATION==MLP_ACTIVATION_LEAKY_RELU)
  for (int i = 0; i < x.length(); ++i) r[i] = f[i] > 0.0 ? x[i] : float16_t(0.001 * x[i]); // leaky relu
#elif (MLP_ACTIVATION==MLP_ACTIVATION_NONE)
  for (int i = 0; i < x.length(); ++i) r[i] = x[i]; // DEBUG no activation
#endif
}

#ifndef INFERENCE
// writes to ssbo_out
void
threadblock_write_output_static(
    uint32_t act_shm_idx,
    uint32_t out_idx)
{
  // output_threadblock will be filled by the thread block's act_shmem
  const uint32_t li = gl_LocalInvocationID.x; // index in warp  ("lane index")
  const uint32_t wi = gl_LocalInvocationID.y; // index in block ("warp index")
  const uint32_t lane_offset = (8 * li) % WIDTH;
  const uint32_t row = (8 * li + wi * 8 * 32) / WIDTH;
  barrier();
  [[unroll]] for (int i = 0; i < N_ITERS; ++i)
    ssbo_out.v[(out_idx + lane_offset + (row + 16*i) * WIDTH)/EL_PER_UVEC4] =
      shm_act[(act_shm_idx + lane_offset + (row + 16*i) * (WIDTH + SKEW))/EL_PER_UVEC4];
}
#endif

// writes to shm_act
// if no inferencing: writes to ssbo_out
// if backward      : loads     ssbo_aux
void
threadblock_layer(
    uint32_t act_shm_idx,            // index points to uvec4 shared memory shm_act
    uint32_t weights_this_layer_idx, // points to uvec4 (global memory) ssbo_weights
                                     // if dL_dinput is requested can point to weights_first_layer too! (but we don't support it)
                                     // the following are only valid/used #ifndef INFERENCE
    uint32_t out_idx,                // offset to output ssbo, points to f16 ssbo_out
    uint32_t act_aux_idx)            // offset to activation aux ssbo, f16 ssbo_aux
{
  coopmat_A_t act_frag;               // row major
  coopmat_B_t weights_frag[N_BLOCKS]; // weights layout
  coopmat_C_t result_frag[N_ITERS];   // layout specified on store

  const uint32_t li = gl_LocalInvocationID.x; // index in warp  ("lane index")
  const uint32_t wi = gl_LocalInvocationID.y; // index in block ("warp index")
  const uint32_t lane_offset = (8 * li) % WIDTH;
  const uint32_t row = (8 * li + wi * 8 * 32) / WIDTH;
  const uint32_t weights_col = 16 * wi;

  [[unroll]] for (uint32_t i = 0; i < N_BLOCKS; ++i)
  { // load N_BLOCKS chunks of weights from global memory into registers.
#ifdef BACKWARD
      // If we're performing the backward pass, additional index swizzling is needed to
      // load the weights in transposed form.
      coopmat_load(weights_frag[i], ssbo_weights.v, (weights_this_layer_idx + 16*i*WIDTH + weights_col)/EL_PER_UVEC4, WIDTH/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
#else
      CHK_WGT((weights_this_layer_idx + 16*i+weights_col*WIDTH)/EL_PER_UVEC4, WIDTH/EL_PER_UVEC4)
      coopmat_load(weights_frag[i], ssbo_weights.v, (weights_this_layer_idx + 16*i+weights_col*WIDTH)/EL_PER_UVEC4, WIDTH/EL_PER_UVEC4, gl_CooperativeMatrixLayoutColumnMajor);
#endif
  }
  barrier();

  [[unroll]] for (int l = 0; l < N_ITERS; ++l)
  {
    result_frag[l] = coopmat_C_new(0.0);
    [[unroll]] for (uint32_t i = 0; i < N_BLOCKS; ++i)
    { // load a chunk of intermediate activations from shared memory and multiply with chunk of weights
      CHK_SHM((act_shm_idx + 16*i+(16*l)*(WIDTH + SKEW))/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4)
      coopmat_load(act_frag, shm_act, (act_shm_idx + 16*i+(16*l)*(WIDTH + SKEW))/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
      result_frag[l] = coopmat_madd(act_frag, weights_frag[i], result_frag[l]);
    }
    // Activation
#ifdef BACKWARD
    // Load the temporary forward matrix for the relu transfer
    coopmat_load(act_frag, ssbo_aux.v, (act_aux_idx + weights_col + l * 16 * WIDTH)/EL_PER_UVEC4, WIDTH/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
    warp_activation_backward(result_frag[l], act_frag, result_frag[l]);
#else
    warp_activation(result_frag[l], result_frag[l]);
#endif
  }

  barrier();
  [[unroll]] for (int l = 0; l < N_ITERS; ++l)
    CHK_SHM((act_shm_idx + weights_col + l*16*(WIDTH+SKEW))/EL_PER_UVEC4, (WIDTH+SKEW)/EL_PER_UVEC4)
    coopmat_store(result_frag[l], shm_act, (act_shm_idx + weights_col + l*16*(WIDTH+SKEW))/EL_PER_UVEC4, (WIDTH+SKEW)/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);

#ifndef INFERENCE
  barrier();
  [[unroll]] for (int l = 0; l < N_ITERS; ++l)
    ssbo_out.v[(out_idx + lane_offset + (row + 16 * l) * WIDTH)/EL_PER_UVEC4] =
      shm_act[(act_shm_idx + lane_offset + (row + 16 * l) * (WIDTH + SKEW))/EL_PER_UVEC4];
#endif
}
