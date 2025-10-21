#pragma once

static inline int
dt_resize_panel(int dragging)
{
  if(vkdt.wstate.fullscreen_view) return dragging;
  float w = vkdt.ctx.style.font->height;
  float pwd = vkdt.state.panel_wd;
  int win_y = vkdt.state.center_y;
  int win_h = vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
  struct nk_rect box = {vkdt.win.width - pwd - w/2, win_y + 0.95*win_h-w/2, w, w};
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  int hover = nk_input_is_mouse_hovering_rect(&vkdt.ctx.input, box);
  if(!dragging && !dt_gui_input_blocked() && hover &&
      vkdt.ctx.input.mouse.buttons[NK_BUTTON_LEFT].down)
    dragging = 1;
  if(!vkdt.ctx.input.mouse.buttons[NK_BUTTON_LEFT].down) dragging = 0;
  if(dragging)
    pwd = vkdt.win.width - vkdt.ctx.input.mouse.pos.x;
  struct nk_color col = hover ? vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER] : vkdt.style.colour[NK_COLOR_BUTTON];
  nk_fill_circle(cmd, box, col);
  float f = CLAMP(pwd / vkdt.win.width, 0.01, 1.0);
  if(dragging)
  {
    dt_rc_set_float(&vkdt.rc, "gui/panelwd", f);
    dt_gui_style_to_state();
  }
  return dragging;
}
