// the original code was written by Adrien Vannson, https://github.com/AdrienVannson/gpu-denoising.git
// and is used with permission under MIT licence here.
#extension GL_KHR_memory_scope_semantics: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable
#extension GL_EXT_control_flow_attributes: enable
#extension GL_KHR_shader_subgroup_basic: enable
#extension GL_EXT_shader_subgroup_extended_types_float16: enable
#extension GL_KHR_shader_subgroup_shuffle: enable
// XXX TODO put in impl- file for runtime switching
// #define VKDT_COOPMAT_FALLBACK
#include "shared/coopmat.glsl"

// we can't always get enough shared memory or register file to embrace
// all the input variables and cooperative matrices we wish for. if this
// is set, it will divide the memory cost for shared and registers for
// the output variable size and instead iterate over the data.
// F_OUT_32 has to be divisible by this number.
// F_OUT_32/F_OUT_BATCHES has to be divisible by 32 too.
#ifndef F_OUT_BATCHES
#define F_OUT_BATCHES 1
#endif

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(push_constant, std140) uniform push_t
{
  int off; // beginning of the weights
  int wd;
  int ht;
} push;

// a thread block is responsible of a tile with these dimensions
const int TILE_HEIGHT = 8;
const int TILE_WIDTH  = 8;

layout(std430, set = 1, binding = 0) readonly  buffer buf_w_t   { float16_t weights[]; };
layout(std430, set = 1, binding = 1) writeonly buffer buf_out_t { float16_t buf_out[]; };

#ifdef INPUT_SKIP_CONNECTION
// the input is separated in two buffers whose features must be concatenated on the fly.
// the sizes are given in NB_INPUT_FEATURES_1 and NB_INPUT_FEATURES_2.
layout(std430, set = 1, binding = 2) readonly buffer buf_in_t   { float16_t buf_in_1[]; };
layout(std430, set = 1, binding = 3) readonly buffer buf_skip_t { float16_t buf_in_2[]; };
#else
layout(std430, set = 1, binding = 2) readonly buffer buf_in_t   { float16_t buf_in[]; };
#endif

uint I_WIDTH; // Same as W_HEIGHT
uint I_WIDTH_32; // Same as W_HEIGHT_32

float16_t weight(const int row, const int col, const uint feature_in, const uint feature_out)
{ // pytorch weights come as OIHW
  uint idx = 0;
  idx = NB_OUTPUT_FEATURES * idx + feature_out;
  idx = NB_INPUT_FEATURES  * idx + feature_in;
  idx = 3 * idx + row;
  idx = 3 * idx + col;
  return weights[push.off + idx];
}

float16_t bias(const uint i)
{
  return weights[push.off + 9*NB_INPUT_FEATURES*NB_OUTPUT_FEATURES + i];
}

float16_t coef_of_image(const int i, const int j, const uint f_in)
{ // i and j are row and column, not x and y..
  if (i < 0 || i >= push.ht || j < 0 || j >= push.wd) return float16_t(0.);
  float16_t res = float16_t(0.);
#ifdef INPUT_SKIP_CONNECTION
  if (f_in < NB_INPUT_FEATURES_1)
  { // first input, not the skip connection
#ifdef INPUT_UPSAMPLE
    // we need to upsample
    const uint cwd = (push.wd+1)/2;
    const uint pos = (i/2) * cwd + (j/2);
    res = buf_in_1[INPUT_1_FEATURE_STRIDE * pos + f_in];
#else
    res = buf_in_1[INPUT_1_FEATURE_STRIDE * (i * push.wd + j) + f_in];
#endif
  }
  else
  { // this is input2, the skip connection at same res as our output
    const uint pos = i * push.wd + j;
    res = buf_in_2[INPUT_2_FEATURE_STRIDE * pos + (f_in - NB_INPUT_FEATURES_1)];
  }
#else // no skip connection
#ifdef INPUT_UPSAMPLE
  // we need to upsample
  const uint cwd = (push.wd+1)/2;
  const uint pos = (i/2) * cwd + (j/2);
  res = buf_in[INPUT_FEATURE_STRIDE * pos + f_in];
#else
  res = buf_in[INPUT_FEATURE_STRIDE * (i * push.wd + j) + f_in];
#endif
#endif
  return res;
}

