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
{ // assume empty/missing sample if first channel is zero
  if(j < 0 || j >= b->ht) return 1;
  if(i < 0 || i >= b->wd) return 1;
  return b->dat[b->cpp*(b->wd*j + i) + 0] == 0.0f;
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
