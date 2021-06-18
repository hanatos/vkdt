/*
    This file is part of corona-6: radiata.

    corona-6: radiata is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    corona-6: radiata is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with corona-6: radiata.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "core/core.h"
#include <math.h>
#include <inttypes.h>


#define cross(v1, v2, res) \
  (res)[0] = (v1)[1]*(v2)[2] - (v2)[1]*(v1)[2];\
  (res)[1] = (v1)[2]*(v2)[0] - (v2)[2]*(v1)[0];\
  (res)[2] = (v1)[0]*(v2)[1] - (v2)[0]*(v1)[1]

#define dot(u, v) ((u)[0]*(v)[0] + (u)[1]*(v)[1] + (u)[2]*(v)[2])

typedef struct
{
  float w, x[3];
}
quat_t;

static inline void
quat_init(quat_t *q, float w, float x, float y, float z)
{
  q->w = w;
  q->x[0] = x;
  q->x[1] = y;
  q->x[2] = z;
}

static inline void
quat_init_angle(
    quat_t *q,
    const float angle,
    float x,
    float y,
    float z)
{
  float ilen = 1.0f/sqrtf(x*x+y*y+z*z);
  float tmp = sinf(angle/2.0f);
  q->w = cosf(angle/2.0f);
  q->x[0] = tmp * x * ilen;
  q->x[1] = tmp * y * ilen;
  q->x[2] = tmp * z * ilen;
}

static inline void
quat_add(quat_t *q, const quat_t *a)
{
  for(int k=0;k<3;k++) q->x[k] += a->x[k];
  q->w += a->w;
}

static inline void
quat_muls(quat_t *q, const float s)
{
  for(int k=0;k<3;k++) q->x[k] *= s;
  q->w *= s;
}

static inline void
quat_conj(quat_t *q)
{
  for(int k=0;k<3;k++) q->x[k] = -q->x[k];
}

// composite transform will first eval b, then a
static inline void
quat_mul(const quat_t *a, const quat_t *b, quat_t *res)
{
  res->x[0] = a->w*b->x[0] + a->x[0]*b->w    + a->x[1]*b->x[2] - a->x[2]*b->x[1];
  res->x[1] = a->w*b->x[1] - a->x[0]*b->x[2] + a->x[1]*b->w    + a->x[2]*b->x[0];
  res->x[2] = a->w*b->x[2] + a->x[0]*b->x[1] - a->x[1]*b->x[0] + a->x[2]*b->w;
  res->w    = a->w*b->w    - a->x[0]*b->x[0] - a->x[1]*b->x[1] - a->x[2]*b->x[2];
}

static inline void
quat_transform_inv(const quat_t *const q, float* p)
{ // q' * v * q
  quat_t v;
  quat_init(&v, 0.0f, p[0], p[1], p[2]);

  quat_t conj = *q;
  quat_conj(&conj);

  quat_t tmp;
  quat_mul(&conj, &v, &tmp);
  quat_t res;
  quat_mul(&tmp, q, &res);
  for(int k=0;k<3;k++)
    p[k] = res.x[k];
}

static inline void
quat_transform(const quat_t *const q, float* p)
{
  // q * v * q'
  quat_t v;
  quat_init(&v, 0.0f, p[0], p[1], p[2]);

  quat_t conj = *q;
  quat_conj(&conj);

  quat_t tmp;
  quat_mul(q, &v, &tmp);
  quat_t res;
  quat_mul(&tmp, &conj, &res);
  for(int k=0;k<3;k++)
    p[k] = res.x[k];
}

static inline float
quat_magnitude(quat_t *q)
{
  return sqrtf(q->x[0]*q->x[0] + q->x[1]*q->x[1] + q->x[2]*q->x[2] + q->w*q->w);
}

static inline void
quat_normalise(quat_t *q)
{
  const float l = quat_magnitude(q);
  for(int k=0;k<3;k++) q->x[k] /= l;
  q->w /= l;
}

static inline void
quat_lerp(const quat_t *const q, const quat_t *const p, const float t, quat_t *res)
{
  res->w = (1.0f-t)*q->w + t*p->w;
  for(int k=0;k<3;k++)
    res->x[k] = (1.0f-t)*q->x[k] + t*p->x[k];
  // TODO: i think the full quat should be re-normalised here.
}

// t = 1 => output will be p. t=1 => q
static inline void
quat_slerp(
    const quat_t *const q,
    const quat_t *const p,
    const float t,
    quat_t *res)
{
  float cos_theta_2 = q->w * p->w + (q->x[0]*p->x[0] + q->x[1]*p->x[1] + q->x[2]*p->x[2]);
  if(cos_theta_2 < 0.0f)
  { // shortest path:
    quat_t mp = *p;
    quat_muls(&mp, -1.0f);
    return quat_slerp(q, &mp, t, res);
  }
  if(fabsf(cos_theta_2) >= 1.0f)
  {
    *res = *q;
    return;
  }
  float theta_2 = acosf(cos_theta_2);
  float sin_theta_2 = sqrtf(1.0f - cos_theta_2*cos_theta_2);
  if(fabsf(sin_theta_2) < 1e-10f)
  {
    res->w = (q->w + p->w)*.5f;
    for(int k=0;k<3;k++) res->x[k] = (q->x[k] + p->x[k])*.5f;
    return;
  }
  float a = sinf((1.0f - t) * theta_2) / sin_theta_2;
  float b = sinf(t * theta_2) / sin_theta_2;
  res->w = q->w * a + p->w * b;
  for(int k=0;k<3;k++)
    res->x[k] = q->x[k] * a + p->x[k] * b;
}

static inline void
quat_to_euler(
    const quat_t *q,
    float *roll,
    float *pitch,
    float *yaw)
{
  float ysqr = q->x[1]*q->x[1];
  // roll (x)
  float t0 = 2.0f * (q->w * q->x[0] + q->x[1] * q->x[2]);
  float t1 = 1.0f - 2.0f * (q->x[0] * q->x[0] + ysqr);
  *pitch = atan2f(t0, t1);

  // pitch (y)
  float t2 = 2.0f * (q->w * q->x[1] - q->x[2] * q->x[0]);
  t2 = CLAMP(t2, -1.0f, 1.0f);
  *yaw = asinf(t2);

  // yaw (z)
  float t3 = 2.0f * (q->w * q->x[2] + q->x[0] * q->x[1]);
  float t4 = 1.0f - 2.0f * (ysqr + q->x[2] * q->x[2]);
  *roll = atan2f(t3, t4);
}

static inline void
quat_from_euler(
    quat_t *q,
    float roll,
    float pitch,
    float yaw)
{
  float t0 = cosf(roll * 0.5f);
  float t1 = sinf(roll * 0.5f);
  float t2 = cosf(pitch * 0.5f);
  float t3 = sinf(pitch * 0.5f);
  float t4 = cosf(yaw * 0.5f);
  float t5 = sinf(yaw * 0.5f);

  q->w    = t0 * t2 * t4 + t1 * t3 * t5;
  q->x[0] = t0 * t3 * t4 - t1 * t2 * t5;
  q->x[1] = t0 * t2 * t5 + t1 * t3 * t4;
  q->x[2] = t1 * t2 * t4 - t0 * t3 * t5;
  quat_normalise(q);
}

// cast to matrix. the reverse way is ugly, see
// https://github.com/g-truc/glm/blob/0.9.8.0/glm/gtc/quaternion.inl#L627
// and i hope we never need it.
// as compared to glm this is transposed, because our matrices are column major.
static inline void
quat_to_mat3(const quat_t *q, float *m)
{
  float qxx = q->x[0] * q->x[0];
  float qyy = q->x[1] * q->x[1];
  float qzz = q->x[2] * q->x[2];
  float qxz = q->x[0] * q->x[2];
  float qxy = q->x[0] * q->x[1];
  float qyz = q->x[1] * q->x[2];
  float qwx = q->w * q->x[0];
  float qwy = q->w * q->x[1];
  float qwz = q->w * q->x[2];

  m[0] = 1.0f - 2.0f * (qyy +  qzz);
  m[3] = 2.0f * (qxy + qwz);
  m[6] = 2.0f * (qxz - qwy);

  m[1] = 2.0f * (qxy - qwz);
  m[4] = 1.0f - 2.0f * (qxx +  qzz);
  m[7] = 2.0f * (qyz + qwx);

  m[2] = 2.0f * (qxz + qwy);
  m[5] = 2.0f * (qyz - qwx);
  m[8] = 1.0f - 2.0f * (qxx +  qyy);
}
