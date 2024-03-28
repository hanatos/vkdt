#pragma once
#include "nk.h"

void dt_draw_star(float u, float v, float size, struct nk_color col)
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
    const struct nk_color bg_col,
    const struct nk_color tint_col,
    uint16_t rating,
    uint16_t labels,
    const char *text,
    int set_nav_focus)
{
  int ret = 0;
  struct nk_image img = nk_image_ptr(dset);
  int wd = MAX(size.x, size.y);

  struct nk_rect full = nk_widget_bounds(ctx);
  // if(nk_widget_is_mouse_clicked(ctx, NK_BUTTON_LEFT)) ret = 1;
  if(nk_input_mouse_clicked(&ctx->input, NK_BUTTON_LEFT, full)) ret = 1;
  struct nk_rect bound = full;
  //if(nk_button_color(ctx, (struct nk_color){0x77,0x77,0x77,0xff})) ret = 1;
  // if(nk_button_label(ctx, "#")) ret = 1;
  nk_label_colored(ctx, "", 0, bg_col);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
  bound.x += (wd-size.x)/2;
  bound.w -= (wd-size.x);
  bound.y += (wd-size.y)/2;
  bound.h -= (wd-size.y);
  nk_draw_image(canvas, bound, &img, (struct nk_color){0x77,0x77,0x77,0xff});

  // render decorations (colour labels/stars/etc?)
  dt_draw_rating(full.x+0.1*wd, full.y+0.10*wd, 0.1*wd, rating);
  dt_draw_labels(full.x+0.1*wd, full.y+0.95*wd, 0.1*wd, labels);

  if(text) // optionally render text
    nk_draw_text(canvas, full, text, strlen(text), &dt_gui_get_font(0)->handle, (struct nk_color){0x77,0x77,0x77,0xff}, (struct nk_color){0x11,0x11,0x11,0xff});

  return ret;


#if 0
  // make sure we're square:
  float wd = MAX(size[0], size[1]) + 2*padding;
  // overloaded operators are not exposed in imgui_internal, so we'll do the hard way:
  const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos[0] + wd, window->DC.CursorPos[1] + wd));
  ImVec2 off((wd - size[0])*.5f, (wd - size[1])*.5f);
  const ImRect image_bb(
      ImVec2(window->DC.CursorPos[0] + off[0],
        window->DC.CursorPos[1] + off[1]),
      ImVec2(window->DC.CursorPos[0] + off[0] + size[0],
        window->DC.CursorPos[1] + off[1] + size[1]));
  ItemSize(bb);
  if (!ItemAdd(bb, id))
    return 0;

  if(set_nav_focus)
    SetFocusID(id, window);

  // stars clicked?
  bool hovered, held;
  bool pressed = ButtonBehavior(bb, id, &hovered, &held);

  // Render
  const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
  RenderNavHighlight(bb, id);
  RenderFrame(bb.Min, bb.Max, col, true, ImClamp(padding, 0.0f, style.FrameRounding));
  if (bg_col.w > 0.0f)
    window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));
  window->DrawList->AddImage(user_texture_id, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

  // render decorations (colour labels/stars/etc?)
  dt_draw_rating(bb.Min[0]+0.1*wd, bb.Min[1]+0.1*wd, 0.1*wd, rating);
  dt_draw_labels(bb.Min[0]+0.1*wd, bb.Min[1]+0.9*wd, 0.1*wd, labels);

  if(text) // optionally render text
    window->DrawList->AddText(0, 0.0f, ImVec2(bb.Min[0]+padding, bb.Min[1]+padding), 0xff111111u, text, text+strlen(text), wd, 0);

  // TODO: return stars or labels if they have been clicked
  return pressed ? 1 : 0;
#endif
}
