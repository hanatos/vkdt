#pragma once

static inline void
dt_draw_quad(struct nk_command_buffer *buf, float u, float v, float size, struct nk_color col)
{
  float c[] = {
     0,  1,  1,  0,
     0, -1, -1,  0};
  float x[20];
  for(int i=0;i<4;i++)
  {
    x[2*i+0] = u + size * c[2*i+0];
    x[2*i+1] = v + size * c[2*i+1];
  }
  nk_fill_polygon  (buf, x, 4, col);
  nk_stroke_polygon(buf, x, 4, .1*size, nk_rgb(0,0,0));
}

static inline int
screen_to_frame(float sx, dt_graph_t *g, struct nk_rect bb)
{
  return CLAMP((int)((sx - bb.x)/bb.w * g->frame_cnt + 0.5), 0, g->frame_cnt-1);
}

static inline float
frame_to_screen(int f, dt_graph_t *g, struct nk_rect bb)
{
  return CLAMP(bb.x + bb.w * f / (float)g->frame_cnt, bb.x, bb.x+bb.w);
}

static inline float
dt_draw_param_line(
    dt_module_t *mod,
    int          p)
{
  if(mod->param_keyframe[p] == 0xffff) return 0.0f; // module has some keyframes, but our parameter?
  struct nk_context *ctx = &vkdt.ctx;
  static uint32_t drag_k = -1u, drag_mod = -1u;
  struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
  const float ht = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  nk_layout_row_dynamic(ctx, ht, 1);
  struct nk_rect bounds = nk_widget_bounds(ctx);
  bounds.x += 0.11 * bounds.w; // move in a bit so we can read the labels
  bounds.y += 0.05 * bounds.h;
  bounds.h -= 0.1  * bounds.h;
  nk_fill_rect(buf, bounds, 0, vkdt.style.colour[NK_COLOR_BUTTON]);

  int modified = 0;
  char text[60];
  snprintf(text, sizeof(text), "%"PRItkn" %"PRItkn" %"PRItkn,
      dt_token_str(mod->name), dt_token_str(mod->inst), dt_token_str(mod->so->param[p]->name));
  nk_label(ctx, text, NK_TEXT_LEFT);
  dt_keyframe_t *key = mod->keyframe + mod->param_keyframe[p];
  do
  {
    int k = key - mod->keyframe;
    float x = frame_to_screen(key->frame, mod->graph, bounds);
    float y = bounds.y+bounds.h/2.0;
    dt_draw_quad(buf, x, y, bounds.h/2.5, vkdt.style.colour[NK_COLOR_DT_ACCENT]);
    struct nk_rect bbk = {.x = x-ht/2.0, .y = y-ht/2.0, .w = ht, .h = ht};
    if(nk_input_is_mouse_hovering_rect(&ctx->input, bbk))
    {
      nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 0));
      nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(0, 0));
      if(nk_tooltip_begin(ctx, vkdt.state.panel_wd))
      {
        nk_layout_row_static(ctx, vkdt.ctx.style.font->height, vkdt.state.panel_wd, 1);
        // TODO: echo more dimensions of the same parameter?
        nk_labelf(ctx, NK_TEXT_LEFT, "%"PRItkn " %f", dt_token_str(mod->so->param[p]->name), *((float*)mod->keyframe[k].data));
        nk_label(ctx, "right click to delete", NK_TEXT_LEFT);
        nk_label(ctx, "left click and drag to move", NK_TEXT_LEFT);
        nk_tooltip_end(ctx);
      }
      nk_style_pop_vec2(ctx);
      nk_style_pop_vec2(ctx);
    }
    if(nk_input_has_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, bbk, nk_true) && 
       drag_k == -1u)
    { // set state: dragging keyframe k
      drag_k = k;
      drag_mod = mod-mod->graph->module;
    }
    if(nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT) && 
       drag_k == k && drag_mod == mod-mod->graph->module)
    { // keep dragging along
      mod->keyframe[k].frame = screen_to_frame(ctx->input.mouse.pos.x, mod->graph, bounds);
      modified = 1;
    }
    if(nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT) && 
       drag_k == k && drag_mod == mod-mod->graph->module)
    { // drag finished
      mod->keyframe[k].frame = screen_to_frame(ctx->input.mouse.pos.x, mod->graph, bounds);
      drag_k = drag_mod = -1u;
      modified = 1;
    }
    if (nk_input_has_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, bbk))
    { // right click: delete this keyframe by copying the last one over it and decreasing keyframe_cnt
      mod->keyframe[k--] = mod->keyframe[--mod->keyframe_cnt];
      modified = 1;
      break; // oops we broke the pointers
    }
    key = key->next;
  }
  while(key);

  // fix keyframe list if we changed anything
  if(modified) dt_module_keyframe_post_update(mod);

  if(!modified && nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, bounds)) // left click: move animation time
    dt_gui_dr_anim_seek(screen_to_frame(ctx->input.mouse.pos.x, mod->graph, bounds));

  float x[4] = {
    frame_to_screen(mod->graph->frame, mod->graph, bounds), bounds.y,
    frame_to_screen(mod->graph->frame, mod->graph, bounds), bounds.y + bounds.h };
  nk_stroke_polyline(buf, x, 2, .1*ht, nk_rgb(0,0,0));
  return ht;
}