float16_t coef_matrix_I(const uint line, const uint column)
{
  if (column >= I_WIDTH) return float16_t(0.);

  // input pixel
  int row = int(TILE_HEIGHT * gl_WorkGroupID.x + line / TILE_WIDTH);
  int col = int(TILE_WIDTH  * gl_WorkGroupID.y + line % TILE_WIDTH);

  const int neighbour = int(column / NB_INPUT_FEATURES);

  row += neighbour / 3 - 1;
  col += neighbour % 3 - 1;

  const uint feature = column % NB_INPUT_FEATURES;
  return coef_of_image(row, col, feature);
}

float16_t coef_matrix_W(const uint line, const uint column)
{
  if (line >= I_WIDTH) return float16_t(0.);

  const int row = int(line / NB_INPUT_FEATURES) / 3;
  const int col = int(line / NB_INPUT_FEATURES) % 3;

  const uint feature_in = line % NB_INPUT_FEATURES;
  const uint feature_out = column;
  
  return weight(row, col, feature_in, feature_out);
}

// the threads cooperate to load the current part of I and W to shared memory
shared float16_t current_column_I[(TILE_HEIGHT * TILE_WIDTH) * 16];
shared float16_t current_line_W[16 * F_OUT_32/F_OUT_BATCHES];

// XXX apparently the max number of registers per thread is 255 so this can't work:
coopmat_t coop_mat_column_I[(TILE_WIDTH * TILE_HEIGHT) / 16];
coopmat_t coop_mat_line_W[F_OUT_32/F_OUT_BATCHES / 16];

// XXX register file is 64K 32-bit floats, so maybe 256KB
// XXX 8*8 / 16 * 128 / 16 * (mat)16*16 / 32 threads is exactly 256
// output of the multiplication
coopmat_t sums[(TILE_WIDTH * TILE_HEIGHT) / 16][F_OUT_32/F_OUT_BATCHES / 16];

// used when exporting to the output buffer
// shared float16_t exported_matrix[16*16];

