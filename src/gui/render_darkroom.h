#pragma once
// some routines shared between node editor and darkroom mode
#include "widget_tooltip.h"

static inline void
dt_gui_set_lod(int lod)
{
  if(vkdt.wstate.lod == lod) return;
  vkdt.wstate.lod = lod;
  // set graph output scale factor and
  // trigger complete pipeline rebuild
  const int mid = dt_module_get(&vkdt.graph_dev, dt_token("display"), dt_token("main"));
  if(mid < 0) return;
  if(lod > 1)
  {
    vkdt.graph_dev.module[mid].connector[0].max_wd = vkdt.state.center_wd / (lod-1);
    vkdt.graph_dev.module[mid].connector[0].max_ht = vkdt.state.center_ht / (lod-1);
  }
  else
  {
    vkdt.graph_dev.module[mid].connector[0].max_wd = 0;
    vkdt.graph_dev.module[mid].connector[0].max_ht = 0;
  }
  vkdt.wstate.busy++;
  vkdt.graph_dev.runflags = s_graph_run_all;
}

static inline void
widget_end()
{
  dt_gui_set_lod(vkdt.wstate.lod_fine);
  if(!vkdt.wstate.grabbed)
  {
    if(vkdt.wstate.active_widget_modid < 0) return; // all good already
    // rerun all (roi could have changed, buttons are drastic)
    vkdt.graph_dev.runflags = s_graph_run_all;
    int modid = vkdt.wstate.active_widget_modid;
    int parid = vkdt.wstate.active_widget_parid;
    int parnm = vkdt.wstate.active_widget_parnm;
    int parsz = vkdt.wstate.active_widget_parsz;
    if(vkdt.wstate.mapped)
    {
      vkdt.wstate.mapped = 0;
    }
    else if(parsz)
    {
      const dt_ui_param_t *p = vkdt.graph_dev.module[modid].so->param[parid];
      float *v = (float*)(vkdt.graph_dev.module[modid].param + p->offset + parsz * parnm);
      memcpy(v, vkdt.wstate.state, parsz);
    }
  }
  dt_gui_ungrab_mouse();
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.selected = -1;
}
static inline void widget_abort()
{
  if(!vkdt.wstate.grabbed)
  {
    if(vkdt.wstate.active_widget_modid < 0) return; // all good already
    // rerun all (roi could have changed, buttons are drastic)
    vkdt.graph_dev.runflags = s_graph_run_all;
    if(vkdt.wstate.mapped) vkdt.wstate.mapped = 0; // mapped can't really abort, need to rollback via history.
  }
  dt_gui_ungrab_mouse();
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.selected = -1;
}

static inline void render_perf_overlay()
{
  const int nvmask = 127;
  static float values[128] = {0.0f};
  static int values_offset = 0;
  char overlay[32];
  values[values_offset] = vkdt.graph_dev.query[(vkdt.graph_dev.frame+1)%2].last_frame_duration;
  snprintf(overlay, sizeof(overlay), "%.2fms", values[values_offset]);

  struct nk_context *ctx = &vkdt.ctx;
  struct nk_command_buffer *buf = nk_window_get_canvas(ctx);
  float sx = vkdt.state.center_wd, sy = vkdt.state.center_ht;
  float px = vkdt.state.center_x + sx/2, py = vkdt.state.center_y + sy*0.1;
  sx = sx/2;
  sy = sy*0.1;
  struct nk_color col = {0x77, 0x77, 0x77, 0xff};
  for(int i=0;i<sizeof(values)/sizeof(values[0])-1;i++)
  {
    int j0 = (i + values_offset    )&nvmask;
    int j1 = (i + values_offset + 1)&nvmask;
    nk_stroke_line(buf,
        px + sx * (i    /(nvmask+1.0)), py + sy * (1.0 - values[j0]/100.0),
        px + sx * ((i+1)/(nvmask+1.0)), py + sy * (1.0 - values[j1]/100.0),
        4.0, col);
  }
  struct nk_color bgcol = {0xcc, 0xcc, 0xcc, 0xff};
  nk_stroke_line(buf, px, py, px+sx, py, 4.0, bgcol);
  nk_stroke_line(buf, px, py+sy, px+sx, py+sy, 4.0, bgcol);
  nk_stroke_line(buf, px, py+(1-16.0/100.0)*sy, px+sx, py+(1-16.0/100.0)*sy, 4.0, bgcol);
  nk_draw_text(buf, (struct nk_rect){px, py, sx,sy}, overlay, strlen(overlay), nk_glfw3_font(2), (struct nk_color){0x77,0x77,0x77,0xff}, col);
  values_offset = (values_offset + 1) & nvmask;
}

static inline void
render_darkroom_widget(int modid, int parid, int is_fav_menu)
{
  const dt_ui_param_t *param = vkdt.graph_dev.module[modid].so->param[parid];
  if(!param) return;
  struct nk_context *ctx = &vkdt.ctx;

  // skip if group mode does not match:
  if(param->widget.grpid != -1)
  {
    if(param->widget.mode <  100 && dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.grpid)[0] != param->widget.mode)
      return;
    if(param->widget.mode >= 100 && dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.grpid)[0] == param->widget.mode-100)
      return;
  }

#define RESETBLOCK \
  {\
  struct nk_rect bounds = nk_widget_bounds(ctx);\
  struct nk_vec2 size = {bounds.w, bounds.w};\
  if(nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_DOUBLE, bounds)) \
  { \
    memcpy(vkdt.graph_dev.module[modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));\
    change = 1; \
  }\
  if(nk_contextual_begin(ctx, 0, size, bounds))\
  {\
    nk_layout_row_dynamic(ctx, row_height, 1);\
    if(is_fav_menu)\
    {\
      if(nk_contextual_item_label(ctx, "remove from favs", NK_TEXT_LEFT))\
        dt_gui_remove_fav(vkdt.graph_dev.module[modid].name, vkdt.graph_dev.module[modid].inst, param->name);\
      if(nk_contextual_item_label(ctx, "move up", NK_TEXT_LEFT))\
        dt_gui_move_fav(vkdt.graph_dev.module[modid].name, vkdt.graph_dev.module[modid].inst, param->name, 1);\
      if(nk_contextual_item_label(ctx, "move down", NK_TEXT_LEFT))\
        dt_gui_move_fav(vkdt.graph_dev.module[modid].name, vkdt.graph_dev.module[modid].inst, param->name, 0);\
    }\
    else if(nk_contextual_item_label(ctx, "add to favs", NK_TEXT_LEFT))\
      dt_gui_add_fav(vkdt.graph_dev.module[modid].name, vkdt.graph_dev.module[modid].inst, param->name);\
    if(vkdt.graph_dev.frame_cnt != 1 && param->type == dt_token("float") && nk_contextual_item_label(ctx, "add keyframe", NK_TEXT_LEFT))\
      dt_gui_keyframe_add(modid, parid);\
    nk_contextual_end(ctx);\
  }}

#ifndef KEYFRAME // enable node graph editor to switch this off
  // common code block to insert a keyframe. currently only supports float (for interpolation)
#define KEYFRAME\
  if(nk_widget_is_hovered(ctx))\
  {\
    if(gui.hotkey == s_hotkey_insert_keyframe)\
    {\
      dt_gui_keyframe_add(modid, parid);\
      gui.hotkey = -1;\
    }\
  }
