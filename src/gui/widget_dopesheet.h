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
    int          p,
    uint32_t     hotkey)
{
  if(mod->param_keyframe[p] == 0xffff) return 0.0f; // module has some keyframes, but our parameter?
  struct nk_context *ctx = &vkdt.ctx;
  static uint32_t drag_k = -1u, drag_mod = -1u;
  struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
  const float ht = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  nk_layout_row_dynamic(ctx, ht, 1);
  struct nk_rect bounds = nk_widget_bounds(ctx);
  bounds.x += 0.11 * bounds.w; // move in a bit so we can read the labels
  bounds.w -= 0.11 * bounds.w;
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
      if(hotkey == 1)
      {
        dt_graph_t *g = mod->graph;
        int modid = mod - g->module;
        uint32_t ki = dt_gui_keyframe_add(modid, p);
        if(ki != -1u) 
          g->module[modid].keyframe[ki].frame = CLAMP(g->frame + 2, 0, g->frame_cnt-1);
      }
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
    if(nk_contextual_begin(ctx, 0, nk_vec2(vkdt.state.panel_wd, vkdt.state.panel_wd), bbk))
    {
      const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
      nk_layout_row_dynamic(ctx, row_height, 1);
      if(nk_contextual_item_label(ctx, "delete keyframe", NK_TEXT_LEFT))
      { // delete this keyframe by copying the last one over it and decreasing keyframe_cnt
        mod->keyframe[k--] = mod->keyframe[--mod->keyframe_cnt];
        modified = 1;
        nk_contextual_end(ctx);
        break; // oops we broke the pointers
      }
      nk_layout_row_dynamic(ctx, row_height, 5);
      int set = 0;
#define ANIM_ITEM(mode, str) do {\
      set = mod->keyframe[k].anim == s_anim_ ## mode; \
      if(set) nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.contextual_button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));\
      if(set) nk_style_push_color(&vkdt.ctx, &vkdt.ctx.style.contextual_button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);\
      if(nk_contextual_item_label(ctx, str, NK_TEXT_LEFT))\
        mod->keyframe[k].anim = s_anim_ ## mode ;\
      if(set) nk_style_pop_style_item(&vkdt.ctx);\
      if(set) nk_style_pop_color(&vkdt.ctx);\
      } while(0)

      ANIM_ITEM(step,"step");
      ANIM_ITEM(lerp,"lerp");
      ANIM_ITEM(ease_in, "ease in");
      ANIM_ITEM(ease_out, "ease out");
      ANIM_ITEM(smooth, "smooth");
#undef ANIM_ITEM

      nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 0));
      nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(0, 0));
      nk_layout_row_static(ctx, vkdt.ctx.style.font->height, vkdt.state.panel_wd, 1);
      // TODO: echo more dimensions of the same parameter?
      nk_labelf(ctx, NK_TEXT_LEFT, "%"PRItkn " %f", dt_token_str(mod->so->param[p]->name), *((float*)mod->keyframe[k].data));
      nk_label(ctx, "left click and drag to move", NK_TEXT_LEFT);
      nk_label(ctx, "see hotkeys to duplicate (default shift-d)", NK_TEXT_LEFT);
      nk_style_pop_vec2(ctx);
      nk_style_pop_vec2(ctx);
      nk_contextual_end(ctx);
    }
    key = key->next;
  }
  while(key);

  // fix keyframe list if we changed anything
  if(modified) dt_module_keyframe_post_update(mod);

  // don't change anim time if popup is still open
  struct nk_window *popup = ctx->current->popup.win;
  if(popup && ctx->current->popup.type == NK_PANEL_CONTEXTUAL)
    modified = 1;

  if(!modified && nk_input_is_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, bounds, nk_true)) // left click: move animation time
    dt_gui_dr_anim_seek(screen_to_frame(ctx->input.mouse.pos.x, mod->graph, bounds));

  float x[4] = {
    frame_to_screen(mod->graph->frame, mod->graph, bounds), bounds.y,
    frame_to_screen(mod->graph->frame, mod->graph, bounds), bounds.y + bounds.h };
  nk_stroke_polyline(buf, x, 2, .1*ht, nk_rgb(0,0,0));
  return ht;
}

static inline float
dt_dopesheet_module(dt_graph_t *g, uint32_t modid, uint32_t hotkey)
{ // draw all parameters of a module
  dt_module_t *mod = g->module + modid;
  if(mod->keyframe_cnt == 0) return 0.0f; // no keyframes to draw
  float size = 0.0f;
  for(int p=0;p<mod->so->num_params;p++)
    size += dt_draw_param_line(mod, p, hotkey);
  return size;
}

static inline void
dt_dopesheet(
    const char *filter_name,
    const char *filter_inst,
    uint32_t hotkey)
{ // draw all modules connected on the graph in same order as right panel in darkroom mode
  dt_graph_t *graph = &vkdt.graph_dev;
  struct nk_context *ctx = &vkdt.ctx;

  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  nk_layout_row_dynamic(ctx, row_height, 7);

  dt_tooltip("play/pause the animation");
  if(vkdt.state.anim_playing)
  {
    if(nk_button_label(ctx, "\ue047")) // or \ue034 for pause icon
      dt_gui_dr_anim_stop();
  }
  else if(nk_button_label(ctx, "\ue037"))
    dt_gui_dr_anim_start();
  dt_tooltip("seek to previous keyframe");
  if(nk_button_label(ctx, "\ue020")) dt_gui_dr_anim_seek_keyframe_bck();
  dt_tooltip("seek to next keyframe");
  if(nk_button_label(ctx, "\ue01f")) dt_gui_dr_anim_seek_keyframe_fwd();
  dt_tooltip("back to previous frame");
  if(nk_button_label(ctx, "\ue045")) dt_gui_dr_anim_step_bck();
  dt_tooltip("advance to next frame");
  if(nk_button_label(ctx, "\ue044")) dt_gui_dr_anim_step_fwd();
  dt_tooltip("rewind to start");
  if(nk_button_label(ctx, "\ue042")) dt_gui_dr_anim_seek(0);

  nk_labelf(ctx, NK_TEXT_LEFT, "frame %d/%d", vkdt.graph_dev.frame, vkdt.graph_dev.frame_cnt);

  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  float size = 0.0;
  for(int m=cnt-1;m>=0;m--)
  {
    if(filter_name && filter_name[0])
    {
      char name[10] = {0};
      memcpy(name, dt_token_str(vkdt.graph_dev.module[modid[m]].name), 8);
      if(!strstr(name, filter_name)) continue;
    }
    if(filter_inst && filter_inst[0])
    {
      char inst[10] = {0};
      memcpy(inst, dt_token_str(vkdt.graph_dev.module[modid[m]].inst), 8);
      if(!strstr(inst, filter_inst)) continue;
    }
    size += dt_dopesheet_module(graph, modid[m], hotkey);
  }

  if(size == 0.0f)
  { // no keyframes yet, want to display something
    struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
    nk_layout_row_dynamic(ctx, row_height, 1);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    bounds.x += 0.11 * bounds.w; // move in a bit so we can read the labels
    bounds.w -= 0.11 * bounds.w;
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
