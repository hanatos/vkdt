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

void render_perf_overlay()
{
#if 0 // TODO port
  const int nvmask = 127;
  static float values[128] = {0.0f};
  static int values_offset = 0;
  char overlay[32];
  values[values_offset] = vkdt.graph_dev.query[(vkdt.graph_dev.frame+1)%2].last_frame_duration;
  snprintf(overlay, sizeof(overlay), "%.2fms", values[values_offset]);

  ImVec2 sz  = ImGui::GetMainViewport()->Size;
  ImVec2 pos = ImGui::GetMainViewport()->Pos;
  pos = ImVec2(pos.x + sz.x/2.0, pos.y + sz.y * 0.1);
  sz = ImVec2(sz.x/2.0, sz.y*0.1);
  ImU32 col = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
  for(int i=0;i<IM_ARRAYSIZE(values)-1;i++)
  {
    int j0 = (i + values_offset    )&nvmask;
    int j1 = (i + values_offset + 1)&nvmask;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(pos.x + sz.x * (i    /(nvmask+1.0)), pos.y + sz.y * (1.0 - values[j0]/100.0)),
        ImVec2(pos.x + sz.x * ((i+1)/(nvmask+1.0)), pos.y + sz.y * (1.0 - values[j1]/100.0)),
        col, 4.0);
  }
  ImU32 bgcol = ImGui::GetColorU32(ImGuiCol_PlotLines);
  ImGui::GetWindowDrawList()->AddLine(pos, ImVec2(pos.x+sz.x, pos.y), bgcol);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y+sz.y), ImVec2(pos.x+sz.x, pos.y+sz.y), bgcol);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, (1.0-16.0/100.0)*sz.y+pos.y), ImVec2(pos.x+sz.x, (1.0-16.0/100.0)*sz.y+pos.y), bgcol);
  ImGui::GetWindowDrawList()->AddText(dt_gui_imgui_get_font(2), dt_gui_imgui_get_font(2)->FontSize, pos, col, overlay);
  values_offset = (values_offset + 1) & nvmask;
#endif
}