void main()
{
  const uint id_loc = gl_LocalInvocationID.x;
  I_WIDTH = 9 * NB_INPUT_FEATURES;
  I_WIDTH_32 = 32 * ((I_WIDTH+31) / 32);

#define FF (F_OUT_32/F_OUT_BATCHES)
  [[unroll]] for(int batch=0;batch<F_OUT_BATCHES;batch++)
  {
    [[unroll]] for (int x = 0; x < TILE_WIDTH * TILE_HEIGHT / 16; x++)
      [[unroll]] for (int y = 0; y < FF / 16; y++)
        sums[x][y] = coopmat_new(0.);

    for (int k = 0; k < I_WIDTH_32 / 16; k++)
    { // load to shared memory
      [[unroll]] for (int i = 0; i < TILE_HEIGHT * TILE_WIDTH / 2; i++)
        current_column_I[32 * i + id_loc] = coef_matrix_I(2*i + (id_loc >= 16 ? 1 : 0), 16*k + id_loc % 16);

      [[unroll]] for (int i = 0; i < 16; i++)
        [[unroll]] for (int j = 0; j < FF / 32; j++)
          current_line_W[i * FF + j * 32 + id_loc] = coef_matrix_W(16*k + i, 32*(j+batch*FF/32) + id_loc);

      // load the cooperative matrices
      [[unroll]] for (int i = 0; i < (TILE_WIDTH * TILE_HEIGHT) / 16; i++)
        coopmat_load_f16(coop_mat_column_I[i], current_column_I, 16*16*i, 16, false);

      [[unroll]] for (int i = 0; i < FF / 16; i++)
        coopmat_load_f16(coop_mat_line_W[i], current_line_W, 16*i, FF, false);

      // do the products
      [[unroll]] for (int i = 0; i < (TILE_WIDTH * TILE_HEIGHT) / 16; i++)
        [[unroll]] for (int j = 0; j < FF / 16; j++)
          sums[i][j] = coopmat_madd(coop_mat_column_I[i], coop_mat_line_W[j], sums[i][j]);
    }

    // export the results
    [[unroll]] for (int i = 0; i < (TILE_WIDTH * TILE_HEIGHT) / 16; i++)
    {
      [[unroll]] for (int j = 0; j < FF / 16; j++)
      {
        coopmat_store_f16(sums[i][j], current_column_I, 0, 16, false);
        const uint feature = 16*(j+batch*FF/16) + (id_loc % 16);

        // now we hold in our hands all the data for the [8x8] rows x cols
        // of the output tile for the first up to 16 output channels.
        // that is, we can perform max pooling for these features *now* and only write
        // a downsampled block of 4x4 rows and cols.
#ifdef PRE_POOL
        // most of the subgroup is doing different features (id_loc % 16).
        // the msb of id_loc (>=16?) indicates a column offset, together with p=0..8
        // this will let us cycle through 2x8 rows x cols of the output tile
        // in one subgroup.
        // to group together the 2x2 max pooling, we have to take the subgroup max of (id_loc msb=0,1) for the column
        // as well as the max of p<4 and p+4 in the same thread (for the row offset).
        // this can be done by first unrolling the p loop into two 0..4 and 4..8 manually, taking the max of value,
        // and then taking the subgroup max: max(x, subgroupShuffleXor(x, 16))
        // now write to a new location by only writing if id_loc < 16 and only once for p < 4 to img_row/2 and img_col/2.
        [[unroll]] for (int p = 0; p < 4; p++)
        {
          const uint line_O  = 16*i + 2*p + (id_loc >= 16 ? 1 : 0);
          const uint img_row = TILE_HEIGHT * gl_WorkGroupID.x + line_O / TILE_WIDTH;
          const uint img_col = TILE_WIDTH  * gl_WorkGroupID.y + line_O % TILE_WIDTH;
          float16_t v0 = current_column_I[32* p    + id_loc] + bias(feature);
          float16_t v1 = current_column_I[32*(p+4) + id_loc] + bias(feature);
          v0 = max(float16_t(0.0), v0); // relu
          v1 = max(float16_t(0.0), v1);
          // v0 = mix(v0, float16_t(0.2)*v0, v0 < float16_t(0.0)); // leaky relu
          // v1 = mix(v1, float16_t(0.2)*v1, v1 < float16_t(0.0));
          if(img_row   >= push.ht || img_col >= push.wd) v0 = float16_t(0.0);
          if(img_row+1 >= push.ht || img_col >= push.wd) v1 = float16_t(0.0);
          float16_t v = max(v0, v1);
          v = max(v, subgroupShuffleXor(v, 16));
          if(id_loc < 16 && feature < NB_OUTPUT_FEATURES)
            buf_out[OUTPUT_FEATURE_STRIDE * ((img_row/2) * ((push.wd+1)/2) + (img_col/2)) + feature] = v;
        }
#else
        [[unroll]] for (int p = 0; p < 16 / 2; p++)
        {
          const uint line_O  = 16*i + 2*p + (id_loc >= 16 ? 1 : 0);
          const uint img_row = TILE_HEIGHT * gl_WorkGroupID.x + line_O / TILE_WIDTH;
          const uint img_col = TILE_WIDTH  * gl_WorkGroupID.y + line_O % TILE_WIDTH;
          if(img_row < push.ht && img_col < push.wd && feature < NB_OUTPUT_FEATURES)
          { // compute the final value
            float16_t value = current_column_I[32*p + id_loc];
            value += bias(feature);           // bias
            value = max(value, float16_t(0)); // ReLU
            // value = mix(value, float16_t(0.2)*value, value < float16_t(0.0)); // leaky relu
            buf_out[OUTPUT_FEATURE_STRIDE * (img_row * push.wd + img_col) + feature] = value;
          }
        }
#endif
      }
    }
  }
}
