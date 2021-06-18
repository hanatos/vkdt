#pragma once

#define GEO_MAGIC 0xc01337
#define GEO_VERSION 2

#include <stdint.h>
#include <assert.h>
#include <math.h>
// #include "half.h"

// convenience struct for SSE things
typedef union
{
  __m128 m;
  float f[4];
  unsigned int i[4];
}
float4_t;

#define dotproduct(u, v) ((u)[0]*(v)[0] + (u)[1]*(v)[1] + (u)[2]*(v)[2])
static inline void normalise(float *f)
{
  const float len = 1.0f/sqrtf(dotproduct(f, f));
  for(int k=0;k<3;k++) f[k] *= len;
}

static inline float tofloat(uint32_t i)
{
  union { uint32_t i; float f; } u = {.i=i};
  return u.f;
}

static inline uint32_t touint(float f)
{
  union { float f; uint32_t i; } u = {.f=f};
  return u.i;
}

static inline int geo_normal_valid(const uint32_t enc)
{
  uint16_t *projected = (uint16_t *)&enc;
  // can either have highest non-sign bit set or any of the following.
  // this makes sure that we do not exceed the [-1,1] range.
  return (projected[0] == 0 || ((projected[0] & 0x4000) ^ (projected[0] & 0x3fff))) &&
         (projected[1] == 0 || ((projected[1] & 0x4000) ^ (projected[1] & 0x3fff)));
}

// 32-bit normal encoding from Journal of Computer Graphics Techniques Vol. 3, No. 2, 2014
// A Survey of Efficient Representations for Independent Unit Vectors
// almost like oct30, but our error is = 0.00077204 avg = 0.00010846 compared to oct32P 0.00246 0.00122 
// i'm thinking maybe because we use a fixed point quantization (only multiples of 2 are mul/divd here)
// this also enables us to precisely encode (1 0 0) vectors.
static inline void geo_decode_normal(const uint32_t enc, float vec[3])
{
  uint16_t *projected = (uint16_t *)&enc;
  assert(geo_normal_valid(enc));
  // copy sign bit and paste rest to upper mantissa to get float encoded in [1,2).
  // the employed bits will only go to [1,1.5] for valid encodings.
  uint32_t vec0 = 0x3f800000 | ((projected[0] & 0x7fff)<<8);
  uint32_t vec1 = 0x3f800000 | ((projected[1] & 0x7fff)<<8);
  // transform to [-1,1] to be able to precisely encode the important academic special cases {-1,0,1}
  vec[0] = tofloat(touint(2.0f*tofloat(vec0) - 2.0f) | ((projected[0] & 0x8000)<<16));
  vec[1] = tofloat(touint(2.0f*tofloat(vec1) - 2.0f) | ((projected[1] & 0x8000)<<16));
  vec[2] = 1.0f - (fabsf(vec[0]) + fabsf(vec[1]));

  if (vec[2] < 0.0f)
  {
    float oldX = vec[0];
    vec[0] = (1.0f - fabsf(vec[1])) * ((oldX < 0.0f) ? -1.0f : 1.0f);
    vec[1] = (1.0f - fabsf(oldX))   * ((vec[1] < 0.0f) ? -1.0f : 1.0f);
  }
  normalise(vec);
}

static inline uint32_t geo_encode_normal(const float vec[3])
{
  uint32_t enc;
  uint16_t *projected = (uint16_t *)&enc;
  const float invL1Norm = 1.0f / (fabsf(vec[0]) + fabsf(vec[1]) + fabsf(vec[2]));

  // first find floating point values of octahedral map in [-1,1]:
  float enc0, enc1;
  if (vec[2] < 0.0f)
  {
    enc0 = (1.0f - fabsf(vec[1] * invL1Norm)) * ((vec[0] < 0.0f) ? -1.0f : 1.0f);
    enc1 = (1.0f - fabsf(vec[0] * invL1Norm)) * ((vec[1] < 0.0f) ? -1.0f : 1.0f);
  }
  else
  {
    enc0 = vec[0] * invL1Norm;
    enc1 = vec[1] * invL1Norm;
  }
  // then encode:
  uint32_t enci0 = touint((fabsf(enc0) + 2.0f)/2.0f);
  uint32_t enci1 = touint((fabsf(enc1) + 2.0f)/2.0f);
  // copy over sign bit and truncated mantissa. could use rounding for increased precision here.
  projected[0] = ((touint(enc0) & 0x80000000)>>16) | ((enci0 & 0x7fffff)>>8);
  projected[1] = ((touint(enc1) & 0x80000000)>>16) | ((enci1 & 0x7fffff)>>8);
  // avoid -0 cases:
  if((projected[0] & 0x7fff) == 0) projected[0] = 0;
  if((projected[1] & 0x7fff) == 0) projected[1] = 0;
  return enc;
}