#endif

  const double throttle = 2.0; // min delay for same param in history, in seconds
  // distinguish by count:
  // get count by param cnt or explicit multiplicity from ui file
  int count = 1, change = 0;
  if(param->name == dt_token("draw")) // TODO: wire through named count too?
    count = 2*dt_module_param_int(vkdt.graph_dev.module + modid, parid)[0]+1; // vertex count + list of 2d vertices
  else if(param->widget.cntid == -1)
    count = param->cnt; // if we know nothing else, we use all elements
  else
    count = CLAMP(dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.cntid)[0], 0, param->cnt);
  const float pwd = vkdt.state.panel_wd - ctx->style.window.scrollbar_size.x - 2*ctx->style.window.padding.x;
  const float ratio[] = {0.7f, 0.3f};
  const float wds[] = { ratio[0]*pwd - ctx->style.window.spacing.x, ratio[1]*pwd};
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  if(param->widget.sep)
  { // draw separator before widget
    nk_layout_row_dynamic(ctx, row_height/2, 1);
    nk_label(ctx, "", 0);
  }

  for(int num=0;num<count;num++)
  {
  char string[256];
  // const float halfw = (0.66*vkdt.state.panel_wd - ctx.style.tab.padding.x)/2;
  char nkid[30];
  snprintf(nkid, sizeof(nkid), "%"PRItkn":%"PRItkn":%"PRItkn,
      dt_token_str(vkdt.graph_dev.module[modid].name),
      dt_token_str(vkdt.graph_dev.module[modid].inst),
      dt_token_str(param->name));
  char str[10] = { 0 };
  memcpy(str, &param->name, 8);
  // distinguish by type:
  if(param->widget.type == dt_token("slider"))
  {
    nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
    if(param->type == dt_token("float"))
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      float oldval = *val;
      RESETBLOCK
      struct nk_rect bounds = nk_widget_bounds(ctx);
      nk_tab_property(float, ctx, nkid, param->widget.min, val, param->widget.max,
          (param->widget.max - param->widget.min)/100.0,
          (param->widget.max - param->widget.min)/(0.6*vkdt.state.center_wd));
      // draw fill level
      struct nk_color col = nk_rgba(255,255,255,30);
      struct nk_rect bar = nk_rect(bounds.x + 0.1*bounds.w, bounds.y + 0.15*bounds.h,
          bounds.w * 0.8*(*val - param->widget.min)/(param->widget.max - param->widget.min), bounds.h * 0.7);
      nk_fill_rect(nk_window_get_canvas(ctx), bar, 0, col);
      // draw default value
      col = nk_rgba(0,0,0,40);
      float defval = param->val[num];
      bar = nk_rect(bounds.x + 0.1*bounds.w + bounds.w * 0.8*(defval - param->widget.min)/(param->widget.max - param->widget.min),
          bounds.y + 0.15*bounds.h, 0.01*bounds.w, 0.7*bounds.h);
      nk_fill_rect(nk_window_get_canvas(ctx), bar, 0, col);

#ifdef ROTARY_ENCODER
      if(nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
        if(gui.pgupdn != 0)
          *val = *val + gui.pgupdn * (param->widget.max - param->widget.min)/100.0;
#endif
      if(nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
      { // update lod, if user requested:
        if(vkdt.wstate.interact_begin) dt_gui_set_lod(vkdt.wstate.lod_interact);
        if(vkdt.wstate.interact_end)   dt_gui_set_lod(vkdt.wstate.lod_fine);
      }

      if(*val != oldval) change = 1;
      if(change)
      {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | flags;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
    }
    else if(param->type == dt_token("int"))
    {
      int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      int32_t oldval = *val;
      RESETBLOCK
      nk_tab_property(int, ctx, nkid, param->widget.min, val, param->widget.max,
          (int)(1.0+(param->widget.max - param->widget.min)/100.0),
          ((param->widget.max - param->widget.min)/(0.6*vkdt.state.center_wd)));
      if(*val != oldval) change = 1;
      if(change)
      {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
    }
    dt_tooltip(param->tooltip);
    KEYFRAME
    nk_label(ctx, str, NK_TEXT_LEFT);
  }
  else if(param->widget.type == dt_token("rgbknobs"))
  { // only works for param->type == float and count == 4
    if((num % 4) == 0 && num+4 <= count)
    {
      const float wd = ratio[0]*pwd/5.0f - ctx->style.window.spacing.x * 4.0f/5.0f;
      const float w5[] = { wd, wd, wd, 2*wd, ratio[1]*pwd};
      nk_layout_row(ctx, NK_STATIC, row_height, 5, w5);
      float *val = (float *)(vkdt.graph_dev.module[modid].param + param->offset) + 4*num;
      struct nk_colorf oldval = {val[0], val[1], val[2], 1.0};
      struct nk_command_buffer *cmd = &vkdt.global_cmd;
      // cache previous hsv so we can reconstruct hue and col for rgb=0
      static struct nk_colorf cc_hsv[9] = {{0}};
      static int cc_id[9] = {0};
      const int hash = (7*modid + parid)%9;
      if(cc_id[hash] != ((modid << 16) | parid))
      {
        cc_id[hash] = (modid<<16) | parid;
        cc_hsv[hash] = (struct nk_colorf){0};
      }
      const float dead_angle = 60.0f;
      struct nk_colorf hsv = rgb2hsv_prior(val[0], val[1], val[2], cc_hsv[hash]);
      const int mode = (int)CLAMP(val[3], 0, 1); // 0 rgb, 1 lshsv

#ifdef ROTARY_ENCODER
#define ROTARY_KNOB(VAL, M) do {\
      if(nk_input_is_mouse_hovering_rect(&ctx->input, bounds)) {\
        if(gui.pgupdn != 0) VAL = CLAMP(VAL + gui.pgupdn/100.0, 0.0, M);\
      }} while(0)
#else
#define ROTARY_KNOB(A, B)
#endif

      // if not mouse down and hovering over bounds, or mouse clicked pos is in bounds
#define DECORATE(VAL, COL, SC, NUM) do {\
      if((!ctx->input.mouse.buttons[NK_BUTTON_LEFT].down && nk_input_is_mouse_hovering_rect(&ctx->input, bounds)) || \
         ( ctx->input.mouse.buttons[NK_BUTTON_LEFT].down && \
         NK_INBOX(ctx->input.mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x,ctx->input.mouse.buttons[NK_BUTTON_LEFT].clicked_pos.y,bounds.x,bounds.y,bounds.w,bounds.h)))\
      {\
        if(ctx->input.mouse.buttons[NK_BUTTON_LEFT].down) {\
          if(vkdt.wstate.active_widget_modid != modid && vkdt.wstate.active_widget_parid != parid &&\
             vkdt.wstate.active_widget_parnm != NUM) widget_end();\
          dt_gui_set_lod(vkdt.wstate.lod_interact);\
          vkdt.wstate.active_widget_modid = modid;\
          vkdt.wstate.active_widget_parid = parid;\
          vkdt.wstate.active_widget_parnm = NUM;\
          vkdt.wstate.active_widget_parsz = 0;\
        }\
        const float c[] = { bounds.x + bounds.w/2.0, bounds.y + bounds.h/2.0 };\
        int N = 40;\
        float phi = (3.0f/2.0f*M_PI-dead_angle/2.0f*M_PI/180.0f), delta_phi = (2.0f*M_PI - dead_angle*M_PI/180.0f)/N,\
              r0 = 0.3*pwd, r1 = 0.4*pwd;\
        struct nk_rect valrect = {c[0]-bounds.w, c[1]+0.3*pwd,\
            2*bounds.w, bounds.h};\
        nk_fill_rect(cmd, valrect, bounds.h*0.4, (struct nk_color){0,0,0,0xff});\
        valrect.x += 0.1*bounds.h; valrect.y += 0.1*bounds.h;\
        char valstr[15];\
        snprintf(valstr, sizeof(valstr), "%4.4f", VAL);\
        nk_draw_text(cmd, valrect,\
            valstr, strlen(valstr),\
            nk_glfw3_font(0),\
            (struct nk_color){0,0,0,0xff},\
            (struct nk_color){0xff,0xff,0xff,0xff});\
        for(int k=0;k<=N;k++)\
        {\
          if(k==N) {\
            float wd = 5.0f;\
            phi = (3.0f/2.0f*M_PI-(dead_angle/2.0f-wd/2.0f+VAL*(360.0f-dead_angle))*M_PI/180.0f);\
            delta_phi = wd*M_PI/180.0f;\
            r0 = 0.28*pwd; r1 = 0.41*pwd;\
          }\
          float x[] = {\
            c[0] + cosf(phi+0.013) * r1,           c[1] - sinf(phi+0.013) * r1,\
            c[0] + cosf(phi-delta_phi-0.013) * r1, c[1] - sinf(phi-delta_phi-0.013) * r1,\
            c[0] + cosf(phi-delta_phi-0.013) * r0, c[1] - sinf(phi-delta_phi-0.013) * r0,\
            c[0] + cosf(phi+0.013) * r0,           c[1] - sinf(phi+0.013) * r0};\
          struct nk_color c = nk_rgba_cf((struct nk_colorf){MAX(0.0f, SC*COL.r), MAX(0.0f, SC*COL.g), MAX(0.0f, SC*COL.b), 1.0f});\
          nk_fill_polygon(cmd, x, 4, k==N ? nk_rgba(100,100,100,200) : c);\
          phi -= delta_phi;\
        }\
      } else if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid &&\
                vkdt.wstate.active_widget_parnm == NUM) widget_end();\
      } while(0)

      if(mode == 0)
      { // rgb
        nk_style_push_color(ctx, &ctx->style.knob.knob_normal, nk_rgba_f(0.5, 0.1, 0.1, 1.0));
        struct nk_rect bounds = nk_widget_bounds(ctx);
        nk_knob_float(ctx, 0.0, val+0, 1.0, 1.0/100.0, NK_DOWN, dead_angle);
        nk_style_pop_color(ctx);
        DECORATE(val[0], nk_rgba_f((k+0.5)/N, val[1], val[2], 1.0f), 1.0f/256.0f, 0);
        ROTARY_KNOB(val[0], 1.0);

        nk_style_push_color(ctx, &ctx->style.knob.knob_normal, nk_rgba_f(0.1, 0.5, 0.1, 1.0));
        bounds = nk_widget_bounds(ctx);
        nk_knob_float(ctx, 0.0, val+1, 1.0, 1.0/100.0, NK_DOWN, dead_angle);
        nk_style_pop_color(ctx);
        DECORATE(val[1], nk_rgba_f(val[0], (k+0.5)/N, val[2], 1.0f), 1.0f/256.0f, 1);
        ROTARY_KNOB(val[1], 1.0);

        nk_style_push_color(ctx, &ctx->style.knob.knob_normal, nk_rgba_f(0.1, 0.1, 0.5, 1.0));
        bounds = nk_widget_bounds(ctx);
        nk_knob_float(ctx, 0.0, val+2, 1.0, 1.0/100.0, NK_DOWN, dead_angle);
        nk_style_pop_color(ctx);
        DECORATE(val[2], nk_rgba_f(val[0], val[1], (k+0.5)/N, 1.0f), 1.0f/256.0f, 2);
        ROTARY_KNOB(val[2], 1.0);

        if(memcmp(&oldval, val, sizeof(float)*3)) change = 1;
      }
      else if(mode == 1)
      { // hsluv
        nk_style_push_color(ctx, &ctx->style.knob.knob_normal, nk_rgba_cf(hsv2rgb(hsv.r, 1.0, 1.0)));
        struct nk_rect bounds = nk_widget_bounds(ctx);
        nk_knob_float(ctx, 0.0, &hsv.r, 1.0, 1.0/100.0, NK_DOWN, dead_angle); // H
        nk_style_pop_color(ctx);
        DECORATE(hsv.r, hsv2rgb((k+0.5)/N, .4, 0.6), 1.0f, 0);
        ROTARY_KNOB(hsv.r, 1.0);

        nk_style_push_color(ctx, &ctx->style.knob.knob_normal, nk_rgba_cf(hsv2rgb(hsv.r, hsv.g, 1.0)));
        bounds = nk_widget_bounds(ctx);
        nk_knob_float(ctx, 0.0, &hsv.g, 1.0, 1.0/100.0, NK_DOWN, dead_angle); // S
        nk_style_pop_color(ctx);
        DECORATE(hsv.g, hsv2rgb(hsv.r, (k+0.5)/N, 0.5), 1.0f, 1);
        ROTARY_KNOB(hsv.g, 1.0);

        // nk_style_push_color(ctx, &ctx->style.knob.knob_normal, nk_rgba_cf(hsv2rgb(hsv.r, hsv.g, hsv.b)));
        bounds = nk_widget_bounds(ctx);
        nk_knob_float(ctx, 0.0, &hsv.b, 1.0, 1.0/100.0, NK_DOWN, dead_angle); // V
        // nk_style_pop_color(ctx);
        DECORATE(hsv.b, hsv2rgb(hsv.r, hsv.g, (k+0.5)/N), 1.0f, 2);
        ROTARY_KNOB(hsv.b, 1.0);

        struct nk_colorf hsv_old = rgb2hsv_prior(oldval.r, oldval.g, oldval.b, cc_hsv[hash]);
        if(memcmp(&hsv_old, &hsv, sizeof(float)*3)) change = 1;
      }
#undef DECORATE
#undef ROTARY_KNOB

      struct nk_vec2 size = { ratio[1]*pwd, ratio[0]*pwd };
      const char *mode_str = "rgb\0hsluv\0\0";
      int newmode = mode;
      nk_combobox_string(ctx, mode_str, &newmode, 0x7fff, row_height, size);
      if(newmode != mode) change = 1;
      if(change) // don't do this if resetblock triggers change
      {
        if(mode == 1)
        { // don't convert rgb
          struct nk_colorf rgb = hsv2rgb(hsv.r, hsv.g, hsv.b);
          val[0] = rgb.r; val[1] = rgb.g; val[2] = rgb.b;
        }
        val[3] = newmode;
      }
      dt_tooltip("%s\n"
                 "select the colour picker mode of operation:\n"
                 "rgb: pick rgb channels\n"
                 "hsluv: pick human friendly hsl",
                 param->tooltip);
      KEYFRAME
      RESETBLOCK
      nk_label(ctx, str, NK_TEXT_LEFT);
      if(change)
      {
        if(hsv.g > 0.0 && hsv.b > 0.0) cc_hsv[hash] = hsv;
        else cc_hsv[hash] = (struct nk_colorf){cc_hsv[hash].r, hsv.g, hsv.b};
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
    }
  }
  else if(param->widget.type == dt_token("coledit"))
  { // only works for param->type == float and count == 3
    if((num % 3) == 0)
    {
      nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
      struct nk_colorf *val = (struct nk_colorf *)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      struct nk_colorf oldval = *val;
      float size = nk_widget_width(ctx);
      RESETBLOCK
      if(nk_combo_begin_color(ctx, nk_rgb_cf(*val), nk_vec2(pwd, size+4.0*row_height)))
      {
        nk_layout_row_dynamic(ctx, size, 1);
        *val = nk_color_picker(ctx, *val, NK_RGB);
        nk_layout_row_dynamic(ctx, row_height, 1);
        val->r = nk_propertyf(ctx, "#R:", 0, val->r, 1.0f, 0.01f, 0.005f);
        val->g = nk_propertyf(ctx, "#G:", 0, val->g, 1.0f, 0.01f, 0.005f);
        val->b = nk_propertyf(ctx, "#B:", 0, val->b, 1.0f, 0.01f, 0.005f);
        nk_combo_end(ctx);
      }
      if(memcmp(val, &oldval, sizeof(float)*3)) change = 1;
      if(change)
      {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      dt_tooltip(param->tooltip);
      KEYFRAME
      nk_label(ctx, str, NK_TEXT_LEFT);
    }
  }
#if 0
  else if(param->widget.type == dt_token("vslider"))
  {
    if(param->type == dt_token("float"))
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      float oldval = *val;
      if(count == 3)
      { // assume rgb vsliders
      }
      else
      {
      }
    }
  }
#endif
  else if(param->widget.type == dt_token("bitmask"))
  { // select named entries of a bitmask
    if(param->type == dt_token("int"))
    {
      int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      int32_t oldval = *val;
      nk_layout_row_dynamic(ctx, row_height, 1);
      RESETBLOCK
      dt_tooltip(param->tooltip);
      nk_labelf(ctx, NK_TEXT_LEFT, "%s = 0x%x", str, val[0]);
      const char *c = (const char *)param->widget.data;
      nk_layout_row_static(ctx, row_height, pwd/9, 8);
      for(int k=0;k<32;k++)
      {
        const int sel = val[0] & (1<<k);
        if(sel)
        {
          nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
          nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
        }
        char label[10];
        snprintf(label, sizeof(label), "%d", k);
        dt_tooltip(c);
        if(nk_button_label(ctx, label)) { val[0] ^= (1<<k); change = 1; }
        if(sel)
        {
          nk_style_pop_style_item(ctx);
          nk_style_pop_color(ctx);
        }

        for(;*c!=0;c++);
        c++;
        if(*c == 0) break; // no more named bits in mask
      }

      if(change)
      {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        vkdt.wstate.busy += 2;
      }
    }
  }
  else if(param->widget.type == dt_token("callback"))
  { // special callback button
    if(num == 0)
    {
      nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
      if(nk_button_label(ctx, str))
      {
        dt_module_t *m = vkdt.graph_dev.module+modid;
        if(m->so->ui_callback) m->so->ui_callback(m, param->name);
        vkdt.graph_dev.runflags |= s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        vkdt.wstate.busy += 2;
      }
      dt_tooltip(param->tooltip);
      KEYFRAME
      if(param->type == dt_token("string"))
      {
        char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
        nk_tab_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, v, count, nk_filter_default);
      }
      else nk_label(ctx, "", NK_TEXT_LEFT);
    }
  }
  else if(param->widget.type == dt_token("combo"))
  { // combo box
    if(param->type == dt_token("int"))
    {
      nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
      int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      int32_t oldval = *val;
      RESETBLOCK
      struct nk_vec2 size = { ratio[0]*pwd, ratio[0]*pwd };
      nk_combobox_string(&vkdt.ctx, (const char *)param->widget.data, val, 0x7fff, row_height, size);
      if(oldval != *val) change = 1;
      if(change)
      {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        vkdt.wstate.busy += 2;
      }
      dt_tooltip(param->tooltip);
      KEYFRAME
      nk_label(ctx, str, NK_TEXT_LEFT);
    }
  }
  else if(param->widget.type == dt_token("btngrid"))
  { // grid of buttons
    if(param->type == dt_token("int"))
    {
      int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      int32_t oldval = *val;
      RESETBLOCK

      float bw = 0.333f*wds[0]-2.0f/3.0f*ctx->style.window.spacing.x;
      float r4[] = {bw, bw, bw, wds[1]};
      nk_layout_row(ctx, NK_STATIC, row_height, 4, r4);
      const char *c = (const char *)param->widget.data;
      int rowidx = 0;
      int firstrow = 1;
      while(1)
      {
        if(c[0])
        {
          int idx = CLAMP(c[0]-'0',0,9)*10+CLAMP(c[1]-'0',0,9);
          const int sel = *val == idx;
          if(sel)
          {
            nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
            nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
          }
          if(nk_button_label(ctx, c+2))
          {
            *val = idx;
            change = 1;
          }
          if(sel)
          {
            nk_style_pop_style_item(ctx);
            nk_style_pop_color(ctx);
          }
          rowidx++;
        }
        if(!c[0] || (rowidx % 3) == 0)
        { // second null byte found or row complete
          if(firstrow)
          {
            firstrow = 0;
            dt_tooltip(param->tooltip);
            KEYFRAME
            nk_label(ctx, str, NK_TEXT_LEFT);
          }
          else if(c[0]) nk_label(ctx, "", 0);
          rowidx = 0;
          if(!c[0]) break;
        }
        while(*c) c++;
        c++; // skip null byte
      }

      if(change)
      {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = flags | s_graph_run_record_cmd_buf;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        vkdt.wstate.busy += 2;
      }
    }
  }
  else if(param->widget.type == dt_token("colour"))
  {
    float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
    snprintf(string, sizeof(string), "%" PRItkn " %d", dt_token_str(param->name), num);
    if(num == 0) nk_layout_row_dynamic(ctx, row_height, 6);
    struct nk_color col = {val[0]*255, val[1]*255, val[2]*255, 0xff };
    if(nk_widget_is_hovered(ctx))
    {
      float xyzE[3] = {0}, xyz[3] = {0};
      const float rec2020_to_xyz[] = { // a copy from glsl row major, i.e. transposed
        6.36958048e-01, 2.62700212e-01, 4.20575872e-11,
        1.44616904e-01, 6.77998072e-01, 2.80726931e-02,
        1.68880975e-01, 5.93017165e-02, 1.06098506e+00};
      for(int i=0;i<3;i++) for(int j=0;j<3;j++)
        xyzE[i] += rec2020_to_xyz[3*j+i]*val[j];
      // fucking icc uses some fucking variant of D50 as a reference white in the
      // profile connection space, for XYZ! so now we do the same nonsense, just
      // to try and match the spotread -x output. we'll use Bradford adaptation,
      // as does icc:
      const float lindbloom_E_to_D50[] = { // too lazy to do the math myself
         0.9977545, -0.0041632, -0.0293713,
        -0.0097677,  1.0183168, -0.0085490,
        -0.0074169,  0.0134416,  0.8191853};
      for(int i=0;i<3;i++) for(int j=0;j<3;j++)
        xyz[i] += lindbloom_E_to_D50[3*i+j]*xyzE[j];
      const float sum = xyz[0]+xyz[1]+xyz[2];
      dt_tooltip("%s\n%5.3f %5.3f %5.3f bt2020\n%5.3f %5.3f %5.3f Yxy (D50)", param->tooltip,
          val[0], val[1], val[2], xyz[1], xyz[0]/sum, xyz[1]/sum);
    }
    nk_button_color(ctx, col);
  }
  else if(param->widget.type == dt_token("pers"))
  {
    nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
    const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
    const float aspect = iwd/iht;
    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      int accept = 0;
      snprintf(string, sizeof(string), "%" PRItkn" done", dt_token_str(param->name));
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, string) || accept)
      {
        // dt_gamepadhelp_pop();
        dt_module_set_param_float(vkdt.graph_dev.module+modid, dt_token("rotate"), vkdt.wstate.state[9]);
        dt_module_set_param_float_n(vkdt.graph_dev.module+modid, dt_token("crop"), vkdt.wstate.state+10, 4);
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      snprintf(string, sizeof(string), "%" PRItkn" start", dt_token_str(param->name));
      if(nk_button_label(ctx, string))
      {
        // dt_gamepadhelp_push();
        // dt_gamepadhelp_clear();
        // dt_gamepadhelp_set(dt_gamepadhelp_R1, "select next corner");
        // dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_R, "move corner");
        // dt_gamepadhelp_set(dt_gamepadhelp_button_cross, "accept changes");
        // dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "discard changes");
        widget_end(); // if another one is still in progress, end that now
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = 0;
        vkdt.wstate.active_widget_parsz = dt_ui_param_size(param->type, param->cnt);
        // copy to quad state
        memcpy(vkdt.wstate.state, v, sizeof(float)*8);
        float rot = dt_module_param_float(vkdt.graph_dev.module+modid, dt_module_get_param(vkdt.graph_dev.module[modid].so, dt_token("rotate")))[0];
        dt_module_set_param_float(vkdt.graph_dev.module+modid, dt_token("rotate"), 0.0f);
        vkdt.wstate.state[9] = rot;
        // reset module params so the image will not appear cropped:
        float defc[] = {
          .5f + MAX(1.0f, 1.0f/aspect) * (0.0f - .5f),
          .5f + MAX(1.0f, 1.0f/aspect) * (1.0f - .5f),
          .5f + MAX(1.0f,      aspect) * (0.0f - .5f),
          .5f + MAX(1.0f,      aspect) * (1.0f - .5f)};
        memcpy(vkdt.wstate.state+10, dt_module_param_float(vkdt.graph_dev.module+modid, dt_module_get_param(vkdt.graph_dev.module[modid].so, dt_token("crop"))), sizeof(float)*4);
        dt_module_set_param_float_n(vkdt.graph_dev.module+modid, dt_token("crop"), defc, 4);
        // reset module params so the image will not appear distorted:
        float def[] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
        memcpy(v, def, sizeof(float)*8);
        vkdt.graph_dev.runflags = s_graph_run_all;
        dt_image_widget_t *w = &vkdt.wstate.img_widget;
        w->scale = 0.9;
      }
      RESETBLOCK
      if(change)
      {
        vkdt.graph_dev.runflags = s_graph_run_all;
        dt_image_reset_zoom(&vkdt.wstate.img_widget);
      }
      dt_tooltip(param->tooltip);
      nk_label(ctx, str, NK_TEXT_LEFT);
    }
    num = count;
  }
  else if(param->widget.type == dt_token("straight"))
  { // horizon line straighten tool for rotation
    float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
    float oldval = *val;
    const float w3[] = {0.3f*pwd-ctx->style.window.spacing.x, 0.4f*pwd-ctx->style.window.spacing.x, wds[1]};
    nk_layout_row(ctx, NK_STATIC, row_height, 3, w3);

    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, "done"))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      dt_tooltip("draw lines on the image to be rotated to align exactly horizontally or vertically");
      if(nk_button_label(ctx, "straighten"))
      {
        widget_end();
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = 0;
        vkdt.wstate.active_widget_parsz = sizeof(float);
        vkdt.wstate.selected = -1;
        memcpy(vkdt.wstate.state, val, sizeof(float));
      }
    }

    // full manual control over parameter using the slider:
    RESETBLOCK
    nk_tab_property(float, ctx, "#", -360.0f, val, 360.0f, 
          (360.0f)/100.0,
          (360.0f)/(0.6*vkdt.state.center_wd));
    if(*val != oldval) change = 1;
    if(change)
    {
      dt_graph_run_t flags = s_graph_run_none;
      if(vkdt.graph_dev.module[modid].so->check_params)
        flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
      vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | flags;
      vkdt.graph_dev.active_module = modid;
      dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
    }
    dt_tooltip(param->tooltip);
    KEYFRAME
    nk_label(ctx, str, NK_TEXT_LEFT);
  }
  else if(param->widget.type == dt_token("crop"))
  {
    const float w3[] = {0.3f*pwd-ctx->style.window.spacing.x, 0.4f*pwd-ctx->style.window.spacing.x, wds[1]};
    nk_layout_row(ctx, NK_STATIC, row_height, 3, w3);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
    const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
    const float aspect = iwd/iht;
    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      int accept = 0;
      snprintf(string, sizeof(string), "%" PRItkn" done", dt_token_str(param->name));
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, string) || accept)
      {
        vkdt.wstate.state[0] = .5f + MAX(1.0f, 1.0f/aspect) * (vkdt.wstate.state[0] - .5f);
        vkdt.wstate.state[1] = .5f + MAX(1.0f, 1.0f/aspect) * (vkdt.wstate.state[1] - .5f);
        vkdt.wstate.state[2] = .5f + MAX(1.0f,      aspect) * (vkdt.wstate.state[2] - .5f);
        vkdt.wstate.state[3] = .5f + MAX(1.0f,      aspect) * (vkdt.wstate.state[3] - .5f);
        widget_end();
        // dt_gamepadhelp_pop();
        dt_image_reset_zoom(&vkdt.wstate.img_widget);
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      snprintf(string, sizeof(string), "%" PRItkn" start", dt_token_str(param->name));
      if(nk_button_label(ctx, string))
      {
        // dt_gamepadhelp_push();
        // dt_gamepadhelp_clear();
        // dt_gamepadhelp_set(dt_gamepadhelp_R1, "select next edge");
        // dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_R, "move edge");
        // dt_gamepadhelp_set(dt_gamepadhelp_button_cross, "accept changes");
        // dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "discard changes");
        widget_end(); // if another one is still in progress, end that now
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = 0;
        vkdt.wstate.active_widget_parsz = dt_ui_param_size(param->type, param->cnt);
        // copy to quad state
        memcpy(vkdt.wstate.state, v, sizeof(float)*4);

        // the values we draw are relative to output of the whole pipeline,
        // but the coordinates of crop are relative to the *input*
        // coordinates of the module!
        // the output is the anticipated output while we switched off crop, i.e
        // using the default values to result in a square of max(iwd, iht)^2
        // first convert these v[] from input w/h to output w/h of the module:
        const float iwd = vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].connector[0].roi.wd;
        const float iht = vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].connector[0].roi.ht;
        const float owd = MAX(iwd, iht);
        const float oht = MAX(iwd, iht);
        float *c = vkdt.wstate.state;
        if(c[0] == 1.0 && c[1] == 3.0 && c[2] == 3.0 && c[3] == 7.0)
        {
          c[0] = c[2] = 0.0f;
          c[1] = c[3] = 1.0f;
        }
        vkdt.wstate.state[0] = .5f +  iwd/owd * (vkdt.wstate.state[0] - .5f);
        vkdt.wstate.state[1] = .5f +  iwd/owd * (vkdt.wstate.state[1] - .5f);
        vkdt.wstate.state[2] = .5f +  iht/oht * (vkdt.wstate.state[2] - .5f);
        vkdt.wstate.state[3] = .5f +  iht/oht * (vkdt.wstate.state[3] - .5f);

        // reset module params so the image will not appear cropped:
        float def[] = {
          .5f + MAX(1.0f, 1.0f/aspect) * (0.0f - .5f),
          .5f + MAX(1.0f, 1.0f/aspect) * (1.0f - .5f),
          .5f + MAX(1.0f,      aspect) * (0.0f - .5f),
          .5f + MAX(1.0f,      aspect) * (1.0f - .5f)};
        memcpy(v, def, sizeof(float)*4);
        vkdt.graph_dev.runflags = s_graph_run_all;
        dt_image_widget_t *w = &vkdt.wstate.img_widget;
        w->scale = 0.9;
      }
    }
    nk_tab_property(float, ctx, "#aspect", 0.0, &vkdt.wstate.aspect, 10.0, 0.1, .001);
    RESETBLOCK
    if(change)
    {
      vkdt.graph_dev.runflags = s_graph_run_all;
      dt_image_reset_zoom(&vkdt.wstate.img_widget);
    }
    KEYFRAME
    dt_module_t *m = vkdt.graph_dev.module+modid;
    if(m->so->ui_callback)
    {
      dt_tooltip("automatically crop away black rims");
      if(nk_button_label(ctx, "auto crop"))
      {
        m->so->ui_callback(m, param->name);
        dt_image_reset_zoom(&vkdt.wstate.img_widget);
        vkdt.graph_dev.runflags = s_graph_run_all;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
    }
    else
    {
      dt_tooltip(param->tooltip);
      nk_label(ctx, str, NK_TEXT_LEFT);
    }
    num = count;
  }
  else if(param->widget.type == dt_token("pick"))
  {  // simple aabb for selection, no distortion transform
    int sz = dt_ui_param_size(param->type, 4);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
    if(num == 0) nk_layout_row_dynamic(ctx, row_height, 6);
    dt_tooltip(param->tooltip);
    if(vkdt.wstate.active_widget_modid == modid &&
       vkdt.wstate.active_widget_parid == parid &&
       vkdt.wstate.active_widget_parnm == num)
    {
      snprintf(string, sizeof(string), "done");
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      snprintf(string, sizeof(string), "%02d", num);
      if(nk_button_label(ctx, string))
      {
        widget_end(); // if another one is still in progress, end that now
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = num;
        vkdt.wstate.active_widget_parsz = sz;
        // copy to quad state
        memcpy(vkdt.wstate.state, v, sz);
      }
    }
  }
  else if(param->widget.type == dt_token("ab"))
  { // mouse over image selects split plane
    int sz = dt_ui_param_size(param->type, 1);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
    nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
    if(vkdt.wstate.active_widget_modid == modid &&
       vkdt.wstate.active_widget_parid == parid &&
       vkdt.wstate.active_widget_parnm == num)
    {
      snprintf(string, sizeof(string), "done");
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      if(nk_button_label(ctx, "ab split"))
      {
        widget_end(); // if another one is still in progress, end that now
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = num;
        vkdt.wstate.active_widget_parsz = sz;
        vkdt.wstate.state[0] = v[0];
      }
    }
    dt_tooltip(param->tooltip);
    KEYFRAME
    nk_label(ctx, str, NK_TEXT_LEFT);
  }
  else if(param->widget.type == dt_token("rbmap"))
  { // red/blue chromaticity mapping via src/target coordinates
    if(6*(num / 6) == num) nk_layout_row_dynamic(ctx, row_height, 6);
    int sz = dt_ui_param_size(param->type, 6); // src rgb -> tgt rgb is six floats
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
    nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color((struct nk_color){255*v[0], 255*v[1], 255*v[2], 0xff}));
    nk_style_push_color(ctx, &ctx->style.button.border_color, (struct nk_color){255*v[3], 255*v[4], 255*v[5], 0xff});
    nk_style_push_float(ctx, &ctx->style.button.border, 0.015*pwd);
    if(vkdt.wstate.active_widget_modid == modid &&
       vkdt.wstate.active_widget_parid == parid &&
       vkdt.wstate.active_widget_parnm == num)
    {
      snprintf(string, sizeof(string), "done");
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      snprintf(string, sizeof(string), "%02d", num);
      if(nk_button_label(ctx, string))
      {
        widget_end(); // if another one is still in progress, end that now
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = num;
        vkdt.wstate.active_widget_parsz = 0;
      }
      count *= 6; // keyframe needs to know it's 6 floats too
      KEYFRAME
      count /= 6;
    }
    nk_style_pop_float(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_style_item(ctx);
    if(num == count - 1)
    { // edit specific colour patch below list of patches:
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid)
      { // now add ability to change target colour coordinate
        int active_num = vkdt.wstate.active_widget_parnm;
        nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
        for(int j=0;j<2;j++)
        {
          nk_label(ctx, j ? "destination" : "source", NK_TEXT_LEFT);
          nk_label(ctx, "", 0);
          for(int i=0;i<3;i++)
          {
            float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset + active_num*sz) + 3*j + i;
            float oldval = *val;
            const char *label[] = {"red", "green", "blue"};
            // custom resetblock: set only this colour spot to identity mapping
            if(nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_DOUBLE, nk_widget_bounds(ctx)))
            {
              memcpy(val-i, val-i-3, sizeof(float)*3);
              change = 1;
            }
            nk_tab_property(float, ctx, "#", 0.0, val, 1.0, 0.1, .001);
            if(*val != oldval) change = 1;
            if(change)
            {
              dt_graph_run_t flags = s_graph_run_none;
              if(vkdt.graph_dev.module[modid].so->check_params)
                flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
              vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | flags;
              vkdt.graph_dev.active_module = modid;
              dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
            }
            nk_label(ctx, label[i], NK_TEXT_LEFT);
          }
        }
      }
    }
  }
  else if(param->widget.type == dt_token("grab"))
  {  // grab all input
    if(num == 0)
    {
      nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid)
      {
        if(nk_button_label(ctx, "stop [esc]"))
        {
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_module_input_event_t p = { .type = -1 };
          if(modid >= 0 && mod->so->input) mod->so->input(mod, &p);
          widget_end();
          dt_gui_dr_anim_stop();
        }
      }
      else
      {
        dt_tooltip(param->tooltip);
        if(nk_button_label(ctx, "grab fullscreen"))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.state.anim_no_keyframes = 1; // switch off animation, we will be moving ourselves
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          dt_module_input_event_t p = { 0 };
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_gui_grab_mouse();
          dt_gui_set_fullscreen_view();
          if(modid >= 0)
            if(mod->so->input) mod->so->input(mod, &p);
          dt_gui_dr_anim_start();
        }
        if(nk_button_label(ctx, "grab input"))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.state.anim_no_keyframes = 1; // switch off animation, we will be moving ourselves
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          dt_module_input_event_t p = { 0 };
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_gui_grab_mouse();
          if(modid >= 0)
            if(mod->so->input) mod->so->input(mod, &p);
          dt_gui_dr_anim_start();
        }
      }
    }
  }
  else if(param->widget.type == dt_token("draw"))
  {
    nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    KEYFRAME
    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      snprintf(string, sizeof(string), "done");
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_color(ctx, &ctx->style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
      nk_style_pop_color(ctx);
    }
    else
    {
      snprintf(string, sizeof(string), "%" PRItkn" start", dt_token_str(param->name));
      dt_tooltip("start drawing brush strokes with the mouse\n"
          "shift click start and end: draw straight line\n"
          "scroll - fine tune radius\n"
          "ctrl scroll - fine tune hardness\n"
          "shift scroll - fine tune opacity\n"
          "right click - discard last stroke");
      if(nk_button_label(ctx, string))
      {
        widget_end(); // if another one is still in progress, end that now
        dt_gui_set_lod(vkdt.wstate.lod_interact);
        vkdt.wstate.state[0] = 1.0f; // abuse for radius
        vkdt.wstate.state[1] = 1.0f; // abuse for opacity
        vkdt.wstate.state[2] = 1.0f; // abuse for hardness
        // get base radius from other param
        int pid = dt_module_get_param(vkdt.graph_dev.module[modid].so, dt_token("radius"));
        if(pid >= 0) vkdt.wstate.state[3] = dt_module_param_float(
            vkdt.graph_dev.module+modid, pid)[0];
        else vkdt.wstate.state[3] = 1.0;
        vkdt.wstate.state[4] = vkdt.graph_dev.module[modid].connector[0].roi.wd;
        vkdt.wstate.active_widget_modid = modid;
        vkdt.wstate.active_widget_parid = parid;
        vkdt.wstate.active_widget_parnm = 0;
        // TODO: how to crop this to smaller size in case it's not required?
        vkdt.wstate.active_widget_parsz = dt_ui_param_size(param->type, param->cnt);
        // for sanity also keep mapped_size to make clear that it belongs to the mapping, not the copy
        vkdt.wstate.mapped_size = dt_ui_param_size(param->type, param->cnt);
        vkdt.wstate.mapped = v; // map state
      }
    }
    if(vkdt.wstate.mapped)
    {
      nk_labelf(ctx, NK_TEXT_LEFT, "%d/10000 verts", ((uint32_t *)vkdt.wstate.mapped)[0]);
    }
    num = count;
  }
  else if(param->widget.type == dt_token("filename"))
  {
    if(num == 0)
    { // only show first, cnt refers to allocation length of string param
      nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
      char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
      nk_flags ret = nk_tab_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, v, count, nk_filter_default);
      if(ret & NK_EDIT_COMMITED)
      { // kinda grave change, rerun all, but only if enter pressed
        nk_edit_unfocus(ctx); // make keyboard nav work again
        vkdt.graph_dev.runflags = s_graph_run_all;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      dt_tooltip(param->tooltip);
      nk_label(ctx, str, NK_TEXT_LEFT);
    }
  }
  else if(param->widget.type == dt_token("rgb"))
  {
    float *col = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
    struct nk_color normal = {(200*col[0]+30)/2, (200*col[1]+30)/2, (200*col[2]+30)/2, 0xff};
    struct nk_color hover  = normal;
    struct nk_color active = normal;
    // questionable if separate hover/active makes much sense, it is separate for buttons and edit and bg and shows boxy artifacts
    nk_style_push_style_item(ctx, &ctx->style.property.edit.normal,       nk_style_item_color(normal));
    nk_style_push_style_item(ctx, &ctx->style.property.edit.hover,        nk_style_item_color(hover));
    nk_style_push_style_item(ctx, &ctx->style.property.edit.active,       nk_style_item_color(active));
    nk_style_push_style_item(ctx, &ctx->style.property.inc_button.normal, nk_style_item_color(normal));
    nk_style_push_style_item(ctx, &ctx->style.property.inc_button.hover,  nk_style_item_color(hover));
    nk_style_push_style_item(ctx, &ctx->style.property.inc_button.active, nk_style_item_color(active));
    nk_style_push_style_item(ctx, &ctx->style.property.dec_button.normal, nk_style_item_color(normal));
    nk_style_push_style_item(ctx, &ctx->style.property.dec_button.hover,  nk_style_item_color(hover));
    nk_style_push_style_item(ctx, &ctx->style.property.dec_button.active, nk_style_item_color(active));
    nk_style_push_style_item(ctx, &ctx->style.property.normal, nk_style_item_color(normal));
    nk_style_push_style_item(ctx, &ctx->style.property.hover,  nk_style_item_color(hover));
    nk_style_push_style_item(ctx, &ctx->style.property.active, nk_style_item_color(active));
    nk_layout_row(ctx, NK_STATIC, row_height, 2, wds);
    for(int comp=0;comp<3;comp++)
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num + comp;
      float oldval = *val;
      snprintf(string, sizeof(string), "%" PRItkn " %s",
          dt_token_str(param->name),
          comp == 0 ? "red" : (comp == 1 ? "green" : "blue"));
      RESETBLOCK
      nk_tab_property(float, ctx, "#", param->widget.min, val, param->widget.max, 0.1, .001);
      if(*val != oldval) change = 1;
      if(change)
      {
        if(nk_input_is_key_pressed(&ctx->input, NK_KEY_SHIFT)) // lockstep all three if shift is pressed
          for(int k=3*(comp/3);k<3*(comp/3)+3;k++) val[k-comp] = val[0];
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | flags;
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      dt_tooltip(param->tooltip);
      KEYFRAME
      nk_label(ctx, string, NK_TEXT_LEFT);
    }
    for(int i=0;i<12;i++) nk_style_pop_style_item(ctx);
    if(param->cnt == count && count <= 4) num = 4; // non-array rgb controls
  }
  else if(param->widget.type == dt_token("print"))
  {
    nk_layout_row_dynamic(ctx, row_height, 1);
    float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    nk_labelf(ctx, NK_TEXT_LEFT, "%g | %g | %g   %" PRItkn, val[0], val[1], val[2], dt_token_str(param->name));
    num = count; // we've done it all at once
  }
  } // end for multiple widgets
