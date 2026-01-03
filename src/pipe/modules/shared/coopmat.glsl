// silly compatibility wrapper around the
// vulkan cooperative matrix extension.
// fills in the code for older/incompatible gpus, but isn't very
// smart about it. this is more an emergency fallback.
// it implements exactly float16 16x16 matrix multiply add.

#ifndef VKDT_COOPMAT_FALLBACK
#extension GL_KHR_cooperative_matrix         : enable
// this is the type of matrix we're using everywhere:
#define coopmat_A_t coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseA>
#define coopmat_B_t coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseB>
#define coopmat_C_t coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseAccumulator>
#define coopmat_load(frag, mem, idx, stride, colmajor) coopMatLoad(frag, mem, idx, stride, colmajor)
#define coopmat_store(frag, mem, idx, stride, colmajor) coopMatStore(frag, mem, idx, stride, colmajor)
#define coopmat_load_f16(frag, mem, idx, stride, colmajor) coopMatLoad(frag, mem, idx, stride, colmajor)
#define coopmat_store_f16(frag, mem, idx, stride, colmajor) coopMatStore(frag, mem, idx, stride, colmajor)
#define coopmat_madd(A, B, C) coopMatMulAdd(A, B, C, 0)
#define coopmat_A_new(A) coopmat_A_t(A)
#define coopmat_B_new(A) coopmat_B_t(A)
#define coopmat_C_new(A) coopmat_C_t(A)
#else
// uh, sorry about what follows:
shared float16_t As[16*16*2]; // gl_NumSubgroups is not a constant. gl_SubgroupSize == 32 is something we assume
shared float16_t Bs[16*16*2]; // oh ffs whatever, i'll just hardcode it fu glsl. (2 == N_BLOCKS)
// interestingly, the following even works to define stuff like `coopmat_t frag[2]`:
#define coopmat_A_t float16_t[8]
#define coopmat_B_t float16_t[8]
#define coopmat_C_t float16_t[8]
#define coopmat_A_new(A) float16_t[8](float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A))
#define coopmat_B_new(A) float16_t[8](float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A))
#define coopmat_C_new(A) float16_t[8](float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A),float16_t(A))

#define coopmat_load(frag, buf, element, stride, colmajor)\
do {\
  const uint lane = gl_SubgroupInvocationID, el = 256*gl_SubgroupID;\
  uint row = lane / 2, col = lane % 2;\
  uvec4 load = buf[element + row * stride + col];\
  As[el + 16*row + 8*col + 0] = unpackFloat2x16(load.x).x;\
  As[el + 16*row + 8*col + 1] = unpackFloat2x16(load.x).y;\
  As[el + 16*row + 8*col + 2] = unpackFloat2x16(load.y).x;\
  As[el + 16*row + 8*col + 3] = unpackFloat2x16(load.y).y;\
  As[el + 16*row + 8*col + 4] = unpackFloat2x16(load.z).x;\
  As[el + 16*row + 8*col + 5] = unpackFloat2x16(load.z).y;\
  As[el + 16*row + 8*col + 6] = unpackFloat2x16(load.w).x;\
  As[el + 16*row + 8*col + 7] = unpackFloat2x16(load.w).y;\
  subgroupMemoryBarrierShared();\
  for(int j=0;j<8;j++)\
  {\
    uint row = (32*j + lane) / 16, col = lane % 16;\
    if(colmajor==1) frag[j] = As[el + 16*col + row];\
    else            frag[j] = As[el + 16*row + col];\
  }\
} while(false)

#define coopmat_store(frag, buf, element, stride, colmajor)\
do {\
  const uint lane = gl_SubgroupInvocationID, el = 256*gl_SubgroupID;\
  for(int j=0;j<8;j++)\
  {\
    uint row = (32*j + lane) / 16, col = lane % 16;\
    if(colmajor==1) As[el + 16*col + row] = frag[j];\
    else            As[el + 16*row + col] = frag[j];\
  }\
  subgroupMemoryBarrierShared();\
  uint row = lane / 2, col = lane % 2;\
  buf[element + row * stride + col] = uvec4(\
      packFloat2x16(f16vec2(As[el + 16*row + 8*col + 0], As[el + 16*row + 8*col + 1])),\
      packFloat2x16(f16vec2(As[el + 16*row + 8*col + 2], As[el + 16*row + 8*col + 3])),\
      packFloat2x16(f16vec2(As[el + 16*row + 8*col + 4], As[el + 16*row + 8*col + 5])),\
      packFloat2x16(f16vec2(As[el + 16*row + 8*col + 6], As[el + 16*row + 8*col + 7])));\
} while(false)

#define coopmat_load_f16(frag, buf, element, stride, colmajor)\
do {\
  uint lane = gl_SubgroupInvocationID;\
  subgroupMemoryBarrierShared();\
  for(int j=0;j<8;j++)\
  {\
    uint row = (32*j + lane) / 16, col = lane % 16;\
    frag[j] = buf[element + stride*row + col];\
  }\
} while(false)

#define coopmat_store_f16(frag, buf, element, stride, colmajor)\
do {\
  uint lane = gl_SubgroupInvocationID;\
  for(int j=0;j<8;j++)\
  {\
    uint row = (32*j + lane) / 16, col = lane % 16;\
    buf[element + stride*row + col] = frag[j];\
  }\
  subgroupMemoryBarrierShared();\
} while(false)

// return A*B + C
coopmat_C_t coopmat_madd(in coopmat_A_t A, in coopmat_B_t B, in coopmat_C_t C)
{
  const uint el = 256*gl_SubgroupID;
  coopmat_store_f16(A, As, el, 16, false);
  coopmat_store_f16(B, Bs, el, 16, false);

  uint lane = gl_SubgroupInvocationID;
  for(int i=0;i<8;i++)
  { // every thread computes 8 output values in C:
    uint row = (32*i + lane) / 16, col = lane % 16;
    for(int k=0;k<16;k++)
      C[i] += As[el + 16*row + k] * Bs[el + 16*k + col];
      // DEBUG: rule out some f16 problems:
      // C[i] = float16_t(clamp(float(C[i]) + float(As[el + 16*row + k]) * float(Bs[el + 16*k + col]), -65000.0, 65000.0));
  }
  return C;
}
#endif
