#pragma once
#include <stdint.h>
#include <math.h>
#include <assert.h>
#define clamp(X, A, B) fminf(B, fmaxf(A, X))
#define UB 8
#define VB 8
#define AB 12
#define WB 8
#define SB 8
#define Um 200
#define UM 900
#define Vm -100.0 //-550.0
#define VM 50.0 // 150.0
#define Wm 0.0
#define WM 2500.0
#define Sm 0.0
#define SM 1.0
// #define Vm -0.001
// #define VM 0.6
// #define Am -0.004
// #define AM 0.001
#define Am -0.0025
#define AM 0.001

static inline uint32_t quant_fixed(double u, double m, double M, int bits)
{
  u = (u-m)/(M-m);
  return clamp((uint32_t)round(u * (1u<<bits)), 0u, (1u<<bits)-1u);
}

static inline double dequant_fixed(uint32_t ui, double m, double M, int bits)
{
  return ui / (double)(1u<<bits) * (M-m) + m;
}

static inline uint32_t quant_float(float u, float m, float M, int bits)
{
  float v = clamp(u, m, M);
  const uint32_t sv = u < 0 ? 1 : 0; // vi>>31;
  if(sv)
  {
    float t = m;
    m = -M;
    M = -t;
    v = -v;
  }
  if(m < 0.0) m = 0.0;
  assert(M >= 0.0);
  if(M < 0.0) M = 0.0; // this should never happen
  assert(m >= 0.0);
  assert(v >= 0.0);
  const uint32_t mi = *(uint32_t *)&m;
  const uint32_t Mi = *(uint32_t *)&M;
  const uint32_t vi = *(uint32_t *)&v;
  return (sv<<(bits-1)) | quant_fixed(vi, mi, Mi, bits-1);
}

static inline float dequant_float(uint32_t q, float m, float M, int bits)
{
  const uint32_t sv = q>>(bits-1);
  float m2 = m, M2 = M;
  if(sv)
  {
    m2 = -M;
    M2 = -m;
  }
  if(m2 < 0.0) m2 = 0.0;
  assert(M2 >= 0.0);
  // if(M2 < 0.0) M2 = 0.0; // this should never happen
  assert(m2 >= 0.0);
  const uint32_t mi = *(uint32_t *)&m2;
  const uint32_t Mi = *(uint32_t *)&M2;
  const double dq = dequant_fixed(q & ~(1u<<(bits-1)), mi, Mi, bits-1);
  const uint32_t qi = round(dq);
  float f = *(float *)&qi;
  assert(f==f);
  if(sv) assert(-f >= m);
  if(sv) assert(-f <= M);
  if(!sv) assert(f >= m);
  if(!sv) assert(f <= M);
  if(sv) return -f;
  return f;
}

