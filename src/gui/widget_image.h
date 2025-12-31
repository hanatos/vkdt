#pragma once
#include "widget_image_util.h"
#include "api.h"
#include "nk.h"
// widget for window-size image with zoom/pan interaction

static inline void
dt_image_events(struct nk_context *ctx, dt_image_widget_t *w, int hovered, int main, struct nk_rect bounds)
{
  const struct nk_color nk_white = {255,255,255,255};
  struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
  int do_events = 1;
  if(main && vkdt.wstate.active_widget_modid >= 0)
  { // on-canvas widget interaction
    do_events = 0; // by default disable std events (zoom/pan) if there is overlay
    struct nk_vec2 pos  = ctx->input.mouse.pos;
    struct nk_vec2 cpos = ctx->input.mouse.buttons[NK_BUTTON_LEFT].clicked_pos;
    const dt_token_t widget = vkdt.graph_dev.module[
        vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(widget == dt_token("pers"))
    { // distinguish by type:
      float *v = vkdt.wstate.state;
      float p[8];
      for(int k=0;k<4;k++)
        dt_image_to_view(&vkdt.wstate.img_widget, v+2*k, p+2*k);
      nk_stroke_polygon(buf, p, 4, 1.0, nk_white);
      int corner_hovered = -1;
      if(nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT))
        vkdt.wstate.selected = -1;
      if(hovered)
      { // find active corner if close enough
        float m[] = {pos.x, pos.y};
        float max_dist = FLT_MAX;
        const float px_dist = 0.1*vkdt.win.height;
        for(int cc=0;cc<4;cc++)
        {
          float n[] = {vkdt.wstate.state[2*cc+0], vkdt.wstate.state[2*cc+1]}, v[2];
          dt_image_to_view(&vkdt.wstate.img_widget, n, v);
          float dist2 =
            (v[0]-m[0])*(v[0]-m[0])+
            (v[1]-m[1])*(v[1]-m[1]);
          if(dist2 < px_dist*px_dist)
          {
            if(dist2 < max_dist)
            {
              max_dist = dist2;
              corner_hovered = cc;
              if(nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT))
                vkdt.wstate.selected = cc;
            }
          }
        }
      }
      if(corner_hovered >= 0)
      {
        float q[2] = {
          p[2*corner_hovered],
          p[2*corner_hovered+1]};
        float rad = 0.02 * vkdt.state.center_wd;
        nk_fill_circle(buf, (struct nk_rect){q[0]-rad, q[1]-rad, 2*rad, 2*rad}, (struct nk_color){0x77,0x77,0x77,0x77});
        if(hovered && nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT))
        {
          float v[] = {pos.x, pos.y}, n[2] = {0};
          dt_image_from_view(&vkdt.wstate.img_widget, v, n);
          dt_gui_dr_pers_adjust(n, 0);
        }
      }
    }
    else if(widget == dt_token("straight"))
    {
      if(vkdt.wstate.selected >= 0)
      {
        float v[4] = { vkdt.wstate.state[1], vkdt.wstate.state[2], vkdt.wstate.state[3], vkdt.wstate.state[4] };
        nk_stroke_polyline(buf, v, 2, 1.0, nk_white);
        if(vkdt.wstate.selected > 0)
        {
          vkdt.wstate.state[3] = pos.x;
          vkdt.wstate.state[4] = pos.y;
        }
      }
      if(hovered && nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT))
      {
        vkdt.wstate.selected = 0;
        float dx = pos.x - vkdt.wstate.state[1];
        float dy = pos.y - vkdt.wstate.state[2];
        float a = atan2f(dy, dx);
        if(fabsf(dx) > fabsf(dy)) a =        + a * 180.0f/M_PI;
        else                      a = 270.0f + a * 180.0f/M_PI;
        if(fabsf(a + 180.0f) < fabsf(a)) a += 180.0f;
        if(fabsf(a - 180.0f) < fabsf(a)) a -= 180.0f;
        if(fabsf(a + 180.0f) < fabsf(a)) a += 180.0f;
        if(fabsf(a - 180.0f) < fabsf(a)) a -= 180.0f;
        vkdt.wstate.state[0] += a;
      }
      if(hovered && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT))
      {
        vkdt.wstate.state[1] = vkdt.wstate.state[3] = pos.x;
        vkdt.wstate.state[2] = vkdt.wstate.state[4] = pos.y;
        vkdt.wstate.selected = 1;
      }
    }
    else if(widget == dt_token("crop"))
    {
      float v[8] = {
        vkdt.wstate.state[0], vkdt.wstate.state[2], vkdt.wstate.state[1], vkdt.wstate.state[2], 
        vkdt.wstate.state[1], vkdt.wstate.state[3], vkdt.wstate.state[0], vkdt.wstate.state[3]
      };
      float p[8];
      for(int k=0;k<4;k++) dt_image_to_view(&vkdt.wstate.img_widget, v+2*k, p+2*k);
      nk_stroke_polygon(buf, p, 4, 1.0, nk_white);
      if(!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT))
        vkdt.wstate.selected = -1;
      int edge_hovered = -1;
      if(hovered)
      {
        float m[2] = {(float)pos.x, (float)pos.y};
        float max_dist = FLT_MAX;
        const float px_dist = 0.1*vkdt.win.height;
        for(int ee=0;ee<4;ee++)
        {
          float n[] = {ee < 2 ? vkdt.wstate.state[ee] : 0, ee >= 2 ? vkdt.wstate.state[ee] : 0}, v[2];
          dt_image_to_view(&vkdt.wstate.img_widget, n, v);
          float dist2 =
            ee < 2 ?
            (v[0]-m[0])*(v[0]-m[0]) :
            (v[1]-m[1])*(v[1]-m[1]);
          if(dist2 < px_dist*px_dist)
          {
            if(dist2 < max_dist)
            {
              max_dist = dist2;
              edge_hovered = ee;
              if(nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT))
                vkdt.wstate.selected = ee;
            }
          }
        }
      }
      if(edge_hovered >= 0)
      {
        float o = vkdt.state.center_wd * 0.02;
        float q0[8] = { p[0], p[1], p[2], p[3], p[2],   p[3]-o, p[0],   p[1]-o};
        float q1[8] = { p[2], p[3], p[4], p[5], p[4]+o, p[5],   p[2]+o, p[3]};
        float q2[8] = { p[4], p[5], p[6], p[7], p[6],   p[7]+o, p[4],   p[5]+o};
        float q3[8] = { p[6], p[7], p[0], p[1], p[0]-o, p[1],   p[6]-o, p[7]};
        float *q = q0;
        if(edge_hovered == 0) q = q3;
        if(edge_hovered == 1) q = q1;
        if(edge_hovered == 2) q = q0;
        if(edge_hovered == 3) q = q2;
        nk_fill_polygon(buf, q, 4, (struct nk_color){0x77,0x77,0x77,0x77});

        static float epos[2];
        if(nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT))
        { // remember offset mouse clicked pos<->original edge
          float ced[2] = {
            vkdt.wstate.state[vkdt.wstate.selected],
            vkdt.wstate.state[vkdt.wstate.selected]};
          dt_image_to_view(&vkdt.wstate.img_widget, ced, epos);
        }
        float v[] = {epos[0]+pos.x-cpos.x, epos[1]+pos.y-cpos.y}, n[2] = {0};
        dt_image_from_view(&vkdt.wstate.img_widget, v, n);
        float edge = vkdt.wstate.selected < 2 ? n[0] : n[1];
        dt_gui_dr_crop_adjust(edge, 0);
      }
      if(vkdt.wstate.selected < 0) do_events = 1; // enable zoom/panning around
    }
    else if(widget == dt_token("pick"))
    {
      float v[8] = {
        vkdt.wstate.state[0], vkdt.wstate.state[2], vkdt.wstate.state[1], vkdt.wstate.state[2], 
        vkdt.wstate.state[1], vkdt.wstate.state[3], vkdt.wstate.state[0], vkdt.wstate.state[3]
      };
      float p[8];
      for(int k=0;k<4;k++)
        dt_image_to_view(&vkdt.wstate.img_widget, v+2*k, p+2*k);
      nk_stroke_polygon(buf, p, 4, 1.0, nk_white);
      float vv[] = {pos.x, pos.y}, n[2] = {0};
      dt_image_from_view(&vkdt.wstate.img_widget, vv, n);
      if(hovered && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT))
      {
        vkdt.wstate.state[0] = n[0];
        vkdt.wstate.state[1] = n[0];
        vkdt.wstate.state[2] = n[1];
        vkdt.wstate.state[3] = n[1];
      }
      if(hovered && nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT))
      {
        vkdt.wstate.state[0] = MIN(vkdt.wstate.state[0], n[0]);
        vkdt.wstate.state[1] = MAX(vkdt.wstate.state[1], n[0]);
        vkdt.wstate.state[2] = MIN(vkdt.wstate.state[2], n[1]);
        vkdt.wstate.state[3] = MAX(vkdt.wstate.state[3], n[1]);
      }
    }
    else if(widget == dt_token("ab"))
    {
      float v[2] = { vkdt.wstate.state[0], 0.5 };
      float p[2];
      dt_image_to_view(&vkdt.wstate.img_widget, v, p);
      nk_stroke_line(buf, p[0], vkdt.state.center_y, p[0], vkdt.state.center_ht, 1.0, nk_white);
      float vv[] = {pos.x, pos.y}, n[2] = {0};
      dt_image_from_view(&vkdt.wstate.img_widget, vv, n);
      if(hovered && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT))
        vkdt.wstate.state[0] = n[0];
      if(hovered && nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT))
      {
        ((float *)dt_module_param_float(vkdt.graph_dev.module+vkdt.wstate.active_widget_modid,
            vkdt.wstate.active_widget_parid))[0] = vkdt.wstate.state[0] = n[0];
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
      }
    }
    else if(widget == dt_token("draw"))
    { // draw indicators for opacity/hardness/radius for each stroke
      float radius   = vkdt.wstate.state[0];
      float opacity  = vkdt.wstate.state[1];
      float hardness = vkdt.wstate.state[2];
      float p[100];
      int cnt = sizeof(p)/sizeof(p[0])/2;

      const int modid = vkdt.wstate.active_widget_modid;
      const float scale = vkdt.wstate.img_widget.scale *
        MIN(vkdt.state.center_wd / (float)
            vkdt.graph_dev.module[modid].connector[0].roi.wd,
            vkdt.state.center_ht / (float)
            vkdt.graph_dev.module[modid].connector[0].roi.ht);
      for(int i=0;i<2;i++)
      { // ui scaled roi wd * radius * stroke radius
        vkdt.wstate.state[4] = vkdt.graph_dev.module[modid].connector[0].roi.wd;
        float r = vkdt.wstate.state[4] * vkdt.wstate.state[3] * radius;
        if(i >= 1) r *= hardness;
        for(int k=0;k<cnt;k++)
        {
          p[2*k+0] = pos.x + scale * r*sinf(k/(cnt-1.0)*M_PI*2.0);
          p[2*k+1] = pos.y + scale * r*cosf(k/(cnt-1.0)*M_PI*2.0);
        }
        nk_stroke_polyline(buf, p, cnt, 4.0f/(i+1.0f), nk_white);
      }
      // opacity is a quad
      float sz = 30.0f;
      p[0] = pos.x; p[1] = pos.y;
      p[4] = pos.x+sz; p[5] = pos.y+sz;
      p[2] = opacity * (pos.x+sz) + (1.0f-opacity)*(pos.x+sz/2.0f);
      p[3] = opacity *  pos.y     + (1.0f-opacity)*(pos.y+sz/2.0f);
      p[6] = opacity *  pos.x     + (1.0f-opacity)*(pos.x+sz/2.0f);
      p[7] = opacity * (pos.y+sz) + (1.0f-opacity)*(pos.y+sz/2.0f);
      nk_stroke_polygon(buf, p, 4, 3.0, nk_white);

      uint32_t *dat = (uint32_t *)vkdt.wstate.mapped;
      if(hovered && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_RIGHT))
      { // right mouse click resets the last stroke
        for(int i=dat[0]-2;i>=0;i--)
        {
          if(i == 0 || dat[1+2*i+1] == 0) // detected end marker
          {
            dat[0] = i; // reset count
            break;
          }
        }
        // trigger recomputation:
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
        vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].flags = s_module_request_read_source;
      }
      float yoff = ctx->input.mouse.scroll_delta.y;
      int shift = glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_SHIFT)   == GLFW_PRESS || glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_SHIFT)   == GLFW_PRESS;
      int ctrl  = glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
      if(hovered && yoff != 0)
      {
        ctx->input.mouse.scroll_delta.y = 0; // handled this
        const float scale = yoff > 0.0 ? 1.2f : 1.0/1.2f;
        if(shift) // opacity
          vkdt.wstate.state[1] = CLAMP(vkdt.wstate.state[1] * scale, 0.1f, 1.0f);
        else if(ctrl) // hardness
          vkdt.wstate.state[2] = CLAMP(vkdt.wstate.state[2] * scale, 0.1f, 1.0f);
        else // radius
          vkdt.wstate.state[0] = CLAMP(vkdt.wstate.state[0] * scale, 0.1f, 1.0f);
      }
      if(!vkdt.wstate.pentablet_enabled)
      {
        float v[] = {pos.x, pos.y}, n[2] = {0};
        dt_image_from_view(&vkdt.wstate.img_widget, v, n);
        if(hovered && nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT))
          dt_gui_dr_draw_position(n, 1.0f);
        else if(!shift)
          dt_gui_dr_draw_position(n, 0.0f);
      }
    }
  }

  if(!do_events) return; // on-canvas got the interaction

  if(hovered)
  { // zoom/pan interaction
    struct nk_vec2 mpos = ctx->input.mouse.pos;
    if(!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT) &&
       !nk_input_is_mouse_down(&ctx->input, NK_BUTTON_MIDDLE))
    {
      w->m_x = w->m_y = -1;
    }
    if((!vkdt.wstate.pentablet_enabled && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) ||
       ( vkdt.wstate.pentablet_enabled && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_MIDDLE)))
    { // click to pan
      if(w->m_x < 0)
      {
        w->old_look_x = w->look_at_x;
        w->old_look_y = w->look_at_y;
      }
      w->m_x = mpos.x;
      w->m_y = mpos.y;
    }
    else if(!vkdt.wstate.pentablet_enabled && nk_input_mouse_clicked(&ctx->input, NK_BUTTON_MIDDLE, bounds))
    { // middle click to 1:1
      dt_image_zoom(w, mpos.x, mpos.y);
    }
    float yoff = ctx->input.mouse.scroll_delta.y;
    if(yoff != 0)
    { // mouse wheel (yoff is the input)
      ctx->input.mouse.scroll_delta.y = 0;
      w->scale *= yoff > 0.0f ? 1.1f : 0.9f;
    }
  }
  else
  { // reset stuff, but enable drag outside:
    if(!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT) &&
       !nk_input_is_mouse_down(&ctx->input, NK_BUTTON_MIDDLE))
    {
      w->m_x = w->m_y = -1;
    }
  }
  if(w->m_x > 0)
  { // mouse moved while dragging
    float img0[2], img1[2];
    float view0[2] = {
      ctx->input.mouse.buttons[0].clicked_pos.x,
      ctx->input.mouse.buttons[0].clicked_pos.y};
    float view1[2] = {
      ctx->input.mouse.pos.x,
      ctx->input.mouse.pos.y};
    dt_image_from_view(w, view0, img0);
    dt_image_from_view(w, view1, img1);
    w->look_at_x = w->old_look_x-img1[0]+img0[0];
    w->look_at_y = w->old_look_y-img1[1]+img0[1];
  }
  w->look_at_x = CLAMP(w->look_at_x, -0.5f, 0.5f);
  w->look_at_y = CLAMP(w->look_at_y, -0.5f, 0.5f);
}

