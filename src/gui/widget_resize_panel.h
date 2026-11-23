#pragma once

static inline int
dt_resize_panel(int dragging)
{
  if(vkdt.wstate.fullscreen_view) return dragging; // no panels

  float pr = dt_rc_get_float(&vkdt.rc, "gui/panelr", 0.2f);
  float pl = dt_rc_get_float(&vkdt.rc, "gui/panell", 0.2f);
  float pb = dt_rc_get_float(&vkdt.rc, "gui/panelb", 0.2f);
  const int have_pl = !vkdt.wstate.fullscreen_view && vkdt.view_mode == s_view_darkroom && vkdt.wstate.history_view;
  const int have_pr = !vkdt.wstate.fullscreen_view;
  const int have_pb = !vkdt.wstate.fullscreen_view && vkdt.view_mode == s_view_darkroom && vkdt.wstate.dopesheet_view;
  float w = vkdt.ctx.style.font->height;
  int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
  int win_w = vkdt.state.center_wd, win_h = vkdt.win.height;
  struct nk_rect box[] = {
    {vkdt.win.width * pl          - w/2, win_y + 0.8*win_h-w/2, w, w},  // left
    {vkdt.win.width * (1.0f - pr) - w/2, win_y + 0.8*win_h-w/2, w, w},  // right
    {win_x + 0.95*win_w           - w/2, (1.0f - pb) * vkdt.win.height - w/2, w, w}}; // bottom
  struct nk_command_buffer *cmd = &vkdt.global_cmd;

  for(int i=0;i<3;i++)
  { // for left, right and bottom panels
    if(i == 0) if(!have_pl) continue;
    if(i == 1) if(!have_pr) continue;
    if(i == 2) if(!have_pb) continue;
    int hover = nk_input_is_mouse_hovering_rect(&vkdt.ctx.input, box[i]);
    struct nk_color col = hover ? vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER] : vkdt.style.colour[NK_COLOR_BUTTON];
    nk_push_scissor(cmd, nk_null_rect);
    nk_fill_circle(cmd, box[i], col);
    if(!dragging && !dt_gui_input_blocked() && hover &&
        vkdt.ctx.input.mouse.buttons[NK_BUTTON_LEFT].down)
      dragging = i+1; // dragging this handle
    if(!vkdt.ctx.input.mouse.buttons[NK_BUTTON_LEFT].down) dragging = 0;
    if(dragging == i+1)
    {
      if(i == 0) pl = vkdt.ctx.input.mouse.pos.x;
      if(i == 1) pr = vkdt.win.width  - vkdt.ctx.input.mouse.pos.x;
      if(i == 2) pb = vkdt.win.height - vkdt.ctx.input.mouse.pos.y;

      if(i == 0) dt_rc_set_float(&vkdt.rc, "gui/panell", CLAMP(pl / vkdt.win.width,  0.01, 1.0));
      if(i == 1) dt_rc_set_float(&vkdt.rc, "gui/panelr", CLAMP(pr / vkdt.win.width,  0.01, 1.0));
      if(i == 2) dt_rc_set_float(&vkdt.rc, "gui/panelb", CLAMP(pb / vkdt.win.height, 0.01, 1.0));
      dt_gui_style_to_state();
    }
  }
  return dragging;
}
