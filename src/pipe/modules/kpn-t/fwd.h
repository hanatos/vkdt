// some extra stuff only for the forward pass:

void threadblock_load_input_static(
    uint32_t act_shm_idx,
    uint32_t in_idx)
{ // act_shmem will be filled by the thread block's chunk of input_threadblock
  const uint32_t li = gl_LocalInvocationID.x; // index in warp  ("lane index")
  const uint32_t wi = gl_LocalInvocationID.y; // index in block ("warp index")
  const uint32_t lane_offset = (8 * li) % WIDTH;
  const uint32_t row = (8 * li + wi * 8 * 32) / WIDTH;
#if 0
  [[unroll]] for (int i = 0; i < N_ITERS; ++i)
    shm_act[(act_shm_idx + lane_offset + (row + 16 * i) * (WIDTH + SKEW))/EL_PER_UVEC4] =
      ssbo_input.v[(in_idx + lane_offset + (row + 16 * i) * WIDTH)/EL_PER_UVEC4];
    // *(int4*)&act_shmem[lane_offset + (row + 16 * i) * (WIDTH + SKEW)] = *(int4*)&input_threadblock[lane_offset + (row + 16 * i) * WIDTH];
#else
#ifdef INFERENCE
  const vec2 noise_ab = vec2(params.noise_a, params.noise_b);
#else
  const vec2 noise_ab = vec2(ssbo_nab.noise_a, ssbo_nab.noise_b);
#endif
  [[unroll]] for (int i = 0; i < N_ITERS; ++i)
  {
    const uint32_t idx = in_idx + lane_offset + (row + 16 * i) * WIDTH;
    uvec4 load; // this is the base index into an f16 buffer where we should find 8 f16 in contiguous memory.
    [[unroll]] for(int k=0;k<WIDTH/4;k+=2)
    { // y-coord in this rowmajor layout is the pixel index, x are the WIDTH input elements
      uint32_t chan =((idx+k) % WIDTH);    // 16 taps with 2 channels each
      uint32_t pxi  = (idx+k) / WIDTH;     // outer index: px coordinate
      uint32_t val = packFloat2x16(f16vec2(vec2(
            load_input_tap(img_in, pxi, chan,   noise_ab),
            load_input_tap(img_in, pxi, chan+1, noise_ab))
            ));
      if     (k < 2) load.x = val;
      else if(k < 4) load.y = val;
      else if(k < 6) load.z = val;
      else if(k < 8) load.w = val;
    }
    shm_act[(act_shm_idx + lane_offset + (row + 16 * i) * (WIDTH + SKEW))/EL_PER_UVEC4] = load;
  }
#endif
  barrier();
}


#ifndef HAVE_NO_SSBO_RES
// writes to ssbo_res
void
threadblock_last_layer_forward(
    uint32_t act_shm_idx,
    uint32_t wgt_idx,
    uint32_t out_idx,
    uint32_t output_stride,
    bool output_colmajor)
{
  // act_shmem contains the intermediate activations (shared memory) of the thread block's chunk of the batch
  // weights_this_layer points to the weight matrix of the current layer
  // out points to the location where the result produced by the thread block should be written to.
  coopmat_A_t act_frag;               // input layout: row major
  coopmat_B_t weights_frag[N_BLOCKS]; // weights layout: col major
  coopmat_C_t result_frag;

  const uint32_t li = gl_LocalInvocationID.x; // index in warp  ("lane index")
  const uint32_t wi = gl_LocalInvocationID.y; // index in block ("warp index")

  const uint32_t weights_shmem_idx = act_shm_idx + N_ITERS * 16 * (WIDTH + SKEW);
  const uint32_t weights_row = (8 * li) % WIDTH;
  const uint32_t weights_col = (8 * li + 8 * 32 * wi) / WIDTH;

  // Load weight matrix into shared memory for the last multiplication.
  // Loading into shared memory as opposed to directly into registers is faster
  // because unlike in the previous layers, each warp uses the same entries of the weight matrix.
  CHK_SHM((weights_shmem_idx + weights_row + weights_col * (WIDTH + SKEW))/EL_PER_UVEC4, 0) // is a bit overly conservative because 16 > 8
  CHK_WGT((wgt_idx + weights_row + weights_col * WIDTH)/EL_PER_UVEC4, 0)
  shm_act[(weights_shmem_idx + weights_row + weights_col * (WIDTH + SKEW))/EL_PER_UVEC4] =
    ssbo_weights.v[(wgt_idx + weights_row + weights_col * WIDTH)/EL_PER_UVEC4];

  barrier();
  [[unroll]] for (uint32_t i = 0; i < N_BLOCKS; ++i)
  {
    CHK_SHM((weights_shmem_idx + 16*i)/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4)
    coopmat_load(weights_frag[i], shm_act, (weights_shmem_idx + 16*i)/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4, gl_CooperativeMatrixLayoutColumnMajor);
  }
  barrier();

  for (uint32_t idx = wi; idx < N_ITERS; idx += N_BLOCKS)
  { // perform last layer by parallelising over iters
    result_frag = coopmat_C_new(0.0);
    [[unroll]] for (uint32_t i = 0; i < N_BLOCKS; ++i)
    { // load a chunk of intermediate activations from shared memory and multiply with chunk of the weight matrix
      CHK_SHM((act_shm_idx + 16*i + (16*idx)*(WIDTH + SKEW))/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4)
      coopmat_load(act_frag, shm_act, (act_shm_idx + 16*i + (16*idx)*(WIDTH + SKEW))/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4, gl_CooperativeMatrixLayoutRowMajor);
      result_frag = coopmat_madd(act_frag, weights_frag[i], result_frag);
    }

    // no activation in last layer:
    // warp_activation(result_frag, result_frag);

    if(output_colmajor) {
      CHK_OUT((out_idx + idx * 16)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4)
      coopmat_store(result_frag, ssbo_res.v, (out_idx + idx * 16)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4, gl_CooperativeMatrixLayoutColumnMajor);
    } else {
      CHK_OUT((out_idx + idx * 16 * output_stride)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4)
      coopmat_store(result_frag, ssbo_res.v, (out_idx + idx * 16 * output_stride)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4, gl_CooperativeMatrixLayoutColumnMajor);
    }
  }
}
#endif