void render_darkroom_widget(int modid, int parid)
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
  if(nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_DOUBLE, nk_widget_bounds(ctx))) \
  { \
    memcpy(vkdt.graph_dev.module[modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));\
    change = 1; \
  }

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
      dt_gui_notification("added keyframe for frame %u %" PRItkn ":%" PRItkn ":%" PRItkn, \
          g->frame, dt_token_str(g->module[modid].name), dt_token_str(g->module[modid].inst), dt_token_str(param->name));\
      dt_graph_history_keyframe(&vkdt.graph_dev, modid, ki);\
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
  switch(param->widget.type)
  {
    case dt_token("slider"):
    {
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      if(param->type == dt_token("float"))
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        float oldval = *val;
        RESETBLOCK
        nk_property_float(ctx, "#", param->widget.min, val, param->widget.max, 0.1, .001);
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
        nk_property_int(ctx, "#", param->widget.min, val, param->widget.max, 1, 0.1);
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
      break;
    }
#if 0
    case dt_token("coledit"):
    { // only works for param->type == float and count == 3
      if((num % 3) == 0)
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        float  oldval = *val;
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        if(ImGui::ColorEdit3(str, val, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueWheel |
             ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_InputRGB   | ImGuiColorEditFlags_Float))
        RESETBLOCK {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              flags | s_graph_run_record_cmd_buf);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        KEYFRAME
        TOOLTIP
      }
      break;
    }
    case dt_token("vslider"):
    {
      if(param->type == dt_token("float"))
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        float oldval = *val;
        if(count == 3)
        { // assume rgb vsliders
          ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.5f, 0.5f));
          ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.6f, 0.5f));
          ImGui::PushStyleColor(ImGuiCol_FrameBgActive, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.7f, 0.5f));
          ImGui::PushStyleColor(ImGuiCol_SliderGrab, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.9f, 0.9f));
          if(ImGui::VSliderFloat("##v",
                ImVec2(vkdt.state.panel_wd / 10.0, vkdt.state.panel_ht * 0.2), val,
                param->widget.min, param->widget.max, ""))
          RESETBLOCK {
            if(io.KeyShift) // lockstep all three if shift is pressed
              for(int k=3*(num/3);k<3*(num/3)+3;k++) val[k-num] = val[0];
            dt_graph_run_t flags = s_graph_run_none;
            if(vkdt.graph_dev.module[modid].so->check_params)
              flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
            vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
                s_graph_run_record_cmd_buf | flags);
            vkdt.graph_dev.active_module = modid;
            dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
          }
          KEYFRAME
          if (ImGui::IsItemActive() || ImGui::IsItemHovered())
            dt_gui_set_tooltip("%s %.3f\nhold shift to lockstep rgb", str, val[0]);

          ImGui::PopStyleColor(4);
          if(parid < vkdt.graph_dev.module[modid].so->num_params - 1 ||
              num < count - 1) ImGui::SameLine();
        }
        else
        {
          if(ImGui::VSliderFloat("##v",
                ImVec2(vkdt.state.panel_wd / (count+1.0), vkdt.state.panel_ht * 0.2), val,
                param->widget.min, param->widget.max, ""))
          RESETBLOCK {
            if(io.KeyShift) // lockstep all three if shift is pressed
              for(int k=0;k<count;k++) val[k-num] = val[0];
            dt_graph_run_t flags = s_graph_run_none;
            if(vkdt.graph_dev.module[modid].so->check_params)
              flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
            vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
                s_graph_run_record_cmd_buf | flags);
            vkdt.graph_dev.active_module = modid;
            dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
          }
          KEYFRAME
          TOOLTIP

          if(parid < vkdt.graph_dev.module[modid].so->num_params - 1 ||
              num < count - 1) ImGui::SameLine();
        }
      }
      break;
    }
    case dt_token("bitmask"):
    { // select named entries of a bitmask
      if(param->type == dt_token("int"))
      {
        int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        int32_t oldval = *val;
        ImGui::Text("%s = 0x%x", str, val[0]);
        if(ImGui::IsItemClicked(0)) RESETBLOCK {};
        TOOLTIP
        const char *c = (const char *)param->widget.data;
        for(int k=0;k<32;k++)
        {
          const int sel = val[0] & (1<<k);
          if(sel) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, vkdt.wstate.fontsize*0.2);
          char label[10];
          snprintf(label, sizeof(label), "%d", k);
          if(ImGui::Button(label, ImVec2(vkdt.state.panel_wd * 0.6 / 8.0, 0))) { val[0] ^= (1<<k); change = 1; }
          if(ImGui::IsItemHovered()) dt_gui_set_tooltip(c);
          if(sel) ImGui::PopStyleVar();
          for(;*c!=0;c++);
          c++;
          if(*c == 0) break; // no more named bits in mask
          if(k % 8 != 7) ImGui::SameLine(); // arrange in blocks of 8
        }

        if(change)
        {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              flags | s_graph_run_record_cmd_buf);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
          vkdt.wstate.busy += 2;
        }
      }
      break;
    }
    case dt_token("callback"):
    { // special callback button
      if(num == 0)
      {
        if(ImGui::Button(str, ImVec2(halfw, 0)))
        {
          dt_module_t *m = vkdt.graph_dev.module+modid;
          if(m->so->ui_callback) m->so->ui_callback(m, param->name);
          // TODO: probably needs a history item appended. for all parameters?
        }
        TOOLTIP
        if(param->type == dt_token("string"))
        {
          ImGui::SameLine();
          char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
          ImGui::PushItemWidth(-1);
          ImGui::InputText("##s", v, count);
          ImGui::PopItemWidth();
        }
      }
      break;
    }
    case dt_token("combo"):
    { // combo box
      if(param->type == dt_token("int"))
      {
        int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        int32_t oldval = *val;
        if(ImGui::Combo(str, val, (const char *)param->widget.data))
        RESETBLOCK
        {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              flags | s_graph_run_record_cmd_buf);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
          vkdt.wstate.busy += 2;
        }
        TOOLTIP
      }
      break;
    }
    case dt_token("colour"):
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
      snprintf(string, sizeof(string), "%" PRItkn " %d", dt_token_str(param->name), num);
      ImVec4 col(val[0], val[1], val[2], 1.0f);
      ImVec2 size(0.1*vkdt.state.panel_wd, 0.1*vkdt.state.panel_wd);
      ImGui::ColorButton(string, col, ImGuiColorEditFlags_HDR, size);
      TOOLTIP
      if((num < count - 1) && ((num % 6) != 5))
        ImGui::SameLine();
      break;
    }
    case dt_token("pers"):
    {
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
      const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
      const float aspect = iwd/iht;
      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        int accept = 0;
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
        snprintf(string, sizeof(string), "%" PRItkn" done", dt_token_str(param->name));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))
        {
          dt_gamepadhelp_pop();
          dt_module_set_param_float(vkdt.graph_dev.module+modid, dt_token("rotate"), vkdt.wstate.state[9]);
          widget_abort();
        }
        else if(ImGui::Button(string, ImVec2(halfw, 0)) || accept)
        {
          dt_gamepadhelp_pop();
          dt_module_set_param_float(vkdt.graph_dev.module+modid, dt_token("rotate"), vkdt.wstate.state[9]);
          dt_module_set_param_float_n(vkdt.graph_dev.module+modid, dt_token("crop"), vkdt.wstate.state+10, 4);
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        ImGui::PopStyleColor();
      }
      else
      {
        snprintf(string, sizeof(string), "%" PRItkn" start", dt_token_str(param->name));
        if(ImGui::Button(string, ImVec2(halfw, 0)))
        {
          dt_gamepadhelp_push();
          dt_gamepadhelp_clear();
          dt_gamepadhelp_set(dt_gamepadhelp_R1, "select next corner");
          dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_R, "move corner");
          dt_gamepadhelp_set(dt_gamepadhelp_button_cross, "accept changes");
          dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "discard changes");
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
        if(0) RESETBLOCK {
          vkdt.graph_dev.runflags = s_graph_run_all;
          dt_image_reset_zoom(&vkdt.wstate.img_widget);
        }
        TOOLTIP
      }
      num = count;
      break;
    }
    case dt_token("straight"):
    { // horizon line straighten tool for rotation
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
      float oldval = *val;

      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        if(ImGui::Button("done"))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        ImGui::PopStyleColor();
      }
      else
      {
        if(ImGui::Button("straighten"))
        {
          widget_end();
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = 0;
          vkdt.wstate.active_widget_parsz = sizeof(float);
          vkdt.wstate.selected = -1;
          memcpy(vkdt.wstate.state, val, sizeof(float));
        }
        if(ImGui::IsItemHovered())
          dt_gui_set_tooltip("draw lines on the image to be rotated to align exactly horizontally or vertically");
      }

      // full manual control over parameter using the slider:
      ImGui::SameLine(); // almost matches the right size:
      ImGui::SetNextItemWidth(ImGui::GetStyle().ItemSpacing.x + 0.66*vkdt.state.panel_wd - ImGui::GetCursorPosX());

      if(ImGui::SliderFloat(str, val, 0.0f, 360.0f, "%2.5f"))
      RESETBLOCK {
        dt_graph_run_t flags = s_graph_run_none;
        if(vkdt.graph_dev.module[modid].so->check_params)
          flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
        vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
            s_graph_run_record_cmd_buf | flags);
        vkdt.graph_dev.active_module = modid;
        dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
      }
      KEYFRAME
      TOOLTIP
      break;
    }
    case dt_token("crop"):
    {
      ImGui::InputFloat("aspect ratio", &vkdt.wstate.aspect, 0.0f, 4.0f, "%.3f");
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
      const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
      const float aspect = iwd/iht;
      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        int accept = 0;
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

        snprintf(string, sizeof(string), "%" PRItkn" done",
            dt_token_str(param->name));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
           ImGui::IsKeyPressed(ImGuiKey_Escape)||
           ImGui::IsKeyPressed(ImGuiKey_CapsLock))
        {
          widget_abort();
          dt_gamepadhelp_pop();
          dt_image_reset_zoom(&vkdt.wstate.img_widget);
        }
        else if(ImGui::Button(string, ImVec2(halfw, 0)) || accept)
        {
          vkdt.wstate.state[0] = .5f + MAX(1.0f, 1.0f/aspect) * (vkdt.wstate.state[0] - .5f);
          vkdt.wstate.state[1] = .5f + MAX(1.0f, 1.0f/aspect) * (vkdt.wstate.state[1] - .5f);
          vkdt.wstate.state[2] = .5f + MAX(1.0f,      aspect) * (vkdt.wstate.state[2] - .5f);
          vkdt.wstate.state[3] = .5f + MAX(1.0f,      aspect) * (vkdt.wstate.state[3] - .5f);
          widget_end();
          dt_gamepadhelp_pop();
          dt_image_reset_zoom(&vkdt.wstate.img_widget);
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        ImGui::PopStyleColor();
      }
      else
      {
        snprintf(string, sizeof(string), "%" PRItkn" start",
            dt_token_str(param->name));
        if(ImGui::Button(string, ImVec2(halfw, 0)))
        {
          dt_gamepadhelp_push();
          dt_gamepadhelp_clear();
          dt_gamepadhelp_set(dt_gamepadhelp_R1, "select next edge");
          dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_R, "move edge");
          dt_gamepadhelp_set(dt_gamepadhelp_button_cross, "accept changes");
          dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "discard changes");
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
        if(0) RESETBLOCK {
          vkdt.graph_dev.runflags = s_graph_run_all;
          dt_image_reset_zoom(&vkdt.wstate.img_widget);
        }
        KEYFRAME
        TOOLTIP
      }
      dt_module_t *m = vkdt.graph_dev.module+modid;
      if(m->so->ui_callback)
      {
        ImGui::SameLine();
        if(ImGui::Button("auto crop", ImVec2(halfw, 0)))
        {
          m->so->ui_callback(m, param->name);
          dt_image_reset_zoom(&vkdt.wstate.img_widget);
          vkdt.graph_dev.runflags = s_graph_run_all;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        if(ImGui::IsItemHovered())
          dt_gui_set_tooltip("automatically crop away black rims");
      }
      num = count;
      break;
    }
    case dt_token("pick"):  // simple aabb for selection, no distortion transform
    {
      int sz = dt_ui_param_size(param->type, 4);
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid &&
         vkdt.wstate.active_widget_parnm == num)
      {
        snprintf(string, sizeof(string), "done");
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        if(ImGui::Button(string))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        ImGui::PopStyleColor();
      }
      else
      {
        snprintf(string, sizeof(string), "%02d", num);
        if(ImGui::Button(string))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = num;
          vkdt.wstate.active_widget_parsz = sz;
          // copy to quad state
          memcpy(vkdt.wstate.state, v, sz);
        }
        TOOLTIP
      }
      if((num < count - 1) && ((num % 6) != 5))
        ImGui::SameLine();
      break;
    }
    case dt_token("rbmap"): // red/blue chromaticity mapping via src/target coordinates
    {
      if(num == 0) ImGui::Dummy(ImVec2(0, 0.01*vkdt.state.panel_wd));
      ImVec2 size = ImVec2(vkdt.state.panel_wd * 0.135, 0);
      int sz = dt_ui_param_size(param->type, 6); // src rgb -> tgt rgb is six floats
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(v[0], v[1], v[2], 1.0));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(v[3], v[4], v[5], 1.0));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, vkdt.state.panel_wd*0.02);
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid &&
         vkdt.wstate.active_widget_parnm == num)
      {
        snprintf(string, sizeof(string), "done");
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        if(ImGui::Button(string, size))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        ImGui::PopStyleColor();
      }
      else
      {
        snprintf(string, sizeof(string), "%02d", num);
        if(ImGui::Button(string, size))
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
      ImGui::PopStyleVar(1);
      ImGui::PopStyleColor(2);
      if((num < count - 1) && ((num % 6) != 5))
      {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(0.01*vkdt.state.panel_wd, 0));
        ImGui::SameLine();
      }
      else if(num == count - 1)
      { // edit specific colour patch below list of patches:
        ImGui::Dummy(ImVec2(0, 0.01*vkdt.state.panel_wd));
        if(vkdt.wstate.active_widget_modid == modid &&
           vkdt.wstate.active_widget_parid == parid)
        { // now add ability to change target colour coordinate
          int active_num = vkdt.wstate.active_widget_parnm;
          for(int i=0;i<3;i++)
          {
            float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset + active_num*sz) + 3 + i;
            float oldval = *val;
            const char *label[] = {"red", "green", "blue"};
            if(ImGui::SliderFloat(label[i], val, 0.0, 1.0, "%2.5f"))
            { // custom resetblock: set only this colour spot to identity mapping
              if(time_now - doubleclick_time > ImGui::GetIO().MouseDoubleClickTime) doubleclick = 0;
              if(doubleclick) memcpy(val-i, val-i-3, sizeof(float)*3);
              change = 1;
            }
            if((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
               (ImGui::IsItemFocused() && gamepad_reset))
            {
              doubleclick_time = time_now;
              gamepad_reset = 0;
              doubleclick = 1;
              memcpy(val-i, val-i-3, sizeof(float)*3);
              change = 1;
            }
            if(change)
            {
              dt_graph_run_t flags = s_graph_run_none;
              if(vkdt.graph_dev.module[modid].so->check_params)
                flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
              vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
                  s_graph_run_record_cmd_buf | flags);
              vkdt.graph_dev.active_module = modid;
              dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
            }
          }
        }
      }
      else ImGui::Dummy(ImVec2(0, 0.02*vkdt.state.panel_wd));
      break;
    }
    case dt_token("grab"):  // grab all input
    {
      if(num != 0) break;
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid)
      {
        if(ImGui::Button("stop [esc]", ImVec2(halfw, 0)))
        {
          // dt_gui_dr_toggle_fullscreen_view();
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_module_input_event_t p = { .type = -1 };
          if(modid >= 0 && mod->so->input) mod->so->input(mod, &p);
          widget_end();
        }
      }
      else
      {
        if(ImGui::Button("grab input", ImVec2(halfw, 0)))
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
        KEYFRAME
        TOOLTIP
        ImGui::SameLine();
        if(ImGui::Button("grab fullscreen", ImVec2(halfw, 0)))
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
        KEYFRAME
        TOOLTIP
      }
      break;
    }
    case dt_token("draw"):
    {
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        snprintf(string, sizeof(string), "%" PRItkn" done", dt_token_str(param->name));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        if(ImGui::Button(string, ImVec2(halfw, 0)))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        ImGui::PopStyleColor();
      }
      else
      {
        snprintf(string, sizeof(string), "%" PRItkn" start", dt_token_str(param->name));
        if(ImGui::Button(string, ImVec2(halfw, 0)))
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
        if(ImGui::IsItemHovered())
          dt_gui_set_tooltip("start drawing brush strokes with the mouse\n"
              "scroll - fine tune radius\n"
              "ctrl scroll - fine tune hardness\n"
              "shift scroll - fine tune opacity\n"
              "right click - discard last stroke");
      }
      if(vkdt.wstate.mapped)
      {
        ImGui::SameLine();
        ImGui::Text("%d/10000 verts", ((uint32_t *)vkdt.wstate.mapped)[0]);
      }
      num = count;
      break;
    }
    case dt_token("filename"):
    {
      if(num == 0)
      { // only show first, cnt refers to allocation length of string param
        char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
        if(ImGui::InputText(str, v, count, ImGuiInputTextFlags_EnterReturnsTrue))
        {
          if(ImGui::IsKeyDown(ImGuiKey_Enter))
          { // kinda grave change, rerun all, but only if enter pressed
            vkdt.graph_dev.runflags = s_graph_run_all;
            dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
          }
        }
        TOOLTIP
      }
      break;
    }
    case dt_token("rgb"):
    {
      float *col = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
      ImGui::PushStyleColor(ImGuiCol_FrameBg, gamma(ImVec4(col[0], col[1], col[2], 1.0)));
      for(int comp=0;comp<3;comp++)
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num + comp;
        float oldval = *val;
        snprintf(string, sizeof(string), "%" PRItkn " %s",
            dt_token_str(param->name),
            comp == 0 ? "red" : (comp == 1 ? "green" : "blue"));
        if(ImGui::SliderFloat(string, val,
              param->widget.min, param->widget.max, "%2.5f"))
        RESETBLOCK
        {
          if(io.KeyShift) // lockstep all three if shift is pressed
            for(int k=3*(comp/3);k<3*(comp/3)+3;k++) val[k-comp] = val[0];
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, num, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              s_graph_run_record_cmd_buf | flags);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        KEYFRAME
        TOOLTIP
      }
      ImGui::PopStyleColor();
      if(param->cnt == count && count <= 4) num = 4; // non-array rgb controls
      break;
    }
    case dt_token("print"):
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      ImGui::Text("%g | %g | %g   %" PRItkn, val[0], val[1], val[2], dt_token_str(param->name));
      num = count; // we've done it all at once
      break;
    }