static inline float
dt_dopesheet_module(dt_graph_t *g, uint32_t modid)
{ // draw all parameters of a module
  dt_module_t *mod = g->module + modid;
  if(mod->keyframe_cnt == 0) return 0.0f; // no keyframes to draw
  float size = 0.0f;
  for(int p=0;p<mod->so->num_params;p++)
    size += dt_draw_param_line(mod, p);
  return size;
}

static inline void
dt_dopesheet()
{ // draw all modules connected on the graph in same order as right panel in darkroom mode
  dt_graph_t *graph = &vkdt.graph_dev;
  struct nk_context *ctx = &vkdt.ctx;

  nk_style_push_font(ctx, &dt_gui_get_font(3)->handle);
#define TOOLTIP(STR) do {\
  nk_style_pop_font(ctx);\
  dt_tooltip(STR);\
  nk_style_push_font(ctx, &dt_gui_get_font(3)->handle);\
} while(0)

  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  nk_layout_row_dynamic(ctx, row_height, 7);

  TOOLTIP("play/pause the animation");
  if(vkdt.state.anim_playing)
  {
    if(nk_button_label(ctx, "\ue047")) // or \ue034 for pause icon
      dt_gui_dr_anim_stop();
  }
  else if(nk_button_label(ctx, "\ue037"))
    dt_gui_dr_anim_start();
  TOOLTIP("seek to previous keyframe");
  if(nk_button_label(ctx, "\ue020")) dt_gui_dr_anim_seek_keyframe_bck();
  TOOLTIP("seek to next keyframe");
  if(nk_button_label(ctx, "\ue01f")) dt_gui_dr_anim_seek_keyframe_fwd();
  TOOLTIP("back to previous frame");
  if(nk_button_label(ctx, "\ue045")) dt_gui_dr_anim_step_bck();
  TOOLTIP("advance to next frame");
  if(nk_button_label(ctx, "\ue044")) dt_gui_dr_anim_step_fwd();
  TOOLTIP("rewind to start");
  if(nk_button_label(ctx, "\ue042")) dt_gui_dr_prev();
  nk_style_pop_font(ctx);\

  nk_labelf(ctx, NK_TEXT_LEFT, "frame %d/%d", vkdt.graph_dev.frame, vkdt.graph_dev.frame_cnt);
#undef TOOLTIP

  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  float size = 0.0;
  for(int m=cnt-1;m>=0;m--)
    size += dt_dopesheet_module(graph, modid[m]);

  if(size == 0.0f)
  { // no keyframes yet, want to display something
    struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
    nk_layout_row_dynamic(ctx, row_height, 1);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    bounds.x += 0.11 * bounds.w; // move in a bit so we can read the labels
    bounds.y += 0.05 * bounds.h;
    bounds.h -= 0.1  * bounds.h;
    nk_fill_rect(buf, bounds, 0, vkdt.style.colour[NK_COLOR_BUTTON]);
    if(nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, bounds)) // left click: move animation time
      dt_gui_dr_anim_seek(screen_to_frame(ctx->input.mouse.pos.x, graph, bounds));

    nk_label(ctx, "timeline", NK_TEXT_LEFT);
    float x[4] = {
      frame_to_screen(graph->frame, graph, bounds), bounds.y,
      frame_to_screen(graph->frame, graph, bounds), bounds.y+bounds.h };
    nk_stroke_polyline(buf, x, 2, .1*row_height, nk_rgb(0,0,0));
  }

  // this is unfortunately not reliable since we skip drawing and everything
  // in case a line cannot be seen on screen.
  // vkdt.wstate.dopesheet_view = MIN(0.5*vkdt.state.center_ht, size);
  vkdt.wstate.dopesheet_view = 0.2*vkdt.state.center_ht; // fixed size
}
