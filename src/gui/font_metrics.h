#pragma once
#include <stdint.h>

typedef struct dt_font_cmap_entry_t
{
  uint32_t codepoint_beg; // first codepoint in range
  uint32_t codepoint_end; // codepoint just after range
  uint32_t glyph_idx_beg; // glyph index of start of range, the rest follows right after
}
dt_font_cmap_entry_t;

typedef struct dt_font_glyph_t
{ // geometry of one glyph
  float advance;
  float pbox_x, pbox_y, pbox_w, pbox_h;
  float tbox_x, tbox_y, tbox_w, tbox_h;
}
dt_font_glyph_t;

typedef struct dt_font_t
{
  int cmap_cnt, glyph_cnt;
  dt_font_cmap_entry_t *cmap;
  dt_font_glyph_t      *glyph;
  int default_glyph;
}
dt_font_t;

typedef struct dt_font_metrics_t
{
  uint32_t codepoint;
  float advance;
  float pbox_x, pbox_y, pbox_w, pbox_h;
  float tbox_x, tbox_y, tbox_w, tbox_h;
}
dt_font_metrics_t;

static inline void
dt_font_cleanup(dt_font_t *f)
{
  free(f->cmap);
  free(f->glyph);
  memset(f, 0, sizeof(*f));
}

static inline void
dt_font_init(dt_font_t *f)
{
  memset(f, 0, sizeof(*f));
}

static inline void
dt_font_read(dt_font_t *f, FILE *s)
{
  dt_lut_header_t header = {0};
  dt_font_metrics_t *data = 0;
  if(s)
  {
    if(fread(&header, sizeof(dt_lut_header_t), 1, s) != 1 ||
      header.version < dt_lut_header_version ||
      header.channels != 1 ||
      header.datatype != dt_lut_header_f32 ||
      header.ht * sizeof(float) != sizeof(dt_font_metrics_t))
    {
      header.wd = 0;
      fprintf(stderr, "[dt font] could not load font metrics lut!\n");
    }
    else
    {
      data = malloc(sizeof(dt_font_metrics_t)*header.wd);
      fread(data, sizeof(dt_font_metrics_t), header.wd, s);
    }
  }
  if(header.wd == 0)
  { // also need to init fake-font metrics:
hell:
    header.wd = 255;
    data = malloc(sizeof(dt_font_metrics_t)*255);
    for(int i=0;i<255;i++)
    {
      data[i] = (dt_font_metrics_t){
        .codepoint = i,
        .advance = 1.0,
        .tbox_x = 0, .tbox_y = 0, .tbox_w = 1, .tbox_h = 1,
        .pbox_x = 0, .pbox_y = 0, .pbox_w = 1, .pbox_h = 1,
      };
    }
  }
  f->glyph_cnt = header.wd;
  f->glyph = malloc(sizeof(dt_font_glyph_t)*f->glyph_cnt);
  // now go through all entries and find consecutive ranges of codepoints
  f->cmap_cnt = 0;
  dt_font_cmap_entry_t e;
  e.codepoint_beg = data[0].codepoint;
  e.glyph_idx_beg = 0;
  for(int i=1;i<=header.wd;i++)
    if(i==header.wd || data[i].codepoint > data[i-1].codepoint+1) f->cmap_cnt++;
  f->cmap = malloc(sizeof(dt_font_cmap_entry_t)*f->cmap_cnt);
  f->cmap_cnt = 0;
  int g = 0;
  for(int i=1;i<=header.wd;i++)
  {
    uint32_t cur = i==header.wd ? 0xffff : data[i].codepoint;
    uint32_t end = data[i-1].codepoint+1;
    if(cur < end) goto hell;
    if(cur > end)
    {
      e.codepoint_end = end;
      f->cmap[f->cmap_cnt++] = e;
      e.glyph_idx_beg += e.codepoint_end - e.codepoint_beg;
      e.codepoint_beg = cur;
    }
    memcpy(&f->glyph[g++], &data[i-1].advance, sizeof(dt_font_glyph_t));
  }
}

static inline int
dt_font_glyph(dt_font_t *f, uint32_t codepoint)
{
  for(int i=0;i<f->cmap_cnt;i++)
  {
    if(codepoint < f->cmap[i].codepoint_beg) return f->default_glyph;
    if(codepoint < f->cmap[i].codepoint_end)
      return f->cmap[i].glyph_idx_beg + codepoint - f->cmap[i].codepoint_beg;
  }
  return f->default_glyph;
}