static inline void
dt_image(
    struct nk_context *ctx,     // this is crucial to run on secondary screen
    dt_image_widget_t *w,
    dt_node_t         *out,     // make sure the out->dset is valid!
    int                events,  // if !=0 provide zoom/pan interaction
    int                main)    // if !=0 do on-canvas ui elements
{
  if(!out) return;
  w->out = out;
  w->wd = (float)out->connector[0].roi.wd;
  w->ht = (float)out->connector[0].roi.ht;
  struct nk_rect wb = nk_widget_bounds(ctx);
  w->win_x = wb.x; w->win_y = wb.y;
  w->win_w = wb.w; w->win_h = wb.h;
  float im0[2], im1[2];
  float v0[2] = {w->win_x, w->win_y};
  float v1[2] = {w->win_x+w->win_w, w->win_y+w->win_h};
  dt_image_from_view(w, v0, im0);
  dt_image_from_view(w, v1, im1);
  im0[0] = CLAMP(im0[0], 0.0f, 1.0f);
  im0[1] = CLAMP(im0[1], 0.0f, 1.0f);
  im1[0] = CLAMP(im1[0], 0.0f, 1.0f);
  im1[1] = CLAMP(im1[1], 0.0f, 1.0f);
  dt_image_to_view(w, im0, v0);
  dt_image_to_view(w, im1, v1);
  const int display_frame = out->module->graph->double_buffer;
  struct nk_rect subimg = {w->wd * im0[0], w->ht * im0[1], w->wd * (im1[0]-im0[0]), w->ht * (im1[1]-im0[1])};
  struct nk_rect disp = {v0[0], v0[1], v1[0]-v0[0], v1[1]-v0[1]};
  float scale1 = 1.0f/MIN(w->win_w/w->wd, w->win_h/w->ht);
  float ns = w->scale / scale1;
  if(ns >= 1.0f)
  { // make sure we align pixels accurately. avoids jittered display for not-quite-accurate correspondence
    // of image pixels to multiple screen pixels. the cost is jittering image display size lower/right.
    subimg.x = (int)subimg.x; subimg.y = (int)subimg.y;
    subimg.w = (int)(subimg.w+0.5f); subimg.h = (int)(subimg.h+0.5f);
    disp.x = MAX((int)w->win_x, (int)disp.x); disp.y = MAX((int)w->win_y, (int)disp.y);
    disp.w = (int)(ns * subimg.w); disp.h = (int)(ns * subimg.h);
  }
  struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
  struct nk_image nkimg = nk_subimage_ptr(out->dset[display_frame], w->wd, w->ht, subimg);
  int hover = nk_input_is_mouse_hovering_rect(&ctx->input, disp);
  nk_draw_image(buf, disp, &nkimg, (struct nk_color){0xff,0xff,0xff,0xff});
  char scaletext[10];
  if(ns >= 1.0f)
  {
    snprintf(scaletext, sizeof(scaletext), "%d%%", (int)(ns*100.0));
    nk_draw_text(buf, (struct nk_rect){w->win_x+0.9*w->win_w,w->win_y+0.05*w->win_h, 0.5*w->win_w, 0.05*w->win_h},
        scaletext, strlen(scaletext), nk_glfw3_font(0), (struct nk_color){0}, (struct nk_color){0xff,0xff,0xff,0xff});
  }
  // now the controls:
  if(events) dt_image_events(ctx, w, hover, main, disp);
}
