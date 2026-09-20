#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// basic pull/push inpainting with a bit of smoothing
// so it doesn't look completely fucked up.

typedef struct dt_inpaint_buf_t
{
  float *dat;      // image buffer
  uint32_t wd, ht; // width and height of the image
  uint32_t cpp;    // channels per pixel
}
dt_inpaint_buf_t;

static inline int
dt_inpaint_unset(const dt_inpaint_buf_t *b, int i, int j)
{ // assume empty/missing sample if all channels are zero
  if(j < 0 || j >= b->ht) return 1;
  if(i < 0 || i >= b->wd) return 1;
  for(int k=0;k<b->cpp;k++)
    if(b->dat[b->cpp*(b->wd*j + i) + k] != 0.0f) return 0;
  return 1;
}

// pull:
// blurs the source buffer s into the destination
// buffer d. will init/alloc all needed bits in d.
static inline void
dt_inpaint_blur(const dt_inpaint_buf_t *s, dt_inpaint_buf_t *d)
{
  d->cpp = s->cpp;
  d->wd = (s->wd + 1)/2;
  d->ht = (s->ht + 1)/2;
  d->dat = calloc(sizeof(float)*d->cpp, d->wd * (uint64_t)d->ht);
  const float wg[5] = {1.0, 4.0, 6.0, 4.0, 1.0};
  for(int j=0;j<d->ht;j++) for(int i=0;i<d->wd;i++)
  {
    float w = 0.0f;
    float px[10] = {0.0f};
    for(int bj=0;bj<2;bj++) for(int bi=0;bi<2;bi++)
    {
      for(int jj=-2;jj<=2;jj++) for(int ii=-2;ii<=2;ii++)
      {
        float ww = wg[2+jj]*wg[2+ii];
        if(!dt_inpaint_unset(s, 2*i+bi+ii,2*j+bj+jj))
        {
          for(int c=0;c<s->cpp;c++)
            px[c] += ww * s->dat[s->cpp*(s->wd*(2*j+bj+jj)+2*i+bi+ii)+c];
          w += ww;
        }
      }
    }
    // normalise or leave at 0 (unset)
    // write out to low-res buffer, binning 2x2 pixels into one output pixel:
    if(w > 0.0f)
      for(int c=0;c<d->cpp;c++)
        d->dat[d->cpp*(d->wd*j+i)+c] = px[c] / w;
  }
}

// push:
// fill the missing/unset bits in the fine buffer f by upsampling the information
// from the coarse buffer c (if it contains anything useful)
static inline void
dt_inpaint_fill(dt_inpaint_buf_t *f, const dt_inpaint_buf_t *c)
{
  for(int j=0;j<f->ht;j++) for(int i=0;i<f->wd;i++)
  {
#if 1
    if(dt_inpaint_unset(f, i, j))
    {
      for(int k=0;k<f->cpp;k++) f->dat[f->cpp*(f->wd*j + i)+k] = 0.0f;
      float w = 0.0f;
      for(int jj=-1;jj<=1;jj++) for(int ii=-1;ii<=1;ii++)
      {
        if(!dt_inpaint_unset(c, (i+ii)/2, (j+jj)/2))
        {
          for(int k=0;k<f->cpp;k++)
            f->dat[f->cpp*(f->wd*j + i)+k] += c->dat[c->cpp*(c->wd*((j+jj)/2)+(i+ii)/2)+k];
          w ++;
        }
      }
      for(int k=0;k<f->cpp;k++) f->dat[f->cpp*(f->wd*j + i)+k] /= w;
    }
#else // straight upsampling
    int jo = j/2, io = i/2;
    if(dt_inpaint_unset(f, i, j) && !dt_inpaint_unset(c, io, jo))
    { // could blur here too, but it seems kinda smooth already.
      for(int k=0;k<f->cpp;k++)
        f->dat[f->cpp*(f->wd*j + i)+k] = c->dat[c->cpp*(c->wd*jo+io)+k];
    }
#endif
  }
}

static inline void
dt_inpaint_rec(dt_inpaint_buf_t *b, int it)
{
  dt_inpaint_buf_t c;
  if(it <= 0) return;
  if(b->wd < 5 || b->ht < 5) return;
  dt_inpaint_blur(b, &c);
  dt_inpaint_rec(&c, it-1);
  dt_inpaint_fill(b, &c);
  free(c.dat);
}

static inline void
dt_inpaint(dt_inpaint_buf_t *b)
{
  dt_inpaint_rec(b, 10);
}

