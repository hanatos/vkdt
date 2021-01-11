#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// basic pull/push inpainting with a bit of smoothing
// so it doesn't look completely fucked up.

typedef struct buf_t
{
  float *dat;      // image buffer
  uint32_t wd, ht; // width and height of the image
  uint32_t cpp;    // channels per pixel
}
buf_t;

static inline int
unset(const buf_t *b, int i, int j)
{ // assume empty/missing sample if first channel is zero
  if(j < 0 || j >= b->ht) return 1;
  if(i < 0 || i >= b->wd) return 1;
  return b->dat[b->cpp*(b->wd*j + i) + 0] == 0.0f;
}

// pull:
// blurs the source buffer s into the destination
// buffer d. will init/alloc all needed bits in d.
static inline void
blur(const buf_t *s, buf_t *d)
{
  d->cpp = s->cpp;
  d->wd = (s->wd + 1)/2;
  d->ht = (s->ht + 1)/2;
  d->dat = calloc(sizeof(float)*d->cpp, d->wd * d->ht);
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
        if(!unset(s, 2*i+bi+ii,2*j+bj+jj))
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
fill(buf_t *f, const buf_t *c)
{
  for(int j=0;j<f->ht;j++) for(int i=0;i<f->wd;i++)
  {
    int jo = j/2, io = i/2;
    if(unset(f, i, j) && !unset(c, io, jo))
    { // could blur here too, but it seems kinda smooth already.
      for(int k=0;k<f->cpp;k++)
        f->dat[f->cpp*(f->wd*j + i)+k] = c->dat[c->cpp*(c->wd*jo+io)+k];
    }
  }
}

static inline void
inpaint_rec(buf_t *b, int it)
{
  buf_t c;
  if(it <= 0) return;
  if(b->wd < 5 || b->ht < 5) return;
  blur(b, &c);
  inpaint_rec(&c, it-1);
  fill(b, &c);
  free(c.dat);
}

static inline void
inpaint(buf_t *b)
{
  inpaint_rec(b, 10);
}