#undef RESETBLOCK
#undef KEYFRAME
}

static inline void render_darkroom_widgets(
    dt_graph_t *graph,          // graph with the modules and parameters
    int         curr)           // which module id to draw
{
  char name[30];
  dt_module_t *const arr = graph->module;
  if(!arr[curr].so->num_params) return;
  int active = graph->active_module == curr;
  if(graph->active_module >= 0 && !active) return;

  struct nk_context *ctx = &vkdt.ctx;
  const float pwd = vkdt.state.panel_wd - ctx->style.window.scrollbar_size.x - 2*ctx->style.window.padding.x;
  const float w3[] = {
    0.06f*pwd,
    0.88f*pwd-2*ctx->style.window.spacing.x,
    0.06f*pwd};
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn,
      dt_token_str(arr[curr].name), dt_token_str(arr[curr].inst));
  dt_module_t *module = graph->module+curr;
  nk_layout_row(ctx, NK_STATIC, row_height, 3, w3);

  nk_uint offx, offy;
  nk_window_get_scroll(ctx, &offx, &offy);
  struct nk_rect bound = nk_layout_widget_bounds(ctx);
  bound.y -= offy; // account for scrolling: both drawing and mouse events are not relative to scroll window, widget bounds are
  nk_fill_rect(nk_window_get_canvas(ctx), bound, 0.0, vkdt.style.colour[NK_COLOR_DT_BACKGROUND]);
  bound.h -= ctx->style.tab.padding.y;
  nk_fill_rect(nk_window_get_canvas(ctx), bound, 0.0, ctx->style.tab.background.data.color);
  bound.x += w3[0]; // mouse click: not the disable button

  if(module->so->has_inout_chain)
  {
    dt_tooltip(module->disabled ? "re-enable this module" :
        "temporarily disable this module without disconnecting it from the graph. "
        "this is just a convenience A/B switch in the ui and will not affect your "
        "processing history, lighttable thumbnail, or export.");
    struct nk_rect box = nk_widget_bounds(ctx);
    ((struct nk_user_font *)ctx->style.font)->height = vkdt.style.fontsize * 0.9;
    nk_label(ctx, module->disabled ? "\ue612" : "\ue836", NK_TEXT_CENTERED);
    ((struct nk_user_font *)ctx->style.font)->height = vkdt.style.fontsize;
    // bit of a crazy dance to avoid double accounting for clicks on combo boxes that just closed above us:
    const struct nk_input *in = (ctx->current->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
    if(in && nk_input_is_mouse_hovering_rect(in, box) && 
        nk_input_has_mouse_click_in_button_rect(in, NK_BUTTON_LEFT, box) &&
        nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT))
    {
      int bad = 0;
      for(int c=0;c<module->num_connectors;c++)
        if(module->connector[c].flags & s_conn_feedback) bad = 1;
      if(bad)
      {
        dt_gui_notification("cannot disable a module with feedback connectors!");
      }
      else
      {
        module->disabled ^= 1;
        int cid = dt_module_get_connector(arr+curr, dt_token("dspy"));
        if(cid >= 0)
        { // (dis)connect dspy
          int mid = dt_module_get(graph, dt_token("display"), dt_token("dspy"));
          if(module->disabled) dt_module_connect(graph, -1, -1, mid, 0);
          else dt_module_connect(graph, curr, cid, mid, 0); // reconnect
        }
        vkdt.graph_dev.runflags = s_graph_run_all;
      }
    }
  }
  else
  {
    dt_tooltip("this module cannot be disabled automatically because\n"
               "it does not implement a simple input -> output chain");
    ((struct nk_user_font *)ctx->style.font)->height = vkdt.style.fontsize * 0.9;
    nk_label(ctx, "\ue15b", NK_TEXT_CENTERED);
    ((struct nk_user_font *)ctx->style.font)->height = vkdt.style.fontsize;
  }
  // nk_label(ctx, active ? "\ue5cf" : "\ue5cc", NK_TEXT_CENTERED);
  // nk_label(ctx, active ? "\ue5e0" : "\ue5e1", NK_TEXT_CENTERED);
  // nk_label(ctx, active ? "\ue5c5" : "\ue5df", NK_TEXT_CENTERED);
  nk_label(ctx, name, NK_TEXT_LEFT);
  ((struct nk_user_font *)ctx->style.font)->height = vkdt.style.fontsize * 0.9;
  nk_label(ctx, active ? "\ue5e0" : "\ue5e1", NK_TEXT_CENTERED);
  ((struct nk_user_font *)ctx->style.font)->height = vkdt.style.fontsize;
  // bit of a crazy dance to avoid double accounting for clicks on combo boxes that just closed above us:
  const struct nk_input *in = (vkdt.ctx.current->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  if(in && nk_input_is_mouse_hovering_rect(in, bound) && 
      nk_input_has_mouse_click_in_button_rect(in, NK_BUTTON_LEFT, bound) &&
      nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT))
  {
    if(!active)
    { // just opened, now this is the 'active module'.
      graph->active_module = curr;
      int cid = dt_module_get_connector(arr+curr, dt_token("dspy"));
      if(cid >= 0)
      { // if 'dspy' output exists, connect to 'dspy' display
        if(graph->module[curr].disabled)
        { // if the module is disabled, disconnect
          CONN(dt_module_connect(graph, curr, cid, -1, -1));
          vkdt.graph_dev.runflags = s_graph_run_all;
        }
        else
        {
          int mid = dt_module_add(graph, dt_token("display"), dt_token("dspy"));
          if(graph->module[mid].connector[0].connected.i != curr ||
             graph->module[mid].connector[0].connected.c != cid)
          { // only if not connected yet, don't record in history
            CONN(dt_module_connect(graph, curr, cid, mid, 0));
            vkdt.graph_dev.runflags = s_graph_run_all;
          }
          if(mid >= 0)
          { // scale output to match panel
            const float pwd = vkdt.style.panel_width_frac * vkdt.win.width;
            graph->module[mid].connector[0].max_wd = pwd;
            graph->module[mid].connector[0].max_ht = pwd;
            vkdt.graph_dev.runflags = s_graph_run_all;
          }
        }
      }
      active = 1;
    }
    else
    {
      active = 0;
      graph->active_module = -1;
    }
  }
  if(active)
  {
    const int display_frame = vkdt.graph_dev.double_buffer % 2;
    if(graph->active_module == curr &&
        dt_module_get_connector(arr+curr, dt_token("dspy")) >= 0)
    {
      dt_node_t *out_dspy = dt_graph_get_display(graph, dt_token("dspy"));
      if(out_dspy && vkdt.graph_res[display_frame] == VK_SUCCESS)
      {
        struct nk_rect row = nk_layout_widget_bounds(ctx);
        float iwd = out_dspy->connector[0].roi.wd;
        float iht = out_dspy->connector[0].roi.ht;
        float scale = MIN(
            MIN(out_dspy->connector[0].roi.wd, row.w) / iwd,
            MIN(out_dspy->connector[0].roi.ht, row.w) / iht);
        int ht = scale * iht, wd = scale * iwd;
        float r = wd / (float)row.w;
        nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0,0));
        float w3[] = {
          0.5*(1-r)*pwd-ctx->style.window.spacing.x,
          r*pwd-ctx->style.window.spacing.x,
          0.5*(1-r)*pwd};
        nk_layout_row(ctx, NK_STATIC, ht, 3, w3);
        nk_label(ctx, "", 0);
        vkdt.wstate.active_dspy_bound = nk_widget_bounds(ctx);
        struct nk_image img = nk_image_ptr(out_dspy->dset[display_frame]);
        nk_image(ctx, img);
        nk_label(ctx, "", 0);
        nk_style_pop_vec2(ctx);
      }
    }
    for(int i=0;i<arr[curr].so->num_params;i++)
      render_darkroom_widget(curr, i, 0);
    if(vkdt.view_mode == s_view_darkroom)
    { // add a bit of extra panel space to be able to scroll down:
      nk_layout_row_dynamic(ctx, 10*row_height, 1);
      nk_label(ctx, "", 0);
    }
  }
}