#if 0
// =========================
// variant with additional disambiguation capability
static inline int // return 1 if ij is better than uv
dt_inpaint_prio(const dt_inpaint_buf_t *b, int i, int j, int u, int v)
{ // assume empty/missing sample if first channel is zero
  if(j < 0 || j >= b->ht) return 0;
  if(i < 0 || i >= b->wd) return 0;
  if(v < 0 || v >= b->ht) return 1;
  if(u < 0 || u >= b->wd) return 1;
  if(dt_inpaint_unset(b, i, j)) return 0;
  if(dt_inpaint_unset(b, u, v)) return 1;
  // TODO make this a callback? now assuming c0 is the quadratic sigmoid coefficient. lower absolute value
  // means likely less saturated metamer, we'll prefer that:
  return fabsf(b->dat[b->cpp*(b->wd*j + i) + 0]) < fabsf(b->dat[b->cpp*(b->wd*v + u) + 0]);
}

// pull:
static inline void
dt_inpaint_blur2(const dt_inpaint_buf_t *s, dt_inpaint_buf_t *d)
{
  d->cpp = s->cpp;
  d->wd = (s->wd + 1)/2;
  d->ht = (s->ht + 1)/2;
  d->dat = calloc(sizeof(float)*d->cpp, d->wd * (uint64_t)d->ht);
  const float wg[5] = {1.0, 4.0, 6.0, 4.0, 1.0};
  for(int j=0;j<d->ht;j++) for(int i=0;i<d->wd;i++)
  {
    float w = 0.0f;
    float px[10] = {0.0f};
    for(int bj=0;bj<2;bj++) for(int bi=0;bi<2;bi++)
    {
      for(int jj=-2;jj<=2;jj++) for(int ii=-2;ii<=2;ii++)
      {
        float ww = wg[2+jj]*wg[2+ii];
        if(!dt_inpaint_unset(s, 2*i+bi+ii,2*j+bj+jj))
          if(dt_inpaint_prio(s, 2*i+bi+ii,2*j+bj+jj, 2*i+bi,2*j+bj))
        {
          for(int c=0;c<s->cpp;c++)
            px[c] += ww * s->dat[s->cpp*(s->wd*(2*j+bj+jj)+2*i+bi+ii)+c];
          w += ww;
        }
      }
    }
    // normalise or leave at 0 (unset)
    // write out to low-res buffer, binning 2x2 pixels into one output pixel:
    if(w > 0.0f)
      for(int c=0;c<d->cpp;c++)
        d->dat[d->cpp*(d->wd*j+i)+c] = px[c] / w;
  }
}

// push:
// fill the missing/unset bits in the fine buffer f by upsampling the information
// from the coarse buffer c (if it contains anything useful)
static inline void
dt_inpaint_fill2(dt_inpaint_buf_t *f, const dt_inpaint_buf_t *c)
{
  for(int j=0;j<f->ht;j++) for(int i=0;i<f->wd;i++)
  {
    // TODO always collect and then check prio
    if(dt_inpaint_unset(f, i, j))
    {
      float res[f->cpp];
      for(int k=0;k<f->cpp;k++) res[k] = 0.0f;//f->dat[f->cpp*(f->wd*j + i)+k] = 0.0f;
      float w = 0.0f;
      for(int jj=-1;jj<=1;jj++) for(int ii=-1;ii<=1;ii++)
      {
        if(!dt_inpaint_unset(c, (i+ii)/2, (j+jj)/2))
          // XXX need to compare between planes
          // if(dt_inpaint_prio(c, (i+ii)/2, (j+jj)/2), (i)/2, (j)/2)
        {
          for(int k=0;k<f->cpp;k++)
            res[k] += c->dat[c->cpp*(c->wd*((j+jj)/2)+(i+ii)/2)+k];
            // f->dat[f->cpp*(f->wd*j + i)+k] += c->dat[c->cpp*(c->wd*((j+jj)/2)+(i+ii)/2)+k];
          w ++;
        }
      }
      for(int k=0;k<f->cpp;k++) res[k] /= w;
      // if we are unset or the other prio is better, use res
      if(dt_inpaint_unset(f, i, j))
        for(int k=0;k<f->cpp;k++) f->dat[f->cpp*(f->wd*j + i)+k] = res[k];
      else if(fabsf(res[0]) < fabsf(f->dat[f->cpp*(f->wd*j + i)]))
        for(int k=0;k<f->cpp;k++) f->dat[f->cpp*(f->wd*j + i)+k] = res[k];
      // TODO evaluate residual?
      // for(int k=0;k<f->cpp;k++) f->dat[f->cpp*(f->wd*j + i)+k] /= w;
    }
  }
}

static inline void
dt_inpaint_rec2(dt_inpaint_buf_t *b, int it)
{
  dt_inpaint_buf_t c;
  if(it <= 0) return;
  if(b->wd < 5 || b->ht < 5) return;
  dt_inpaint_blur2(b, &c);
  dt_inpaint_rec2(&c, it-1);
  dt_inpaint_fill2(b, &c);
  free(c.dat);
}

static inline void
dt_inpaint2(dt_inpaint_buf_t *b)
{
  dt_inpaint_rec2(b, 10);
}
#endif
