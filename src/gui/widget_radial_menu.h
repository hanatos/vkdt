#pragma once

static inline void
dt_radial_menu_text(
    struct nk_command_buffer *cmd,
    const char *text,
    struct nk_rect box,
    int selected,
    int left)
{
  static struct nk_user_font font;
  if(selected)
  {
    font = *nk_glfw3_font(0);
    font.height *= 1.5;
    box.h *= 1.5;
    if(left) box.x -= 3*box.w;
    box.w *= 4;
    nk_fill_rect(cmd, (struct nk_rect){box.x,box.y-box.h/2,box.w,box.h}, box.h/2,
        vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
  }
  const char *t = text;
  while(1)
  { // split at every white space
    int len = 0;
    for(;t[len]!=0&&t[len]!=' ';len++) {}
    if(len)
    {
      if(selected)
      {
        nk_fill_rect(cmd, box, 0.0f, vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
        nk_fill_rect(cmd, (struct nk_rect){box.x,box.y+box.h/2,box.w,box.h}, box.h/2,
            vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
        struct nk_rect bx = {box.x+font.height, box.y, box.w-2*font.height, box.h};
        nk_draw_text(cmd, bx, t, len, &font,
            (struct nk_color){0,0,0,0xff},
            vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT_HOVER]);
      }
      else
      {
        nk_draw_text(cmd, box, t, len, nk_glfw3_font(0),
            (struct nk_color){0,0,0,0xff},
            vkdt.style.colour[NK_COLOR_TEXT]);
      }
      box.y += box.h-1;
    }
    if(*t==0) break;
    t += len+1;
  }
}

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
  int ret = -1, left = 1;
  struct nk_rect selbox;
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
      if(dx > 0) left = 0;
      ret = k;
      c = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER];
    }
    nk_fill_polygon(cmd, x, 2*NSEG, c);
    // center of mass of wedge
    const float row_height = nk_glfw3_font(0)->height * 1.1f;
    float w = radius/4.0f;
    struct nk_rect bb = {
      (2*x[0] + x[2*NSEG+0] + 2*x[2*NSEG-2] + x[4*NSEG-2])/6.0f - w/2,
      (2*x[1] + x[2*NSEG+1] + 2*x[2*NSEG-1] + x[4*NSEG-1])/6.0f - row_height,
      w, row_height};
    if(ret == k) selbox = bb;
    else dt_radial_menu_text(cmd, text[k], bb, 0, 0);
    phi += delta_phi;
#undef NSEG
  }
  if(ret >= 0)
    dt_radial_menu_text(cmd, text[ret], selbox, 1, left);
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
  int ret = 1; // assume we're done
  if(nk_input_is_mouse_click_in_rect(&vkdt.ctx.input, NK_BUTTON_DOUBLE, bounds))
  { // reset param
    memcpy(vkdt.graph_dev.module[modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
    goto changed;
  }
  // but ensure we block input if the radial widget is active
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  if(param->type == dt_token("float"))
  {
    ret = 0; // keep open
    const float min = param->widget.min;
    const float max = param->widget.max;
    float x = vkdt.state.center_x, w = vkdt.state.center_wd;
    float y = vkdt.state.center_y + 0.6*vkdt.state.center_ht, h = 0.05*vkdt.state.center_ht;
    struct nk_rect box = { x + (val[0]-min)/(max-min)*w, y, w*0.02f, h };
    nk_fill_rect(cmd, box, MIN(box.h,box.w)/2.0f, vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
    // TODO two triangles at min and max
    static struct nk_user_font font;
    font = *nk_glfw3_font(0);
    font.height *= 2.0;
    char text[256];
    snprintf(text, sizeof(text), "%"PRItkn" %"PRItkn" %f",
        dt_token_str(vkdt.graph_dev.module[modid].name),
        dt_token_str(param->name),
        val[0]);
    nk_draw_text(cmd, (struct nk_rect){x+0.1*w, y, w, h}, text, strlen(text), &font,
        (struct nk_color){0,0,0,0xff},
        (struct nk_color){0xff,0xff,0xff,0xff});
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
    else if(ctx->input.mouse.buttons[NK_BUTTON_RIGHT].down)
    {
      ret = 1;
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