static inline uint32_t
render_darkroom_apply_preset(
    const char *presetname) // name of the preset.pst, directory to be reconstructed
{
  char filename[PATH_MAX];
  snprintf(filename, sizeof(filename), "presets/%s", presetname);
  FILE *f = dt_graph_open_resource(0, 0, filename, "rb");
  uint32_t lno = -1u;
  if(f)
  {
    char line[300000];
    while(!feof(f))
    {
      fscanf(f, "%299999[^\n]", line);
      if(fgetc(f) == EOF) break; // read \n
      lno++;
      // > 0 are warnings, < 0 are fatal, 0 is success
      if(dt_graph_read_config_line(&vkdt.graph_dev, line) < 0) goto error;
      dt_graph_history_line(&vkdt.graph_dev, line);
    }
    fclose(f);
    for(int m=0;m<vkdt.graph_dev.num_modules;m++) dt_module_keyframe_post_update(vkdt.graph_dev.module+m);
    vkdt.graph_dev.runflags = s_graph_run_all;
    dt_image_reset_zoom(&vkdt.wstate.img_widget);
    return 0u;
  }
  else
  {
error:
    if(f) fclose(f);
    return lno;
  }
}

static inline void
render_darkroom_modals()
{
  struct nk_rect bounds = { vkdt.state.center_x+0.2*vkdt.state.center_wd, vkdt.state.center_y+0.2*vkdt.state.center_ht,
    0.6*vkdt.state.center_wd, 0.6*vkdt.state.center_ht };
  if(vkdt.wstate.popup == s_popup_assign_tag)
  {
    if(nk_begin(&vkdt.ctx, "assign tag", bounds, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
      static char filter[256] = "all time best";
      static char name[PATH_MAX];
      int ok = filteredlist(0, "tags", filter, name, sizeof(name), s_filteredlist_allow_new | s_filteredlist_return_short);
      if(ok) vkdt.wstate.popup = 0;
      if(ok == 1)
      {
        dt_db_add_to_collection(&vkdt.db, vkdt.db.current_imgid, name);
        dt_gui_read_tags();
      }
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
  else if(vkdt.wstate.popup == s_popup_add_module)
  {
    if(nk_begin(&vkdt.ctx, "add module", bounds, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
      static char mod_inst[10] = "01";
      const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
      nk_layout_row_dynamic(&vkdt.ctx, row_height, 2);
      nk_flags ret = nk_tab_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, mod_inst, 9, nk_filter_default);
      nk_label(&vkdt.ctx, "instance name", NK_TEXT_LEFT);
      char filename[1024] = {0};
      static char filter[256];
      if(ret & NK_EDIT_COMMITED) vkdt.wstate.popup_appearing = 1; // re-focus, will trigger enter => accept
      int ok = filteredlist("modules", 0, filter, filename, sizeof(filename), s_filteredlist_descr_req | s_filteredlist_return_short);
      if(ok) vkdt.wstate.popup = 0;
      if(ok == 1)
      {
        int new_modid = dt_module_add(&vkdt.graph_dev, dt_token(filename), dt_token(mod_inst));
        if(new_modid >= 0) dt_graph_history_module(&vkdt.graph_dev, new_modid);
      }
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
  else if(vkdt.wstate.popup == s_popup_create_preset)
  {
    if(nk_begin(&vkdt.ctx, "create preset", bounds, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
      struct nk_rect total_space = nk_window_get_content_region(&vkdt.ctx);
      static char  preset[32] = "default";
      static char  filter[32] = "";
      static int   line_cnt = 0;
      static char *line[1024] = {0};
      static char  sel[1024];
      static char *buf = 0;
      static int   ok = 0;
      if(vkdt.wstate.popup_appearing && buf)
      { // free any potential leftovers from last time
        free(buf);
        buf = 0;
      }
      if(!buf)
      {
        ok = 0;
        char filename[1024] = {0};
        uint32_t cid = dt_db_current_imgid(&vkdt.db);
        if(cid != -1u) dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
        if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
          dt_graph_write_config_ascii(&vkdt.graph_dev, filename);
        FILE *f = fopen(filename, "rb");
        size_t s = 0;
        if(f)
        {
          fseek(f, 0, SEEK_END);
          s = ftell(f);
          fseek(f, 0, SEEK_SET);
          buf = (char *)malloc(s);
          fread(buf, s, 1, f);
          fclose(f);
        }
        line_cnt = 1;
        line[0] = buf;
        for(uint32_t i=0;i<s-1 && line_cnt < 1024;i++) if(buf[i] == '\n')
        {
          line[line_cnt++] = buf+i+1;
          buf[i] = 0;
        }
        for(int i=0;i<line_cnt;i++)
        { // mark all lines as selected initially, only ones related to input modules not
          sel[i] = strstr(line[i], "param:i-") == 0 ? 1 : 0;
        }
        if(buf[s-1] == '\n') buf[s-1] = 0;
      }
    
      const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
      if(ok == 0)
      {
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 4);
        nk_label(&vkdt.ctx, "filter", NK_TEXT_LEFT);
        dt_tooltip(
            "type to filter the list\n"
            "press enter to apply top item\n"
            "press escape to close");
        if(vkdt.wstate.popup_appearing) nk_edit_focus(&vkdt.ctx, 0);
        nk_flags ret = nk_tab_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, filter, 256, nk_filter_default);
        if(ret & NK_EDIT_COMMITED) ok = 1;
        vkdt.wstate.popup_appearing = 0;

        int select_all = 0;
        if(nk_button_label(&vkdt.ctx, "select all shown")) select_all = 1;
        if(nk_button_label(&vkdt.ctx, "deselect shown"))   select_all = 2;

        nk_layout_row_dynamic(&vkdt.ctx, total_space.h-3*row_height, 1);
        nk_group_begin(&vkdt.ctx, "new-preset-scrollpane", 0);
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
        for(int i=0;i<line_cnt;i++)
        {
          if(filter[0] == 0 || strstr(line[i], filter))
          {
            if(select_all == 1) sel[i] = 1;
            if(select_all == 2) sel[i] = 0;
            const int selected = sel[i];
            if(selected)
            {
              nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
              nk_style_push_color(&vkdt.ctx, &vkdt.ctx.style.button.text_normal, vkdt.style.colour[NK_COLOR_DT_ACCENT_TEXT]);
            }
            dt_tooltip(selected ? "click to drop from preset" : "click to include in preset");
            if(nk_button_label(&vkdt.ctx, line[i])) sel[i] ^= 1;
            if(selected) nk_style_pop_style_item(&vkdt.ctx);
            if(selected) nk_style_pop_color(&vkdt.ctx);
          }
        }
        nk_group_end(&vkdt.ctx);

        nk_layout_row_dynamic(&vkdt.ctx, row_height, 5);
        nk_label(&vkdt.ctx, "", NK_TEXT_LEFT);
        nk_label(&vkdt.ctx, "", NK_TEXT_LEFT);
        nk_label(&vkdt.ctx, "", NK_TEXT_LEFT);
        if (nk_button_label(&vkdt.ctx, "cancel")) ok = 3;
        if (nk_button_label(&vkdt.ctx, "ok"))     ok = 1;
      }
      else if(ok == 1)
      {
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 2);
        nk_label(&vkdt.ctx, "enter preset name", NK_TEXT_LEFT);
        dt_tooltip("presets will be stored as\n"
            "~/.config/vkdt/presets/<this>.pst");
        nk_edit_focus(&vkdt.ctx, 0); // ???
        nk_flags ret = nk_tab_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, preset, NK_LEN(preset), nk_filter_default);
        if(ret & NK_EDIT_COMMITED) ok = 2;
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 5);
        nk_label(&vkdt.ctx, "", 0);
        nk_label(&vkdt.ctx, "", 0);
        nk_label(&vkdt.ctx, "", 0);
        if (nk_button_label(&vkdt.ctx, "cancel")) ok = 3;
        if (nk_button_label(&vkdt.ctx, "ok"))     ok = 2;
      }

      if(ok == 2)
      {
        char filename[PATH_MAX+100];
        snprintf(filename, sizeof(filename), "%s/presets", dt_pipe.homedir);
        fs_mkdir_p(filename, 0755);
        snprintf(filename, sizeof(filename), "%s/presets/%s.pst", dt_pipe.homedir, preset);
        FILE *f = fopen(filename, "wb");
        if(f)
        {
          for(int i=0;i<line_cnt;i++)
            if(sel[i]) fprintf(f, "%s\n", line[i]);
          fclose(f);
        }
        else dt_gui_notification("failed to write %s!", filename);
      }

      if(ok > 1)
      {
        free(buf);
        buf = 0;
        line_cnt = 0;
        ok = 0;
        vkdt.wstate.popup = 0;
      }
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
  else if(vkdt.wstate.popup == s_popup_apply_preset)
  {
    if(nk_begin(&vkdt.ctx, "apply preset", bounds, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
      char filename[1024] = {0};
      uint32_t cid = dt_db_current_imgid(&vkdt.db);
      if(cid != -1u) dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
      if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
        dt_graph_write_config_ascii(&vkdt.graph_dev, filename);
      static char filter[256];
      int ok = filteredlist("presets", "presets", filter, filename, sizeof(filename), s_filteredlist_return_short);
      if(ok) vkdt.wstate.popup = 0;
      if(ok == 1)
      {
        uint32_t err_lno = render_darkroom_apply_preset(filename);
        if(err_lno)
          dt_gui_notification("failed to read %s line %u", filename, err_lno);
      } // end if ok == 1
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
}