#endif
    default:;
  }
  } // end for multiple widgets
#undef RESETBLOCK
#undef KEYFRAME
}

void render_darkroom_widgets(
    dt_graph_t *graph,          // graph with the modules and parameters
    int         curr,           // which module id to draw
    char       *open,           // gui state: is the expander open
    int32_t    *active_module)  // id of the currently active module
{
  char name[30];
  dt_module_t *const arr = graph->module;
  if(!arr[curr].so->num_params) return;

  struct nk_context *ctx = &vkdt.ctx;
  const float ratio[] = {0.06f, 0.06f, 0.88f};
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn,
      dt_token_str(arr[curr].name), dt_token_str(arr[curr].inst));
  dt_module_t *module = graph->module+curr;
  nk_layout_row(ctx, NK_DYNAMIC, row_height, 3, ratio);

  struct nk_rect bound = nk_layout_widget_bounds(ctx);
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
    nk_label(ctx, module->disabled ? "\ue612" : "\ue836", NK_TEXT_LEFT);
    if(nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, box))
    {
      int bad = 0;
      for(int c=0;c<module->num_connectors;c++)
        if(module->connector[c].frames > 1) bad = 1;
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
    nk_label(ctx, "\ue15b", NK_TEXT_LEFT);
  }
  nk_label(ctx, open[curr] ? "\ue5cf" : "\ue5cc", NK_TEXT_LEFT);
  nk_style_pop_font(ctx);
  nk_label(ctx, name, NK_TEXT_LEFT);
  if(nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, bound))
  {
    if(!open[curr])
    { // just opened, now this is the 'active module'.
      *active_module = curr;
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
      }
      open[curr] = 1;
    }
    else open[curr] = 0;
  }
  if(open[curr])
  {
    if(*active_module == curr &&
        dt_module_get_connector(arr+curr, dt_token("dspy")) >= 0)
    {
      dt_node_t *out_dspy = dt_graph_get_display(graph, dt_token("dspy"));
      if(out_dspy && vkdt.graph_res == VK_SUCCESS)
      {
        // TODO: center and aspect
        float iwd = out_dspy->connector[0].roi.wd;
        float iht = out_dspy->connector[0].roi.ht;
        float scale = MIN(vkdt.state.panel_wd / iwd, 2.0f/3.0f*vkdt.state.panel_wd / iht);
        int ht = scale * iht;
        nk_layout_row_dynamic(ctx, ht, 1);
        struct nk_image img = nk_image_ptr(out_dspy->dset[0]);
        nk_image(ctx, img);
      }
    }
    for(int i=0;i<arr[curr].so->num_params;i++)
      render_darkroom_widget(curr, i);
  }
}
