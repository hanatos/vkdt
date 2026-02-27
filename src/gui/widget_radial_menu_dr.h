#pragma once
#include "widget_radial_menu.h"

static inline void
dt_radial_menu_dr(
    dt_radial_menu_dr_t *m,
    struct nk_rect bounds) // bounds inside which to click/where to display if opened with gamepad
{
  GLFWgamepadstate gamepad = {0};
  if(vkdt.wstate.have_joystick) glfwGetGamepadState(vkdt.wstate.joystick_id, &gamepad);
  int mouse_activated = !dt_gui_input_blocked() &&
    nk_input_is_mouse_hovering_rect(&vkdt.ctx.input, bounds) &&
    vkdt.wstate.active_widget_modid < 0 &&
    vkdt.ctx.input.mouse.buttons[NK_BUTTON_RIGHT].down;
  int gamepad_activated = gamepad.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER];

  if((gamepad_activated || mouse_activated) &&
     (m->state == s_radial_menu_dr_closed))
  { // when first activated, collect our list from custom favs
    if(mouse_activated) m->mouse = 1;
    else m->mouse = 0;

    m->mcnt = 0;
    for(int i=0;i<vkdt.fav_cnt&&m->mcnt<20;i++)
    {
      int modid = vkdt.fav_modid[i];
      int parid = vkdt.fav_parid[i];
      if(modid == -1)
      {
        int pst = parid;
        if(pst >= sizeof(vkdt.fav_preset_name) / sizeof(vkdt.fav_preset_name[0])) continue;
        snprintf(m->mtxt[m->mcnt], 50, "preset %s", vkdt.fav_preset_name[pst]);
        m->mmodid [m->mcnt] = -1;
        m->mparid [m->mcnt] = pst;
        m->mtxtptr[m->mcnt] = m->mtxt[m->mcnt];
        m->mcnt++;
        continue;
      }
      const dt_ui_param_t *param = vkdt.graph_dev.module[modid].so->param[parid];
      if(param->widget.type == dt_token("slider"))
      { // currently the only supported kind of widget
        snprintf(m->mtxt[m->mcnt], 50, "%"PRItkn" %"PRItkn,
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        m->mmodid [m->mcnt] = modid;
        m->mparid [m->mcnt] = parid;
        m->mtxtptr[m->mcnt] = m->mtxt[m->mcnt];
        m->mcnt++;
      }
    }
    m->state = s_radial_menu_dr_main;
  }

  if(m->state == s_radial_menu_dr_main)
  {
    struct nk_vec2 pos = vkdt.ctx.input.mouse.pos;
    if(!m->mouse) pos = (struct nk_vec2){
        bounds.x + bounds.w/2 + gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]*bounds.w,
        bounds.y + bounds.h/2 + gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]*bounds.w};
    if(m->mouse) m->selected = m->left = -1;
    int sel = dt_radial_menu(&vkdt.ctx,
        // could pass mouse positions like
        // m->mouse ? vkdt.ctx.input.mouse.buttons[NK_BUTTON_RIGHT].clicked_pos.x :
        // m->mouse ? vkdt.ctx.input.mouse.buttons[NK_BUTTON_RIGHT].clicked_pos.y :
        // but would need to be careful that clicked_pos is reset when the button is released.
        // such that sel is valid further down, would need to cache clicked_pos on m.
        bounds.x + bounds.w/2,
        bounds.y + bounds.h/2,
        MIN(bounds.w, bounds.h)/3,
        pos.x, pos.y,
        m->mcnt, &m->selected, &m->left, m->mtxtptr);
    if(( m->mouse && !vkdt.ctx.input.mouse.buttons[NK_BUTTON_RIGHT].down) ||
       (!m->mouse && !gamepad.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]))
    {
      if(sel > -1)
      {
        if(m->mmodid[sel] == -1)
        { // preset button, apply it
          int pst = m->mparid[sel];
          char filename[512];
          const char *preset = vkdt.fav_preset_name[pst];
          snprintf(filename, sizeof(filename), "%s.pst", preset);
          uint32_t err_lno = render_darkroom_apply_preset(filename);
          if(err_lno)
            dt_gui_notification("failed to read preset %s line %u", filename, err_lno);
          dt_radial_menu_dr_close(m);
        }
        else
        {
          m->state = s_radial_menu_dr_widget;
          m->modid = m->mmodid[sel];
          m->parid = m->mparid[sel];
          dt_gamepadhelp_push();
          dt_gamepadhelp_clear();
          dt_gamepadhelp_set(dt_gamepadhelp_ps, "toggle this help");
          dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_R, "x: modify value, y: fast");
          dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "reset value");
          dt_gamepadhelp_set(dt_gamepadhelp_button_cross, "accept value");
        }
      }
      else dt_radial_menu_dr_close(m);
    }
  }

  if(m->state == s_radial_menu_dr_widget)
  { // radial menu widget active:
    if(dt_radial_widget(&vkdt.ctx, m->modid, m->parid,
        m->mouse ? 0 : gamepad.axes+GLFW_GAMEPAD_AXIS_RIGHT_X,
        m->mouse ? 0 : gamepad.axes+GLFW_GAMEPAD_AXIS_RIGHT_Y))
      dt_radial_menu_dr_close(m);
  }
}

// reset widget and close
static inline void
dt_radial_menu_dr_reset(dt_radial_menu_dr_t *m)
{
  m->selected = m->left = -1;
  if(m->state == s_radial_menu_dr_widget)
  { // reset parameters
    const double throttle = 2.0; // min delay for same param in history, in seconds
    const int num = 0; // can't do multi-dimensional values now
    const dt_ui_param_t *param = vkdt.graph_dev.module[m->modid].so->param[m->parid];
    const dt_token_t widget = param->widget.type;
    if(widget != dt_token("slider")) return;
    float *val = (float*)(vkdt.graph_dev.module[m->modid].param + param->offset) + num;
    float oldval = val[0];
    dt_graph_run_t flags = s_graph_run_none;
    memcpy(vkdt.graph_dev.module[m->modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));
    if(vkdt.graph_dev.module[m->modid].so->check_params)
      flags = vkdt.graph_dev.module[m->modid].so->check_params(vkdt.graph_dev.module+m->modid, m->parid, num, &oldval);
    vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | flags;
    vkdt.graph_dev.active_module = m->modid;
    dt_graph_history_append(&vkdt.graph_dev, m->modid, m->parid, throttle);
  }
  dt_radial_menu_dr_close(m);
}
