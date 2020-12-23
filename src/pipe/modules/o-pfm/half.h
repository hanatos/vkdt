#pragma once
#include <stdint.h>
#include <xmmintrin.h>

// float->half variants.
// by Fabian "ryg" Giesen.
// https://gist.github.com/rygorous/2156668
//
// I hereby place this code in the public domain, as per the terms of the
// CC0 license:
//
//   https://creativecommons.org/publicdomain/zero/1.0/

static inline float half_to_float(uint16_t hi)
{
  typedef union FP32
  {
    uint32_t u;
    float f;
    struct
    {
      uint32_t Mantissa : 23;
      uint32_t Exponent : 8;
      uint32_t Sign : 1;
    };
  } FP32;
  typedef union FP16
  {
    uint16_t u;
    struct
    {
      uint16_t Mantissa : 10;
      uint16_t Exponent : 5;
      uint16_t Sign : 1;
    };
  } FP16;
  FP16 h = { hi };
  static const FP32 magic = { 113 << 23 };
  static const uint32_t shifted_exp = 0x7c00 << 13; // exponent mask after shift
  FP32 o;

  o.u = (h.u & 0x7fff) << 13;       // exponent/mantissa bits
  uint32_t exp = shifted_exp & o.u; // just the exponent
  o.u += (127 - 15) << 23;          // exponent adjust

  // handle exponent special cases
  if (exp == shifted_exp) // Inf/NaN?
    o.u += (128 - 16) << 23;    // extra exp adjust
  else if (exp == 0) // Zero/Denormal?
  {
    o.u += 1 << 23;             // extra exp adjust
    o.f -= magic.f;             // renormalize
  }

  o.u |= (h.u & 0x8000) << 16;    // sign bit
  return o.f;
}

// Approximate solution. This is faster but converts some sNaNs to
// infinity and doesn't round correctly. Handle with care.
static inline uint16_t float_to_half(float fi)
{
  typedef union FP32
  {
    uint32_t u;
    float f;
    struct
    {
      uint32_t Mantissa : 23;
      uint32_t Exponent : 8;
      uint32_t Sign : 1;
    };
  } FP32;
  typedef union FP16
  {
    uint16_t u;
    struct
    {
      uint16_t Mantissa : 10;
      uint16_t Exponent : 5;
      uint16_t Sign : 1;
    };
  } FP16;
  FP32 f = { .f = fi };
  FP32 f32infty = { 255 << 23 };
  FP32 f16max = { (127 + 16) << 23 };
  FP32 magic = { 15 << 23 };
  FP32 expinf = { (255 ^ 31) << 23 };
  uint32_t sign_mask = 0x80000000u;
  FP16 o = { 0 };

  uint32_t sign = f.u & sign_mask;
  f.u ^= sign;

  if (!(f.f < f32infty.u)) // Inf or NaN
    o.u = f.u ^ expinf.u;
  else
  {
    if (f.f > f16max.f) f.f = f16max.f;
    f.f *= magic.f;
  }

  o.u = f.u >> 13; // Take the mantissa bits
  o.u |= sign >> 16;
  return o.u;
}

// round-half-up (same as ISPC)
static inline __m128i float_to_half_sse(__m128 f)
{
#define CONSTF(name) _mm_castsi128_ps(name)
  __m128i mask_sign       = _mm_set1_epi32(0x80000000u);
  __m128i mask_round      = _mm_set1_epi32(~0xfffu);
  __m128i c_f32infty      = _mm_set1_epi32(255 << 23);
  __m128i c_magic         = _mm_set1_epi32(15 << 23);
  __m128i c_nanbit        = _mm_set1_epi32(0x200);
  __m128i c_infty_as_fp16 = _mm_set1_epi32(0x7c00);
  __m128i c_clamp         = _mm_set1_epi32((31 << 23) - 0x1000);

  __m128  msign       = CONSTF(mask_sign);
  __m128  justsign    = _mm_and_ps(msign, f);
  __m128i f32infty    = c_f32infty;
  __m128  absf        = _mm_xor_ps(f, justsign);
  __m128  mround      = CONSTF(mask_round);
  __m128i absf_int    = _mm_castps_si128(absf); // pseudo-op, but val needs to be copied once so count as mov
  __m128i b_isnan     = _mm_cmpgt_epi32(absf_int, f32infty);
  __m128i b_isnormal  = _mm_cmpgt_epi32(f32infty, _mm_castps_si128(absf));
  __m128i nanbit      = _mm_and_si128(b_isnan, c_nanbit);
  __m128i inf_or_nan  = _mm_or_si128(nanbit, c_infty_as_fp16);

  __m128  fnosticky   = _mm_and_ps(absf, mround);
  __m128  scaled      = _mm_mul_ps(fnosticky, CONSTF(c_magic));
  __m128  clamped     = _mm_min_ps(scaled, CONSTF(c_clamp)); // logically, we want PMINSD on "biased", but this should gen better code
  __m128i biased      = _mm_sub_epi32(_mm_castps_si128(clamped), _mm_castps_si128(mround));
  __m128i shifted     = _mm_srli_epi32(biased, 13);
  __m128i normal      = _mm_and_si128(shifted, b_isnormal);
  __m128i not_normal  = _mm_andnot_si128(b_isnormal, inf_or_nan);
  __m128i joined      = _mm_or_si128(normal, not_normal);

  __m128i sign_shift  = _mm_srli_epi32(_mm_castps_si128(justsign), 16);
  __m128i final       = _mm_or_si128(joined, sign_shift);

  // ~20 SSE2 ops
  return final;
#undef CONSTF
}
