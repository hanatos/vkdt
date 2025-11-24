#pragma once

static inline void
mat3mulv(float *vo, const float *const M, const float *const vi)
{
  for(int i=0; i<3; i++) vo[i] = 0.0f;
  for(int k=0; k<3; k++) for(int i=0; i<3; i++)
    vo[k] += M[3*k+i] * vi[i];
}

static inline void
mat3mul(float *dst, const float *const m1, const float *const m2)
{
  for(int i=0; i<9; i++) dst[i] = 0.0f;
  for(int k=0; k<3; k++)
    for(int i=0; i<3; i++)
      for(int j=0; j<3; j++) dst[3*k+i] += m1[3*k+j] * m2[3*j+i];
}

static inline int
mat3inv(float *const dst, const float *const src)
{
#define A(y, x) src[(y - 1) * 3 + (x - 1)]
#define B(y, x) dst[(y - 1) * 3 + (x - 1)]
  const float det = A(1, 1) * (A(3, 3) * A(2, 2) - A(3, 2) * A(2, 3))
                  - A(2, 1) * (A(3, 3) * A(1, 2) - A(3, 2) * A(1, 3))
                  + A(3, 1) * (A(2, 3) * A(1, 2) - A(2, 2) * A(1, 3));
  const float epsilon = 1e-7f;
  if(det > -epsilon && det < epsilon) return 1;
  const float idet = 1.f / det;
  B(1, 1) =  idet * (A(3, 3) * A(2, 2) - A(3, 2) * A(2, 3));
  B(1, 2) = -idet * (A(3, 3) * A(1, 2) - A(3, 2) * A(1, 3));
  B(1, 3) =  idet * (A(2, 3) * A(1, 2) - A(2, 2) * A(1, 3));

  B(2, 1) = -idet * (A(3, 3) * A(2, 1) - A(3, 1) * A(2, 3));
  B(2, 2) =  idet * (A(3, 3) * A(1, 1) - A(3, 1) * A(1, 3));
  B(2, 3) = -idet * (A(2, 3) * A(1, 1) - A(2, 1) * A(1, 3));

  B(3, 1) =  idet * (A(3, 2) * A(2, 1) - A(3, 1) * A(2, 2));
  B(3, 2) = -idet * (A(3, 2) * A(1, 1) - A(3, 1) * A(1, 2));
  B(3, 3) =  idet * (A(2, 2) * A(1, 1) - A(2, 1) * A(1, 2));
#undef A
#undef B
  return 0;
}
