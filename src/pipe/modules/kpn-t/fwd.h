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
  [[unroll]] for (int i = 0; i < N_ITERS; ++i)
  {
    const uint32_t idx = in_idx + lane_offset + (row + 16 * i) * WIDTH;
    uvec4 load; // this is the base index into an f16 buffer where we should find 8 f16 in contiguous memory.
    [[unroll]] for(int k=0;k<8;k+=2)
    { // y-coord in this rowmajor layout is the pixel index, x are the 32 input elements
      uint32_t tp  =((idx+k) % WIDTH)/2;  // 16 taps with 2 channels each
      uint32_t pxi = (idx+k) / WIDTH;     // outer index: px coordinate
      ivec2 tc = ivec2(pxi % textureSize(img_in, 0).x, pxi / textureSize(img_in, 0).x);
      uint32_t val = packFloat2x16(f16vec2(load_input_tap(img_in, tc, tp)));
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
  coopmat_t act_frag;               // input layout: row major
  coopmat_t weights_frag[N_BLOCKS]; // weights layout: col major
  coopmat_t result_frag;

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
    CHK_SHM((weights_shmem_idx + 16*i)/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4)
    coopmat_load(weights_frag[i], shm_act, (weights_shmem_idx + 16*i)/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4, /*colmajor*/true);
  barrier();

  for (uint32_t idx = wi; idx < N_ITERS; idx += N_BLOCKS)
  { // perform last layer by parallelising over iters
    result_frag = coopmat_new(0.0);
    [[unroll]] for (uint32_t i = 0; i < N_BLOCKS; ++i)
    { // load a chunk of intermediate activations from shared memory and multiply with chunk of the weight matrix
      CHK_SHM((act_shm_idx + 16*i + (16*idx)*(WIDTH + SKEW))/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4)
      coopmat_load(act_frag, shm_act, (act_shm_idx + 16*i + (16*idx)*(WIDTH + SKEW))/EL_PER_UVEC4, (WIDTH + SKEW)/EL_PER_UVEC4, /*colmajor*/false);
      result_frag = coopmat_madd(act_frag, weights_frag[i], result_frag);
    }

    // no activation in last layer:
    // warp_activation(result_frag, result_frag);

    if(output_colmajor) {
      CHK_OUT((out_idx + idx * 16)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4)
      coopmat_store(result_frag, ssbo_res.v, (out_idx + idx * 16)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4, /*colmajor*/true);
    } else {
      CHK_OUT((out_idx + idx * 16 * output_stride)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4)
      coopmat_store(result_frag, ssbo_res.v, (out_idx + idx * 16 * output_stride)/EL_PER_UVEC4, output_stride/EL_PER_UVEC4, /*colmajor*/false);
    }
  }
}
#endif


#if 0 // XXX TODO: port this if we want a different input width or layout:
void
threadblock_input_layer_forward_dynamic(
    uint32_t act_shmem_idx,
    // XXX:
    const __half* __restrict__ input_threadblock,
    const __half* __restrict__ weights_this_layer,
    OUT_T* __restrict__ out_intermediate_threadblock_this_layer,
    const uint32_t in_width,
    const uint32_t batch_size)
{
  // act_shmem contains the intermediate activations (shared memory) of the thread block's chunk of the batch
  // input_threadblock points to the thread block's chunk of the input batch in global memory
  // weights_this_layer points to the weight matrix of the current layer
  // out_intermediate_threadblock_this_layer points to the location where intermediate activations produced by the thread block should be written to.
  //                  Can be nullptr if nothing should be written.
  // in_width is the dynamic width of the input layer

  fcoopmatNV<16, gl_ScopeSubgroup, 16, 16> act_frag;             // input layout: (row major?)
  fcoopmatNV<16, gl_ScopeSubgroup, 16, 16> weights_frag;         // weights layout: col major
  fcoopmatNV<16, gl_ScopeSubgroup, 16, 16> result_frag[N_ITERS];
    
  const uint32_t li = gl_LocalInvocationID.x; // index in warp  ("lane index")
  const uint32_t wi = gl_LocalInvocationID.y; // index in block ("warp index")
  const uint32_t lane_offset = (8 * li) % WIDTH;
  const uint32_t row = (8 * li + wi * 8 * 32) / WIDTH;
  const uint32_t weights_col = 16 * wi;

  const uint32_t weights_shmem_idx = 16 * (in_width + INPUT_SKEW); // offset in f16

  // Load input weight matrix (fits completely into shared memory)
  // Each thread can load 8 fp16 elements (16 bytes) at once; we have N_BLOCKS warps
  const uint32_t n_elems_per_load = N_BLOCKS * 32 * 8;
  const uint32_t thread_elem_idx = (li + wi * 32) * 8;
  const uint32_t n_elems_b = WIDTH * in_width;

  [[unroll]] for (uint32_t idx = thread_elem_idx; idx < n_elems_b; idx += n_elems_per_load)
  {
    const uint32_t idx_skewed = idx + idx / in_width * INPUT_SKEW;
    shm_act[weights_shmem_idx + idx_skewed] = weights_this_layer[idx]; // XXX uvec4 vs half*!
    // *(int4*)&weights_shmem[idx_skewed] = *(int4*)&weights_this_layer[idx];
  }

  const uint32_t n_tensor_ops = in_width / 16;

  // XXX ???
  // if (std::is_same<INPUT_LAYOUT, wmma::col_major>::value) __syncthreads();
  [[unroll]]
  for (int l = 0; l < N_ITERS; ++l)
  {
    // if (std::is_same<INPUT_LAYOUT, wmma::row_major>::value) {
      // Load chunk of inputs into shmem.
      // This is faster than loading it from gmem directly, even though it is only used once.
      // (Possibly due to latency hiding through staging.)
      const uint32_t n_elems_a = 16 * in_width;

      [[unroll]]
      for (uint32_t idx = thread_elem_idx; idx < n_elems_a; idx += n_elems_per_load)
      {
        const uint32_t idx_skewed = idx + idx / in_width * INPUT_SKEW;
        act_shmem[idx_skewed] = input_threadblock[l*n_elems_a + idx]; // XXX uvec4 vs half*!
        // *(int4*)&act_shmem[idx_skewed] = *(int4*)&input_threadblock[l * n_elems_a + idx];
      }

      // __syncthreads();
      barrier();
    // }

    // wmma::fill_fragment(result_frag[l], 0.0f);
    result_frag[l] = fcoopmatNV<16, gl_ScopeSubgroup, 16, 16>(0.0);
    [[unroll]]
    for (uint32_t i = 0; i < n_tensor_ops; ++i)
    { // Load chunk of inputs and weights from shared memory and multiply them
      // if (std::is_same<INPUT_LAYOUT, wmma::row_major>::value) {
        // wmma::load_matrix_sync(act_frag, act_shmem + 16 * i, in_width + INPUT_SKEW);
        coopMatLoadNV(act_frag, act_shmem, 16*i, in_width + INPUT_SKEW, /*col major*/false);
      // } else {
      //  wmma::load_matrix_sync(act_frag, input_threadblock + 16 * i * batch_size + 16 * l, batch_size);
      // }
      coopMatLoadNV(weights_frag, act_shmem, weights_shmem_idx + 16*i + weights_col * (in_width + INPUT_SKEN), in_width + INPUT_SKEW, /*col major*/true);
      result_frag[l] = coopMatMulAddNV(act_frag, weights_frag, result_frag[l]);
      // wmma::load_matrix_sync(weights_frag, weights_shmem + 16 * i + weights_col * (in_width + INPUT_SKEW), in_width + INPUT_SKEW);
      // wmma::mma_sync(result_frag[l], act_frag, weights_frag, result_frag[l]);
    }

    barrier();
    // if (std::is_same<INPUT_LAYOUT, wmma::row_major>::value) {
      // __syncthreads();
    // }

    warp_activation(result_frag[l], result_frag[l]);
    // warp_activation<__half>(activation, result_frag[l], result_frag[l]);
  }

  // if (std::is_same<INPUT_LAYOUT, wmma::col_major>::value) {
    // __syncthreads();
  // }

  [[unroll]]
  for (int l = 0; l < N_ITERS; ++l)
    // wmma::store_matrix_sync(act_shmem + weights_col + (16 * l) * (WIDTH + SKEW), result_frag[l], WIDTH + SKEW, wmma::mem_row_major);
    coopMatStoreNV(result_frag[l], act_shmem, weights_col + l*16*(WIDTH+SKEW), WIDTH+SKEW, /*colmajor*/false);

  // XXX ???
  if (out_intermediate_threadblock_this_layer != nullptr) {
    // __syncthreads();
    barrier();

    [[unroll]] for (int i = 0; i < N_ITERS; ++i)
      // *(int4*)&out_intermediate_threadblock_this_layer[lane_offset + (row + 16 * i) * WIDTH] = *(int4*)&act_shmem[lane_offset + (row + 16 * i) * (WIDTH + SKEW)];
      out_intermediate_threadblock_this_layer[lane_offset + (row + 16*i)*WIDTH] = act_shmem[lane_offset + (row + 16*i) * (WIDTH + SKEW)]; // XXX uvec4 vs half*
  }
}
#endif

// TODO: move specialised version of this into inf.comp:
#if 0
#if 0
// template <int WIDTH, int N_ITERS, typename OUT_T, Activation ACTIVATION, bool INFERENCE>
// __global__
void kernel_mlp_fused(
    const Activation output_activation,
    const __half* __restrict__ input,
    const __half* __restrict__ weights,
    OUT_T* __restrict__ out_intermediate,
    OUT_T* __restrict__ out,
    const uint32_t output_stride,
    const uint32_t batch_size,
    const uint32_t in_width,
    const uint32_t out_width,
    const uint32_t n_hidden_matmuls,
    const nvcuda::wmma::layout_t input_layout,
    const nvcuda::wmma::layout_t output_layout)
#endif
void main()
{
  // `input` points to the input matrix. Can be any width.
  // `weights` points to the weight matrices (contiguous in memory).
  // `out_intermediate` points to the memory where intermediate activations should be written. When performing inference, a value of nullptr is expected (intermediate results are not written).
  // `out` points to the memory where the network output should be written. (Output width is assumed to be 16 neurons.)

  // Commented out due to isolated strange side-effects on Windows
  // if (INFERENCE) {
  //  assert(out_intermediate == nullptr);
  // } else {
  //  assert(out_intermediate);
  // }

  // Shared memory contains the intermediate activations of blockDim.y*16 elements.
  // In some cases, it also contains the weight matrix for the first and last layer.
  // extern __shared__ __half shmem[];
  // __half* act_shmem = shmem;

  // Each block computes exactly one 16-element chunk of the batch.
  // const uint32_t elem_idx = 16 * blockIdx.x * N_ITERS;
  const uint32_t elem_idx = 16 * gl_WorkGroupID.x * N_ITERS;

  // First layer
  if (input_layout == nvcuda::wmma::mem_col_major || in_width != WIDTH) {
    if (input_layout == nvcuda::wmma::mem_row_major) {
      threadblock_input_layer_forward_dynamic<WIDTH, N_ITERS, OUT_T, nvcuda::wmma::row_major>(ACTIVATION, act_shmem, input + elem_idx * in_width, weights, !INFERENCE ? (out_intermediate + elem_idx * WIDTH) : nullptr, in_width, batch_size);
    } else {
      threadblock_input_layer_forward_dynamic<WIDTH, N_ITERS, OUT_T, nvcuda::wmma::col_major>(ACTIVATION, act_shmem, input + elem_idx, weights, !INFERENCE ? (out_intermediate + elem_idx * WIDTH) : nullptr, in_width, batch_size);
    }
  } else {
    // If the input has the same width & layout as the hidden layers, we can simply use the network's regular layer routine (with static size)
    // instead of using the slower dynamic input layer routine.
    threadblock_load_input_static<WIDTH, N_ITERS>(act_shmem, input + elem_idx * WIDTH);
    // XXX BACKWARD=false
    threadblock_layer<WIDTH, N_ITERS, OUT_T>(ACTIVATION, act_shmem, weights, !INFERENCE ? (out_intermediate + elem_idx * WIDTH) : nullptr);
  }

  const uint32_t first_weights_stride = WIDTH * in_width;
  const uint32_t weights_stride = WIDTH * WIDTH;
  const uint32_t layer_stride = WIDTH * batch_size;

  // Hidden layers
  for (uint32_t k = 0; k < n_hidden_matmuls; ++k)
    // threadblock_layer<WIDTH, N_ITERS, OUT_T>(ACTIVATION, act_shmem, weights + first_weights_stride + weights_stride * k, !INFERENCE ? (out_intermediate + layer_stride * (k + 1) + elem_idx * WIDTH) : nullptr);
    // XXX BACKWARD=false
    threadblock_layer(weights + first_weights_stride + weights_stride * k, !INFERENCE ? (out_intermediate + layer_stride * (k + 1) + elem_idx * WIDTH) : nullptr);

  if (out_width > 16) {
    // In the forward pass, intermediate activations are already written out.
    if (INFERENCE) {
      threadblock_write_output_static<WIDTH, N_ITERS>(act_shmem, out_intermediate + elem_idx * WIDTH);
    }
  } else if (out) {
    // Last layer
    if (output_layout == nvcuda::wmma::mem_row_major) {
      threadblock_last_layer_forward<WIDTH, N_ITERS, OUT_T>(output_activation, act_shmem, weights + first_weights_stride + weights_stride * n_hidden_matmuls, out + elem_idx * output_stride, output_stride, output_layout);
    } else {
      threadblock_last_layer_forward<WIDTH, N_ITERS, OUT_T>(output_activation, act_shmem, weights + first_weights_stride + weights_stride * n_hidden_matmuls, out + elem_idx, output_stride, output_layout);
    }
  }
}
#endif
