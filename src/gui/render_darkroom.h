#pragma once
// some drawing routines shared between node editor and darkroom mode
static inline void widget_end()
{
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
  nk_draw_text(buf, (struct nk_rect){px, py, sx,sy}, overlay, strlen(overlay), &dt_gui_get_font(2)->handle, (struct nk_color){0x77,0x77,0x77,0xff}, col);
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
    if(dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.grpid)[0] != param->widget.mode)
      return;

#if 0 // TODO port
  int axes_cnt = 0;
  const float *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt) : 0;
  static int gamepad_reset = 0;
  if(ImGui::IsKeyPressed(ImGuiKey_GamepadR3)) gamepad_reset = 1;
#endif
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
    nk_contextual_end(ctx);\
  }}

#ifndef KEYFRAME // enable node graph editor to switch this off
  // common code block to insert a keyframe. currently only supports float (for interpolation)
#define KEYFRAME\
  if(nk_widget_is_hovered(ctx))\
  {\
    if(gui.hotkey == s_hotkey_insert_keyframe)\
    {\
      dt_graph_t *g = &vkdt.graph_dev;\
      uint32_t ki = -1u;\
      for(uint32_t i=0;ki<0&&i<g->module[modid].keyframe_cnt;i++)\
        if(g->module[modid].keyframe[i].param == param->name && \
           g->module[modid].keyframe[i].frame == g->frame)\
          ki = i;\
      if(ki == -1u)\
      {\
        ki = g->module[modid].keyframe_cnt++;\
        g->module[modid].keyframe = (dt_keyframe_t *)dt_realloc(g->module[modid].keyframe, &g->module[modid].keyframe_size, sizeof(dt_keyframe_t)*(ki+1));\
        g->module[modid].keyframe[ki].beg   = 0;\
        g->module[modid].keyframe[ki].end   = count;\
        g->module[modid].keyframe[ki].frame = g->frame;\
        g->module[modid].keyframe[ki].param = param->name;\
        g->module[modid].keyframe[ki].data  = g->params_pool + g->params_end;\
        g->params_end += dt_ui_param_size(param->type, count);\
        assert(g->params_end <= g->params_max);\
      }\
      memcpy(g->module[modid].keyframe[ki].data, g->module[modid].param + param->offset, dt_ui_param_size(param->type, count));\
      dt_module_keyframe_post_update(g->module+modid);\
      dt_gui_notification("added keyframe for frame %u %" PRItkn ":%" PRItkn ":%" PRItkn, \
          g->frame, dt_token_str(g->module[modid].name), dt_token_str(g->module[modid].inst), dt_token_str(param->name));\
      dt_graph_history_keyframe(&vkdt.graph_dev, modid, ki);\
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
  for(int num=0;num<count;num++)
  {
  const float ratio[] = {0.7f, 0.3f};
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  char string[256];
  // const float halfw = (0.66*vkdt.state.panel_wd - ctx.style.tab.padding.x)/2;
  char str[10] = { 0 };
  memcpy(str, &param->name, 8);
  // distinguish by type:
  if(param->widget.type == dt_token("slider"))
  {
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
    if(param->type == dt_token("float"))
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      float oldval = *val;
      RESETBLOCK
      struct nk_rect bounds = nk_widget_bounds(ctx);
      nk_property_float(ctx, "#", param->widget.min, val, param->widget.max,
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
      nk_property_int(ctx, "#", param->widget.min, val, param->widget.max,
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
  else if(param->widget.type == dt_token("hsvknobs"))
  { // only works for param->type == float and count == 3
    if((num % 3) == 0)
    {
      float r7[] = {0.1666*ratio[0], 0.1666*ratio[0], 0.1666*ratio[0], 0.1666*ratio[0], 0.1666*ratio[0], 0.1666*ratio[0], ratio[1] };
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 7, r7);
      struct nk_colorf *val = (struct nk_colorf *)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      struct nk_colorf oldval = *val;
      struct nk_command_buffer *cmd = &vkdt.global_cmd;
      const float dead_angle = 60.0f;
      nk_label(ctx, "h", NK_TEXT_RIGHT);
      nk_style_push_color(ctx, &ctx->style.knob.knob_active, nk_hsv_f(val->r, 1.0, 1.0));
      struct nk_rect bounds = nk_widget_bounds(ctx);
      nk_knob_float(ctx, 0.0, &val->r, 1.0, 1.0/100.0, NK_DOWN, dead_angle); // H
      nk_style_pop_color(ctx);

#define DECORATE(VAL, COL) do {\
      if((!ctx->input.mouse.buttons[NK_BUTTON_LEFT].down && (nk_input_is_mouse_hovering_rect(&ctx->input, bounds)) || \
         ( ctx->input.mouse.buttons[NK_BUTTON_LEFT].down && (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED))))\
      {\
        const float c[] = { bounds.x + bounds.w/2.0, bounds.y + bounds.h/2.0 };\
        int N = 40;\
        float phi = (3.0f/2.0f*M_PI-dead_angle/2.0f*M_PI/180.0f), delta_phi = (2.0f*M_PI - dead_angle*M_PI/180.0f)/N,\
              r0 = 0.4*vkdt.state.panel_wd, r1 = 0.5*vkdt.state.panel_wd;\
        for(int k=0;k<=N;k++)\
        {\
          if(k==N) {\
            phi = (3.0f/2.0f*M_PI-(dead_angle/2.0f-0.5f+VAL*(360.0f-dead_angle))*M_PI/180.0f);\
            delta_phi = M_PI/180.0f;\
          }\
          float x[] = {\
            c[0] + cosf(phi) * r0,           c[1] - sinf(phi) * r0,\
            c[0] + cosf(phi-delta_phi) * r0, c[1] - sinf(phi-delta_phi) * r0,\
            c[0] + cosf(phi-delta_phi) * r1, c[1] - sinf(phi-delta_phi) * r1,\
            c[0] + cosf(phi) * r1,           c[1] - sinf(phi) * r1};\
          nk_fill_polygon(cmd, x, 4, k==N ? nk_rgb(0,0,0) : COL);\
          phi -= delta_phi;\
        }\
      }} while(0)

      DECORATE(val->r, nk_hsv_f((k+0.5)/N, 1.0, 1.0));

      nk_label(ctx, "s", NK_TEXT_RIGHT);
      nk_style_push_color(ctx, &ctx->style.knob.knob_active, nk_hsv_f(val->r, val->g, 1.0));
      bounds = nk_widget_bounds(ctx);
      nk_knob_float(ctx, 0.0, &val->g, 1.0, 1.0/100.0, NK_DOWN, 60.0f); // S
      nk_style_pop_color(ctx);
      DECORATE(val->g, nk_hsv_f(val->r, (k+0.5)/N, 1.0));
      nk_label(ctx, "v", NK_TEXT_RIGHT);
      bounds = nk_widget_bounds(ctx);
      nk_knob_float(ctx, 0.0, &val->b, 2.0, 1.0/100.0, NK_DOWN, 60.0f); // V
      DECORATE(val->b/2.0, nk_hsv_f(val->r, val->g, (k+0.5)/N));

#undef DECORATE
      if(memcmp(val, &oldval, sizeof(float)*3)) change = 1;
      dt_tooltip(param->tooltip);
      KEYFRAME
      RESETBLOCK
      nk_label(ctx, str, NK_TEXT_LEFT);
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
  }
  else if(param->widget.type == dt_token("coledit"))
  { // only works for param->type == float and count == 3
    if((num % 3) == 0)
    {
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      struct nk_colorf *val = (struct nk_colorf *)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      struct nk_colorf oldval = *val;
      float size = nk_widget_width(ctx);
      RESETBLOCK
      if(nk_combo_begin_color(ctx, nk_rgb_cf(*val), nk_vec2(vkdt.state.panel_wd, size+4.0*row_height)))
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
      nk_layout_row_static(ctx, row_height, vkdt.state.panel_wd/9, 8);
      for(int k=0;k<32;k++)
      {
        const int sel = val[0] & (1<<k);
        if(sel) nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
        char label[10];
        snprintf(label, sizeof(label), "%d", k);
        dt_tooltip(c);
        if(nk_button_label(ctx, label)) { val[0] ^= (1<<k); change = 1; }
        if(sel) nk_style_pop_style_item(ctx);

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
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
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
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, v, count, nk_filter_default);
      }
      else nk_label(ctx, "", NK_TEXT_LEFT);
    }
  }
  else if(param->widget.type == dt_token("combo"))
  { // combo box
    if(param->type == dt_token("int"))
    {
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      int32_t oldval = *val;
      RESETBLOCK
      struct nk_vec2 size = { ratio[0]*vkdt.state.panel_wd, ratio[0]*vkdt.state.panel_wd };
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
  else if(param->widget.type == dt_token("colour"))
  {
    float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
    snprintf(string, sizeof(string), "%" PRItkn " %d", dt_token_str(param->name), num);
    if(num == 0) nk_layout_row_dynamic(ctx, row_height, 6);
    struct nk_color col = {val[0]*255, val[1]*255, val[2]*255, 0xff };
    dt_tooltip(param->tooltip);
    nk_button_color(ctx, col);
  }
  else if(param->widget.type == dt_token("pers"))
  {
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
    const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
    const float aspect = iwd/iht;
    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      int accept = 0;
#if 0 // TODO: port gamepad interaction
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
      {
        vkdt.wstate.selected ++;
        if(vkdt.wstate.selected == 4) vkdt.wstate.selected = 0;
      }
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))
        accept = 1;
      const float scale = vkdt.wstate.img_widget.scale > 0.0f ? vkdt.wstate.img_widget.scale : 1.0f;
      if(vkdt.wstate.selected >= 0 && axes)
      {
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
        float inc[2] = {
            0.002f/scale * SMOOTH(axes[3]),
            0.002f/scale * SMOOTH(axes[4])};
#undef SMOOTH
        dt_gui_dr_pers_adjust(inc, 1);
      }
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))
      {
        dt_gamepadhelp_pop();
        dt_module_set_param_float(vkdt.graph_dev.module+modid, dt_token("rotate"), vkdt.wstate.state[9]);
        widget_abort();
      }
#endif
      snprintf(string, sizeof(string), "%" PRItkn" done", dt_token_str(param->name));
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      if(nk_button_label(ctx, string) || accept)
      {
        // dt_gamepadhelp_pop();
        dt_module_set_param_float(vkdt.graph_dev.module+modid, dt_token("rotate"), vkdt.wstate.state[9]);
        dt_module_set_param_float_n(vkdt.graph_dev.module+modid, dt_token("crop"), vkdt.wstate.state+10, 4);
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
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
        w->scale = 0.9 * MIN(w->win_w/w->wd, w->win_h/w->ht);
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
    const float rat3[] = {0.3f, 0.4f, 0.3f};
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 3, rat3);

    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      if(nk_button_label(ctx, "done"))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
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
    nk_property_float(ctx, "#", -360.0f, val, 360.0f, 
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
    const float rat3[] = {0.3f, 0.4f, 0.3f};
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 3, rat3);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
    const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
    const float aspect = iwd/iht;
    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      int accept = 0;
#if 0 // TODO port gamepad controls
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
      {
        vkdt.wstate.selected ++;
        if(vkdt.wstate.selected == 4) vkdt.wstate.selected = 0;
      }
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))
        accept = 1;

      int axes_cnt = 0;
      const float* axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt) : 0;
      const float scale = vkdt.wstate.img_widget.scale > 0.0f ? vkdt.wstate.img_widget.scale : 1.0f;
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
      if(vkdt.wstate.selected >= 0 && axes)
        dt_gui_dr_crop_adjust(0.002f/scale * SMOOTH(axes[vkdt.wstate.selected < 2 ? 3 : 4]), 1);
