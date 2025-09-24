#pragma once

// TODO when mouse released on wedge, activate widget
static inline int // return selected entry, or -1
dt_radial_menu(
    struct nk_context *ctx,
    float  cx,
    float  cy,
    float  radius,
    int    N,
    const char *text[])
{
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  float phi =  -M_PI, delta_phi = (2.0f*M_PI)/N,
        r0 = 0.3*radius, r1 = radius;
  const struct nk_vec2 pos = ctx->input.mouse.pos;
  float dx = pos.x-cx, dy = pos.y-cy;
  const float mphi = (dx*dx+dy*dy>r0*r0) ? atan2f(pos.y-cy, pos.x-cx) : -666;
  int ret = -1;
  for(int k=0;k<N;k++)
  {
    float x[] = {
      cx + cosf(phi+0.013) * r1,           cy + sinf(phi+0.013) * r1,
      cx + cosf(phi+delta_phi-0.013) * r1, cy + sinf(phi+delta_phi-0.013) * r1,
      cx + cosf(phi+delta_phi-0.013) * r0, cy + sinf(phi+delta_phi-0.013) * r0,
      cx + cosf(phi+0.013) * r0,           cy + sinf(phi+0.013) * r0};
    struct nk_color c = vkdt.style.colour[NK_COLOR_BUTTON];
    if(mphi > phi && mphi < phi+delta_phi)
    {
      ret = k;
      c = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER];
    }
    // TODO maybe want to fill a rounder wedge at least on the outside radial part?
    nk_fill_polygon(cmd, x, 4, c);
    float aa[4] = {
      MIN(MIN(x[0], x[2]), MIN(x[4], x[6])),
      MIN(MIN(x[1], x[3]), MIN(x[5], x[7])),
      MAX(MAX(x[0], x[2]), MAX(x[4], x[6])),
      MAX(MAX(x[1], x[3]), MAX(x[5], x[7]))};
    struct nk_rect bb = {aa[0], aa[1], aa[2]-aa[0], aa[3]-aa[1]};
    nk_draw_text(cmd, bb, text[k], strlen(text[k]), nk_glfw3_font(0),
            (struct nk_color){0,0,0,0xff},
            (struct nk_color){0xff,0xff,0xff,0xff});
    phi += delta_phi;
  }
  return ret;
}

// TODO widget
static inline void
dt_radial_slider(
    struct nk_context *ctx,
    int    modid,
    int    parid)
{
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  const dt_ui_param_t *param = vkdt.graph_dev.module[modid].so->param[parid];
  const dt_token_t widget = param->widget.type;
  if(widget != dt_token("slider")) return;
  if(param->type == dt_token("float"))
  {
    const int num = 0; // can't do multi-dimensional values now
    float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
    const float min = param->widget.min;
    const float max = param->widget.max;
    float x = vkdt.state.center_x, w = vkdt.state.center_wd;
    float y = vkdt.state.center_y + 0.6*vkdt.state.center_ht, h = 0.05*vkdt.state.center_ht;
    struct nk_rect box = { x + (val[0]-min)/(max-min)*w, y, w*0.02f, h };
    nk_fill_rect(cmd, box, 0, vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]);
    // TODO two triangles at min and max
    // TODO something follow mouse
  }
}