#if 0
static inline uint32_t geo_encode_uv(const float u, const float v)
{
  uint16_t enc16[2];
  enc16[0] = float_to_half(u);
  enc16[1] = float_to_half(v);
  return *(uint32_t *)enc16;
}

static inline void geo_decode_uv(const uint32_t enc, float *u, float *v)
{
  const uint16_t *enc16 = (const uint16_t *)&enc;
  *u = half_to_float(enc16[0]);
  *v = half_to_float(enc16[1]);
}

static inline uint32_t geo_encode_uvw(const float u, const float v, const float w)
{ // 11 11 10 fixed point, assume input is in [0, 1)
  return (((int)CLAMP(u*2048, 0, 2047)) << 21) | (((int)CLAMP(v*2048, 0, 2047)) << 10) | (int)CLAMP(w*1024, 0, 1023);
}

static inline void geo_decode_uvw(const uint32_t enc, float *uvw)
{
  uvw[0] = (enc >> 21)/2048.0f;
  uvw[1] = ((enc&0x1ffc00) >> 10)/2048.0f;
  uvw[2] = (enc&0x3ff)/1024.0f;
}
#endif

static inline int geo_get_vertex_count(const prims_t *p, primid_t pi)
{
  return pi.vcnt;
}

static inline const float *geo_get_vertex(const prims_t *p, primid_t pi, int v)
{
  prims_shape_t *s = p->shape + pi.shapeid;
  return s->vtx[(pi.mb+1)*s->vtxidx[pi.vi + v].v].v;
}

static inline const float *geo_get_vertex_shutter_close(const prims_t *p, primid_t pi, int v)
{
  const prims_shape_t *s = p->shape + pi.shapeid;
  return s->vtx[(pi.mb+1)*s->vtxidx[pi.vi + v].v + pi.mb].v;
}

static inline void geo_get_vertex_time(const prims_t *p, primid_t pi, int v, float time, float4_t *vtx)
{
  const prims_shape_t *s = p->shape + pi.shapeid;
  if(pi.mb)
  {
    // for(int k=0;k<3;k++)
    //   vtx[k] = (1.0f-time)*s->vtx[2*s->vtxidx[pi.vi + v].v].v[k] +
    //     time*s->vtx[2*s->vtxidx[pi.vi + v].v + 1].v[k];
    vtx->m = _mm_add_ps(
        _mm_mul_ps(_mm_set1_ps(1.0f-time), s->vtx[2*s->vtxidx[pi.vi + v].v].m),
        _mm_mul_ps(_mm_set1_ps(     time), s->vtx[2*s->vtxidx[pi.vi + v].v + 1].m));
  }
  else
  {
    // const float *vt = s->vtx[s->vtxidx[pi.vi + v].v].v;
    // for(int k=0;k<3;k++) vtx[k] = vt[k];
    vtx->m = *(__m128*)s->vtx[s->vtxidx[pi.vi + v].v].v;
  }
}

static inline void geo_get_normal_shutter_open(const prims_t *p, primid_t pi, int v, float *n)
{
  const prims_shape_t *s = p->shape + pi.shapeid;
  geo_decode_normal(s->vtx[(pi.mb+1)*s->vtxidx[pi.vi+v].v].n, n);
}

