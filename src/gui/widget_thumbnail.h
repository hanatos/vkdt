#pragma once
#include "nk.h"

static inline void dt_draw_star(float u, float v, float size, struct nk_color col)
{
  // material icon code point for star:
  struct nk_command_buffer *buf = nk_window_get_canvas(&vkdt.ctx);
  const char* stars = "\ue838\ue838\ue838\ue838\ue838";
  const struct nk_rect bounds = {u-0.5*size, v-0.5*size, u+0.5*size, v+0.5*size};
  nk_draw_text(buf, bounds, stars, 2, &dt_gui_get_font(3)->handle, (struct nk_color){0x77,0x77,0x77,0xff}, col);
}

static inline void dt_draw_rating(float x, float y, float wd, uint16_t rating)
{
  if(!rating) return;
  const struct nk_color col = { 0, 0, 0, 0xff };
  const char* stars = "\ue838\ue838\ue838\ue838\ue838";
  const struct nk_rect bounds = {x-0.5*wd, y-0.5*wd, rating*wd, wd};
  struct nk_command_buffer *buf = nk_window_get_canvas(&vkdt.ctx);
  nk_draw_text(buf, bounds, stars, 3*rating, &dt_gui_get_font(3)->handle, (struct nk_color){0x77,0x77,0x77,0xff}, col);
}
static inline void dt_draw_labels(float x, float y, float wd, uint16_t labels)
{
  const struct nk_color label_col[] = {
    {0xcc,0x33,0x33,0xff}, // red
    {0x33,0xcc,0x33,0xff}, // green
    {0x33,0x33,0xcc,0xff}, // blue
    {0xcc,0xcc,0x33,0xff}, // yellow
    {0xcc,0x33,0xcc,0xff}, // magenta
    {0x33,0x33,0x33,0xff}, // black
    {0x33,0x33,0x33,0xff}, // black
  };
  struct nk_command_buffer *buf = nk_window_get_canvas(&vkdt.ctx);
  int j=0;
  for(int i=0;i<5;i++) // regular round colour labels
    if(labels & (1<<i))
      nk_fill_circle(buf, (struct nk_rect){x+wd*(j++)-0.4*wd, y-0.4*wd, 0.8*wd, 0.8*wd}, label_col[i]);
  if(labels & s_image_label_video)
  { // movie indicator
    float tc[] = {x + wd*j - 0.2f*wd, y-0.4f*wd, x + wd*j - 0.2f*wd, y+0.4f*wd, x+wd*j+0.4f*wd, y };
    nk_fill_polygon(buf, tc, 3, label_col[5]);
    nk_stroke_polygon(buf, tc, 3, .05*wd, (struct nk_color){0xff,0xff,0xff,0xff});
    j++;
  }
  if(labels & s_image_label_bracket)
  { // bracketed shot indicator
    float tc[] = {
      x+j*wd-0.2f*wd,y-0.4f*wd, x+j*wd-0.4f*wd,y-0.4f*wd, x+j*wd-0.4f*wd,y+0.4f*wd, x+j*wd-0.2f*wd, y+0.4f*wd,
      x+j*wd+0.2f*wd,y-0.4f*wd, x+j*wd+0.4f*wd,y-0.4f*wd, x+j*wd+0.4f*wd,y+0.4f*wd, x+j*wd+0.2f*wd, y+0.4f*wd };
    nk_stroke_polyline(buf, tc,   4, .1*wd, label_col[6]);
    nk_stroke_polyline(buf, tc+8, 4, .1*wd, label_col[6]);
  }
}

static inline uint32_t
dt_thumbnail_image(
    struct nk_context *ctx,
    VkDescriptorSet dset,
    const struct nk_vec2 size,
    const struct nk_color hov_col,
    const struct nk_color reg_col,
    uint16_t rating,
    uint16_t labels,
    const char *text)
{
  int ret = 0;
  struct nk_image img = nk_image_ptr(dset);
  int wd = MAX(size.x, size.y);

  struct nk_rect full = nk_widget_bounds(ctx);
  nk_bool hovered = nk_input_is_mouse_hovering_rect(&ctx->input, full);
  if(nk_input_mouse_clicked(&ctx->input, NK_BUTTON_LEFT, full)) ret = 1;
  struct nk_rect bound = full;
  nk_label(ctx, "", 0);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
  nk_fill_rect(canvas, full, 0, hovered ? hov_col : reg_col);
  bound.x += (wd-size.x)/2;
  bound.w -= (wd-size.x);
  bound.y += (wd-size.y)/2;
  bound.h -= (wd-size.y);
  nk_draw_image(canvas, bound, &img, (struct nk_color){0xff,0xff,0xff,0xff});

  // render decorations (colour labels/stars/etc?)
  dt_draw_rating(full.x+0.1*wd, full.y+0.10*wd, 0.1*wd, rating);
  dt_draw_labels(full.x+0.1*wd, full.y+0.95*wd, 0.1*wd, labels);

  if(text) // optionally render text
    nk_draw_text(canvas, full, text, strlen(text), &dt_gui_get_font(0)->handle, (struct nk_color){0x77,0x77,0x77,0xff}, (struct nk_color){0x11,0x11,0x11,0xff});

  return ret;
}