#undef SMOOTH
      // TODO: at least make keyboard interaction work
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
         ImGui::IsKeyPressed(ImGuiKey_Escape)||
         ImGui::IsKeyPressed(ImGuiKey_CapsLock))
      {
        widget_abort();
        dt_gamepadhelp_pop();
        dt_image_reset_zoom(&vkdt.wstate.img_widget);
      }
#endif
      snprintf(string, sizeof(string), "%" PRItkn" done", dt_token_str(param->name));
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
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
        w->scale = 0.9 * MIN(w->win_w/w->wd, w->win_h/w->ht);
      }
    }
    nk_property_float(ctx, "#aspect", 0.0, &vkdt.wstate.aspect, 10.0, 0.1, .001);
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
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
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
  else if(param->widget.type == dt_token("rbmap"))
  { // red/blue chromaticity mapping via src/target coordinates
    if(6*(num / 6) == num) nk_layout_row_dynamic(ctx, row_height, 6);
    int sz = dt_ui_param_size(param->type, 6); // src rgb -> tgt rgb is six floats
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
    nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color((struct nk_color){255*v[0], 255*v[1], 255*v[2], 0xff}));
    nk_style_push_color(ctx, &ctx->style.button.border_color, (struct nk_color){255*v[3], 255*v[4], 255*v[5], 0xff});
    nk_style_push_float(ctx, &ctx->style.button.border, 0.015*vkdt.state.panel_wd);
    if(vkdt.wstate.active_widget_modid == modid &&
       vkdt.wstate.active_widget_parid == parid &&
       vkdt.wstate.active_widget_parnm == num)
    {
      snprintf(string, sizeof(string), "done");
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
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
        nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
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
            nk_property_float(ctx, "#", 0.0, val, 1.0, 0.1, .001);
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
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid)
      {
        if(nk_button_label(ctx, "stop [esc]"))
        {
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_module_input_event_t p = { .type = -1 };
          if(modid >= 0 && mod->so->input) mod->so->input(mod, &p);
          widget_end();
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
          dt_gui_dr_set_fullscreen_view();
          if(modid >= 0)
            if(mod->so->input) mod->so->input(mod, &p);
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
        }
      }
    }
  }
  else if(param->widget.type == dt_token("draw"))
  {
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
    float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
    if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
    {
      snprintf(string, sizeof(string), "done");
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      if(nk_button_label(ctx, string))
      {
        widget_end();
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      nk_style_pop_style_item(ctx);
    }
    else
    {
      snprintf(string, sizeof(string), "%" PRItkn" start", dt_token_str(param->name));
      dt_tooltip("start drawing brush strokes with the mouse\n"
          "scroll - fine tune radius\n"
          "ctrl scroll - fine tune hardness\n"
          "shift scroll - fine tune opacity\n"
          "right click - discard last stroke");
      if(nk_button_label(ctx, string))
      {
        widget_end(); // if another one is still in progress, end that now
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
      KEYFRAME
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
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
      nk_flags ret = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, v, count, nk_filter_default);
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
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
    for(int comp=0;comp<3;comp++)
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num + comp;
      float oldval = *val;
      snprintf(string, sizeof(string), "%" PRItkn " %s",
          dt_token_str(param->name),
          comp == 0 ? "red" : (comp == 1 ? "green" : "blue"));
      RESETBLOCK
      nk_property_float(ctx, "#", param->widget.min, val, param->widget.max, 0.1, .001);
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
  const float ratio[] = {0.06f, 0.06f, 0.88f};
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn,
      dt_token_str(arr[curr].name), dt_token_str(arr[curr].inst));
  dt_module_t *module = graph->module+curr;
  nk_layout_row(ctx, NK_DYNAMIC, row_height, 3, ratio);

  nk_uint offx, offy;
  nk_window_get_scroll(ctx, &offx, &offy);
  struct nk_rect bound = nk_layout_widget_bounds(ctx);
  bound.y -= offy; // account for scrolling: both drawing and mouse events are not relative to scroll window, widget bounds are
  bound.h -= 2; // leave 2px padding
  nk_fill_rect(nk_window_get_canvas(ctx), bound, 0.0, ctx->style.tab.background.data.color);
  bound.x += ratio[0] * vkdt.state.panel_wd; // mouse click: not the disable button

  if(module->so->has_inout_chain)
  {
    dt_tooltip(module->disabled ? "re-enable this module" :
        "temporarily disable this module without disconnecting it from the graph.\n"
        "this is just a convenience A/B switch in the ui and will not affect your\n"
        "processing history, lighttable thumbnail, or export.");
    nk_style_push_font(ctx, &dt_gui_get_font(3)->handle);
    struct nk_rect box = nk_widget_bounds(ctx);
    nk_label(ctx, module->disabled ? "\ue612" : "\ue836", NK_TEXT_CENTERED);
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
    nk_style_push_font(ctx, &dt_gui_get_font(3)->handle);
    nk_label(ctx, "\ue15b", NK_TEXT_CENTERED);
  }
  nk_label(ctx, active ? "\ue5cf" : "\ue5cc", NK_TEXT_CENTERED);
  nk_style_pop_font(ctx);
  nk_label(ctx, name, NK_TEXT_LEFT);
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
        int mid = dt_module_add(graph, dt_token("display"), dt_token("dspy"));
        if(graph->module[mid].connector[0].connected_mi != curr ||
           graph->module[mid].connector[0].connected_mc != cid)
        { // only if not connected yet, don't record in history
          CONN(dt_module_connect(graph, curr, cid, mid, 0));
          vkdt.graph_dev.runflags = s_graph_run_all;
        }
        if(mid >= 0)
        { // scale output to match panel
          const float pwf = 0.2; // probably make a config param so we have access too (not only the gui)
          const float pwd = pwf * (16.0/9.0) * vkdt.win.height;
          graph->module[mid].connector[0].max_wd = pwd;
          graph->module[mid].connector[0].max_ht = (2.0/3.2) * pwd;
          vkdt.graph_dev.runflags = s_graph_run_all;
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
    if(graph->active_module == curr &&
        dt_module_get_connector(arr+curr, dt_token("dspy")) >= 0)
    {
      dt_node_t *out_dspy = dt_graph_get_display(graph, dt_token("dspy"));
      if(out_dspy && vkdt.graph_res == VK_SUCCESS)
      {
        const int display_frame = vkdt.graph_dev.double_buffer % 2;
        struct nk_rect row = nk_layout_widget_bounds(ctx);
        float iwd = out_dspy->connector[0].roi.wd;
        float iht = out_dspy->connector[0].roi.ht;
        float scale = MIN(
            MIN(out_dspy->connector[0].roi.wd, row.w) / iwd,
            MIN(out_dspy->connector[0].roi.ht, 2.0f/3.0f*row.w) / iht);
        int ht = scale * iht, wd = scale * iwd;
        float r = wd / (float)row.w;
        float ratio[] = {0.5*(1-r) , r, 0.5*(1-r)};
        nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0,0));
        nk_layout_row(ctx, NK_DYNAMIC, ht, 3, ratio);
        nk_label(ctx, "", 0);
        struct nk_image img = nk_image_ptr(out_dspy->dset[display_frame]);
        nk_image(ctx, img);
        nk_label(ctx, "", 0);
        nk_style_pop_vec2(ctx);
      }
    }
    for(int i=0;i<arr[curr].so->num_params;i++)
      render_darkroom_widget(curr, i, 0);
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
      int ok = filteredlist(0, "%s/tags", filter, name, sizeof(name), s_filteredlist_allow_new | s_filteredlist_return_short);
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
      nk_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, mod_inst, 8, nk_filter_default);
      nk_label(&vkdt.ctx, "instance name", NK_TEXT_LEFT);
      char filename[1024] = {0};
      static char filter[256];
      int ok = filteredlist("%s/modules", 0, filter, filename, sizeof(filename), s_filteredlist_descr_req | s_filteredlist_return_short);
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
        nk_flags ret = nk_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, filter, 256, nk_filter_default);
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
              nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
            dt_tooltip(selected ? "click to drop from preset" : "click to include in preset");
            if(nk_button_label(&vkdt.ctx, line[i])) sel[i] ^= 1;
            if(selected) nk_style_pop_style_item(&vkdt.ctx);
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
        nk_flags ret = nk_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, preset, NK_LEN(preset), nk_filter_default);
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
        snprintf(filename, sizeof(filename), "%s/presets", vkdt.db.basedir);
        fs_mkdir_p(filename, 0755);
        snprintf(filename, sizeof(filename), "%s/presets/%s.pst", vkdt.db.basedir, preset);
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
      int ok = filteredlist("%s/data/presets", "%s/presets", filter, filename, sizeof(filename), s_filteredlist_default);
      if(ok) vkdt.wstate.popup = 0;
      if(ok == 1)
      {
        FILE *f = fopen(filename, "rb");
        uint32_t lno = 0;
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
        }
        else
        {
  error:
          if(f) fclose(f);
          dt_gui_notification("failed to read %s line %d", filename, lno);
        }
      } // end if ok == 1
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
}