static inline void geo_get_normal_shutter_close(const prims_t *p, primid_t pi, int v, float *n)
{
  const prims_shape_t *s = p->shape + pi.shapeid;
  geo_decode_normal(s->vtx[(pi.mb+1)*s->vtxidx[pi.vi+v].v+pi.mb].n, n);
}

static inline void geo_get_normal_time(const prims_t *p, primid_t pi, int v, float time, float *n)
{
  if(pi.mb)
  {
    float n0[3], n1[3];
    geo_get_normal_shutter_open(p, pi, v, n0);
    geo_get_normal_shutter_close(p, pi, v, n1);
    for(int k=0;k<3;k++) n[k] = (1.0f-time)*n0[k] + time*n1[k];
  }
  else geo_get_normal_shutter_open(p, pi, v, n);
}

#if 0
static inline void geo_get_uv(const prims_t *p, primid_t pi, const int vtx, float *uv)
{
  const prims_shape_t *s = p->shape + pi.shapeid;
  if(pi.vcnt == s_prim_line)
    geo_decode_uvw(s->vtxidx[pi.vi + vtx].uv, uv);
  else
    geo_decode_uv(s->vtxidx[pi.vi + vtx].uv, uv, uv+1);
}
#endif


#if 0
// some helpers related to prims_shape_t which aren't needed during run-time of the renderer.

// find geometric vertex normals n0 at shutter open and n1 at shutter close.
// these may depend on the vertex for non-planar quads.
static inline void geo_get_normal(const prims_shape_t *s, const primid_t p, const int v, float *n0, float *n1)
{
  float *v0, *v1, *v2;
  for(int m=0;m<p.mb+1;m++)
  {
    if(p.vcnt == 3)
    {
      v0 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+0].v+m].v;
      v1 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+1].v+m].v;
      v2 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+2].v+m].v;
    }
    else if(p.vcnt == 4)
    {
      if(v == 0) v0 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+3].v+m].v;
      else       v0 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+v-1].v+m].v;
      v1 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+v].v+m].v;
      if(v == 3) v2 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+0].v+m].v;
      else       v2 = s->vtx[(p.mb+1)*s->vtxidx[p.vi+v+1].v+m].v;
    }
    else return;
    float *n = n0;
    if(m) n = n1;
    n[0] = (v1[1] - v0[1])*(v2[2] - v0[2]) - (v1[2] - v0[2])*(v2[1] - v0[1]);
    n[1] = (v1[2] - v0[2])*(v2[0] - v0[0]) - (v1[0] - v0[0])*(v2[2] - v0[2]);
    n[2] = (v1[0] - v0[0])*(v2[1] - v0[1]) - (v1[1] - v0[1])*(v2[0] - v0[0]);
  }
}

// recompute vertex normals by averaging the adjacent geometric face normals by area
static inline void geo_recompute_normals(
    prims_shape_t *shape)
{
  // recompute normals:
  const int mb = shape->primid[0].mb+1;
  const uint32_t num_verts = (shape->data_size - ((uint8_t *)shape->vtx - (uint8_t *)shape->data))/(sizeof(prims_vtx_t) * mb);
  float *n = (float *)malloc(sizeof(float)*3*num_verts*mb);
  memset(n, 0, sizeof(float)*3*num_verts*mb);
  for(uint64_t s=0;s<shape->num_prims;s++)
  { // go through all faces, compute geometric normal (not normalised for area weight)
    float n0[3], n1[3];
    const primid_t p = shape->primid[s];
    for(int k=0;k<p.vcnt;k++)
    {
      geo_get_normal(shape, p, k, n0, n1);
      for(int m=0;m<mb;m++)
        for(int i=0;i<3;i++)
          n[3*(mb*(shape->vtxidx[p.vi+k].v)+m)+i] += m ? n1[i] : n0[i];
    }
  }
  // go through all vertices, write back/encode (will l1-normalise)
#ifdef _OPENMP
#pragma omp parallel for schedule(static) default(shared)
#endif
  for(int i=0;i<num_verts*mb;i++)
    shape->vtx[i].n = geo_encode_normal(n + 3*i);
  free(n);
}
#endif
