#pragma once

static inline int // return selected entry, or -1
dt_radial_menu(
    struct nk_context *ctx,
    float  cx,     // center of wheel
    float  cy,
    float  radius, // radius of menu wheel
    float  mx,     // cursor position
    float  my,
    int    N,
    const char *text[])
{
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  float phi =  -M_PI, delta_phi = (2.0f*M_PI)/N,
        r0 = 0.3*radius, r1 = radius;
  float dx = mx-cx, dy = my-cy;
  const float mphi = (dx*dx+dy*dy>r0*r0) ? atan2f(my-cy, mx-cx) : -666;
  int ret = -1;
  for(int k=0;k<N;k++)
  {
#define NSEG 10 // number of vertices on linearly segmented arc
    float x[4*NSEG];
    float b = 0.05*radius;
    float da = phi + (NSEG/2)/(NSEG-1.0f)*delta_phi;
    float dx = b*cosf(da), dy = b*sinf(da);
    for(int s=0;s<NSEG;s++)
    {
      const float a = phi + s/(NSEG-1.0f)*delta_phi;
      const float sa = sinf(a), ca = cosf(a);
      x[2*s+0]          = cx + dx + ca * r1;
      x[2*s+1]          = cy + dy + sa * r1;
      x[4*NSEG-2-2*s+0] = cx + dx + ca * r0;
      x[4*NSEG-2-2*s+1] = cy + dy + sa * r0;
    }
    struct nk_color c = vkdt.style.colour[NK_COLOR_BUTTON];
    if(mphi > phi && mphi < phi+delta_phi)
    {
      ret = k;
      c = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER];
    }
    nk_fill_polygon(cmd, x, 2*NSEG, c);
    // approximate bounding box of wedge:
    // float aa[4] = {
    //   MIN(MIN(x[0], x[2*NSEG+0]), MIN(x[2*NSEG-2], x[4*NSEG-2])),
    //   MIN(MIN(x[1], x[2*NSEG+1]), MIN(x[2*NSEG-1], x[4*NSEG-1])),
    //   MAX(MAX(x[0], x[2*NSEG+0]), MAX(x[2*NSEG-2], x[4*NSEG-2])),
    //   MAX(MAX(x[1], x[2*NSEG+1]), MAX(x[2*NSEG-1], x[4*NSEG-1]))};
    // struct nk_rect bb = {aa[0], aa[1], aa[2]-aa[0], aa[3]-aa[1]};
    // box in two lists where height comes from center of outer wedge arc
    // struct nk_rect bb = {
    //   cx+(dx>0?1:-1)*radius/2 - (dx>0?0:radius/2),
    //   x[2*(NSEG/2)+1], radius/2, 30};
    // center of mass of wedge
    struct nk_rect bb = {
      (2*x[0] + x[2*NSEG+0] + 2*x[2*NSEG-2] + x[4*NSEG-2])/6.0f - radius/4,
      (2*x[1] + x[2*NSEG+1] + 2*x[2*NSEG-1] + x[4*NSEG-1])/6.0f - 15,
      radius/2, 30};
    // visualise the box:
    // nk_stroke_rect(cmd, bb, 0.0f, 2.0f, vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
    nk_draw_text(cmd, bb, text[k], strlen(text[k]), nk_glfw3_font(0),
            (struct nk_color){0,0,0,0xff},
            (struct nk_color){0xff,0xff,0xff,0xff});
    phi += delta_phi;
#undef NSEG
  }
  return ret;
}

static inline int // return non-zero if done and the state should be cleared
dt_radial_widget(
    struct nk_context *ctx,
    int    modid,
    int    parid,
    float *ax,   // gamepad axis x, or 0
    float *ay)
{
  const double throttle = 2.0; // min delay for same param in history, in seconds
  const int num = 0; // can't do multi-dimensional values now
  const dt_ui_param_t *param = vkdt.graph_dev.module[modid].so->param[parid];
  const dt_token_t widget = param->widget.type;
  if(widget != dt_token("slider")) return 1;
  float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
  float oldval = val[0];
  int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
  int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
  struct nk_rect bounds = {win_x, win_y, win_w, win_h};
  dt_graph_run_t flags = s_graph_run_none;
  int ret = 1;
  if(nk_input_is_mouse_click_in_rect(&vkdt.ctx.input, NK_BUTTON_DOUBLE, bounds))
  { // reset param
    memcpy(vkdt.graph_dev.module[modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
    goto changed;
    return 1;
  }
  // but ensure we block input if the radial widget is active
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  if(param->type == dt_token("float"))
  {
    ret = 0;
    const float min = param->widget.min;
    const float max = param->widget.max;
    float x = vkdt.state.center_x, w = vkdt.state.center_wd;
    float y = vkdt.state.center_y + 0.6*vkdt.state.center_ht, h = 0.05*vkdt.state.center_ht;
    struct nk_rect box = { x + (val[0]-min)/(max-min)*w, y, w*0.02f, h };
    nk_fill_rect(cmd, box, 0, vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
    // TODO write module/instance/param name to screen
    // TODO two triangles at min and max
    if(ax && ay)
    {
      if(fabsf(ax[0]) > fabsf(ay[0]))
        val[0] += copysignf(MAX(0, fabsf(ax[0])-0.04f), ax[0]) * (max-min) * 0.002f;
      else
        val[0] -= copysignf(MAX(0, fabsf(ay[0])-0.04f), ay[0]) * (max-min) * 0.01f;
      if(val[0] != oldval) goto changed;
    }
    else if(ctx->input.mouse.buttons[NK_BUTTON_LEFT].down)
    {
      const struct nk_vec2 pos = ctx->input.mouse.pos;
      val[0] = min + (max-min)*(pos.x - x)/w;
      goto changed;
    }
  }
  if(0)
  {
changed:
    if(vkdt.graph_dev.module[modid].so->check_params)
      flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
    vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | flags;
    vkdt.graph_dev.active_module = modid;
    dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
  }
  return ret;
}
