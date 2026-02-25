#define NK_IMPLEMENTATION
#define NK_GLFW_VULKAN_IMPLEMENTATION
#include "nk.h"
#include "gui.h"
#include "view.h"
#include "core/log.h"
#include "qvk/qvk.h"
#include "pipe/graph-export.h"
#include "pipe/graph-io.h"
#include "pipe/graph-history.h"
#include "api.h"
#include "render.h"
#include "widget_draw.h"
#include "render_view.h"
#include "widget_radial_menu_dr-fwd.h"
#include "pipe/modules/matrices.h"

static inline int
style_name_to_colour(const char *name)
{
  if(!strcmp(name, "TEXT"))                        return  NK_COLOR_TEXT;
  if(!strcmp(name, "TEXT_HOVER"))                  return  NK_COLOR_TEXT_HOVER;
  if(!strcmp(name, "TEXT_ACTIVE"))                 return  NK_COLOR_TEXT_ACTIVE;
  if(!strcmp(name, "WINDOW"))                      return  NK_COLOR_WINDOW;
  if(!strcmp(name, "HEADER"))                      return  NK_COLOR_HEADER;
  if(!strcmp(name, "BORDER"))                      return  NK_COLOR_BORDER;
  if(!strcmp(name, "BUTTON"))                      return  NK_COLOR_BUTTON;
  if(!strcmp(name, "BUTTON_HOVER"))                return  NK_COLOR_BUTTON_HOVER;
  if(!strcmp(name, "BUTTON_ACTIVE"))               return  NK_COLOR_BUTTON_ACTIVE;
  if(!strcmp(name, "TOGGLE"))                      return  NK_COLOR_TOGGLE;
  if(!strcmp(name, "TOGGLE_HOVER"))                return  NK_COLOR_TOGGLE_HOVER;
  if(!strcmp(name, "TOGGLE_CURSOR"))               return  NK_COLOR_TOGGLE_CURSOR;
  if(!strcmp(name, "SELECT"))                      return  NK_COLOR_SELECT;
  if(!strcmp(name, "SELECT_ACTIVE"))               return  NK_COLOR_SELECT_ACTIVE;
  if(!strcmp(name, "SLIDER"))                      return  NK_COLOR_SLIDER;
  if(!strcmp(name, "SLIDER_CURSOR"))               return  NK_COLOR_SLIDER_CURSOR;
  if(!strcmp(name, "SLIDER_CURSOR_HOVER"))         return  NK_COLOR_SLIDER_CURSOR_HOVER;
  if(!strcmp(name, "SLIDER_CURSOR_ACTIVE"))        return  NK_COLOR_SLIDER_CURSOR_ACTIVE;
  if(!strcmp(name, "PROPERTY"))                    return  NK_COLOR_PROPERTY;
  if(!strcmp(name, "EDIT"))                        return  NK_COLOR_EDIT;
  if(!strcmp(name, "EDIT_CURSOR"))                 return  NK_COLOR_EDIT_CURSOR;
  if(!strcmp(name, "COMBO"))                       return  NK_COLOR_COMBO;
  if(!strcmp(name, "CHART"))                       return  NK_COLOR_CHART;
  if(!strcmp(name, "CHART_COLOR"))                 return  NK_COLOR_CHART_COLOR;
  if(!strcmp(name, "CHART_COLOR_HIGHLIGHT"))       return  NK_COLOR_CHART_COLOR_HIGHLIGHT;
  if(!strcmp(name, "SCROLLBAR"))                   return  NK_COLOR_SCROLLBAR;
  if(!strcmp(name, "SCROLLBAR_CURSOR"))            return  NK_COLOR_SCROLLBAR_CURSOR;
  if(!strcmp(name, "SCROLLBAR_CURSOR_HOVER"))      return  NK_COLOR_SCROLLBAR_CURSOR_HOVER;
  if(!strcmp(name, "SCROLLBAR_CURSOR_ACTIVE"))     return  NK_COLOR_SCROLLBAR_CURSOR_ACTIVE;
  if(!strcmp(name, "TAB_HEADER"))                  return  NK_COLOR_TAB_HEADER;
  if(!strcmp(name, "KNOB"))                        return  NK_COLOR_KNOB;
  if(!strcmp(name, "KNOB_CURSOR"))                 return  NK_COLOR_KNOB_CURSOR;
  if(!strcmp(name, "KNOB_CURSOR_HOVER"))           return  NK_COLOR_KNOB_CURSOR_HOVER;
  if(!strcmp(name, "KNOB_CURSOR_ACTIVE"))          return  NK_COLOR_KNOB_CURSOR_ACTIVE;
  if(!strcmp(name, "DT_ACCENT_TEXT"))              return  NK_COLOR_DT_ACCENT_TEXT;
  if(!strcmp(name, "DT_ACCENT_TEXT_HOVER"))        return  NK_COLOR_DT_ACCENT_TEXT_HOVER;
  if(!strcmp(name, "DT_ACCENT_TEXT_ACTIVE"))       return  NK_COLOR_DT_ACCENT_TEXT_ACTIVE;
  if(!strcmp(name, "DT_ACCENT"))                   return  NK_COLOR_DT_ACCENT;
  if(!strcmp(name, "DT_ACCENT_HOVER"))             return  NK_COLOR_DT_ACCENT_HOVER;
  if(!strcmp(name, "DT_ACCENT_ACTIVE"))            return  NK_COLOR_DT_ACCENT_ACTIVE;
  if(!strcmp(name, "DT_BACKGROUND"))               return  NK_COLOR_DT_BACKGROUND;
  return -1;
}

static inline void
read_style_colours(struct nk_context *ctx)
{
  // set default colours
  nk_style_default(ctx);
  // see if we can find 
  FILE *f = dt_graph_open_resource(0, 0, "style.txt", "rb");
  if(!f) return;
  while(!feof(f))
  {
    int rgba[4];
    char name[128];
    fscanf(f, "%127[^:]:%d:%d:%d:%d%*[^\n]", name, rgba, rgba+1, rgba+2, rgba+3);
    if(!name[0]) break;
    if(fgetc(f) == EOF) break;
    int idx = style_name_to_colour(name);
    if(idx >= 0) vkdt.style.colour[idx] = nk_rgba(rgba[0], rgba[1], rgba[2], rgba[3]);
  }
  // init from this table
  nk_style_from_table(ctx, vkdt.style.colour);
}

void dt_gui_init_fonts()
{
  dt_gui_style_to_state(); // compute vkdt.style.fontsize based on scale factors/dpi settings
  static float curr_fontsize = 0.0f;
  // fontsize represents font height, internally it scales the EM square (font will convert these measures)
  float fontsize = vkdt.style.fontsize;
  if(fontsize == curr_fontsize) return;

  if(curr_fontsize == 0.0f)
  {
    const char *fontfile = dt_rc_get(&vkdt.rc, "gui/msdffont", "font");
    threads_mutex_lock(&qvk.queue[qvk.qid[s_queue_graphics]].mutex);
    int ret = nk_glfw3_font_load(fontfile, fontsize,
        vkdt.win.command_buffer[vkdt.win.frame_index%DT_GUI_MAX_IMAGES],
        qvk.queue[qvk.qid[s_queue_graphics]].queue);
    threads_mutex_unlock(&qvk.queue[qvk.qid[s_queue_graphics]].mutex);
    nk_style_set_font(&vkdt.ctx, nk_glfw3_font(0));
    if(ret)
      dt_log(s_log_err|s_log_gui, "will not be able to display text! please find the missing font file!");
  }
  // maybe already loaded, anyways set font sizes
  nk_glfw3_font(0)->height = fontsize;
  nk_glfw3_font(1)->height = floorf(1.5*fontsize);
  nk_glfw3_font(2)->height = 2*fontsize;
  curr_fontsize = fontsize;
  // update padding to match fontsize
  nk_style_from_table(&vkdt.ctx, vkdt.style.colour);
}

void dt_gui_update_cm()
{
  read_style_colours(&vkdt.ctx);

  int monitors_cnt;
  GLFWmonitor** monitors = glfwGetMonitors(&monitors_cnt);
  const char *name0 = glfwGetMonitorName(monitors[0]);
  const char *name1 = glfwGetMonitorName(monitors[MIN(monitors_cnt-1, 1)]);
  int xpos0, xpos1, ypos;
  glfwGetMonitorPos(monitors[0], &xpos0, &ypos);
  glfwGetMonitorPos(monitors[MIN(monitors_cnt-1, 1)], &xpos1, &ypos);
  float gamma0[] = {0, 0, 0}; // 0 means use sRGB TRC
  float rec2020_to_dspy0[] = matrix_rec2020_to_rec709;
  float gamma1[] = {0, 0, 0};
  float rec2020_to_dspy1[] = matrix_rec2020_to_rec709;
  if(vkdt.win.surf_format.colorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT)
  { // fake cm for sdr monitors:
    if(monitors_cnt > 2)
      dt_log(s_log_gui, "you have more than 2 monitors attached! only the first two will be colour managed!");
    char tmp[PATH_MAX] = {0};
    snprintf(tmp, sizeof(tmp), "display.%s", name0);
    FILE *f = dt_graph_open_resource(0, 0, tmp, "rb");
    if(f)
    {
      dt_log(s_log_gui, "loading display profile %s", tmp);
      fscanf(f, "%f %f %f\n", gamma0, gamma0+1, gamma0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+0, rec2020_to_dspy0+1, rec2020_to_dspy0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+3, rec2020_to_dspy0+4, rec2020_to_dspy0+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+6, rec2020_to_dspy0+7, rec2020_to_dspy0+8);
      fclose(f);
    }
    else dt_log(s_log_gui, "no display profile file display.%s, using sRGB!", name0);
    if(monitors_cnt > 1)
    {
      snprintf(tmp, sizeof(tmp), "display.%s", name1);
      f = dt_graph_open_resource(0, 0, tmp, "rb");
      if(f)
      {
        dt_log(s_log_gui, "loading display profile %s", tmp);
        fscanf(f, "%f %f %f\n", gamma1, gamma1+1, gamma1+2);
        fscanf(f, "%f %f %f\n", rec2020_to_dspy1+0, rec2020_to_dspy1+1, rec2020_to_dspy1+2);
        fscanf(f, "%f %f %f\n", rec2020_to_dspy1+3, rec2020_to_dspy1+4, rec2020_to_dspy1+5);
        fscanf(f, "%f %f %f\n", rec2020_to_dspy1+6, rec2020_to_dspy1+7, rec2020_to_dspy1+8);
        fclose(f);
      }
      else dt_log(s_log_gui, "no display profile file display.%s, using sRGB!", name1);
    }
  }
  int bitdepth = 8; // the display output will be dithered according to this
  if(vkdt.win.surf_format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
     vkdt.win.surf_format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    bitdepth = 10;
  if(monitors_cnt < 2 || xpos1 == 0)
  {
    memcpy(rec2020_to_dspy1, rec2020_to_dspy0, sizeof(rec2020_to_dspy0));
    memcpy(gamma1, gamma0, sizeof(gamma0));
  }
  if(vkdt.win.surf_format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
  { // running on hdr monitor
    gamma0[0] = gamma0[1] = gamma0[2] = -1.0; // PQ
    gamma1[0] = gamma1[1] = gamma1[2] = -1.0;
    memset(rec2020_to_dspy0, 0, sizeof(rec2020_to_dspy0));
    memset(rec2020_to_dspy1, 0, sizeof(rec2020_to_dspy1));
    rec2020_to_dspy0[0] = rec2020_to_dspy0[4] = rec2020_to_dspy0[8] = 
    rec2020_to_dspy1[0] = rec2020_to_dspy1[4] = rec2020_to_dspy1[8] = 1.0f;
  }
  nk_glfw3_setup_display_colour_management(gamma0, rec2020_to_dspy0, gamma1, rec2020_to_dspy1, xpos1, bitdepth);
}

int dt_gui_init_nk()
{
  nk_init_default(&vkdt.ctx, 0);
  nk_glfw3_init(
      &vkdt.ctx,
      vkdt.win.render_pass,
      vkdt.win.window,
      qvk.device, qvk.physical_device,
      vkdt.win.num_swap_chain_images * 2560*1024,
      vkdt.win.num_swap_chain_images * 640*1024);

  dt_gui_update_cm();

  nk_buffer_init_default(&vkdt.global_buf);
  nk_command_buffer_init(&vkdt.global_cmd, &vkdt.global_buf, NK_CLIPPING_OFF);

  // prepare list of potential modules for ui selection:
  vkdt.wstate.module_names_buf = (char *)calloc(9, dt_pipe.num_modules+1);
  vkdt.wstate.module_names     = (const char **)malloc(sizeof(char*)*dt_pipe.num_modules);
  int pos = 0;
  for(uint32_t i=0;i<dt_pipe.num_modules;i++)
  {
    const char *name = dt_token_str(dt_pipe.module[i].name);
    const size_t len = strnlen(name, 8);
    memcpy(vkdt.wstate.module_names_buf + pos, name, len);
    vkdt.wstate.module_names[i] = vkdt.wstate.module_names_buf + pos;
    pos += len+1;
  }

  render_lighttable_init();
  render_darkroom_init();
  render_nodes_init();
  render_files_init();
  return 0;
}

// call from main loop:
void dt_gui_render_frame_nk()
{
  nk_buffer_clear(&vkdt.global_buf);
  double now = glfwGetTime();

  static double time_tab;
  if(glfwGetKey(vkdt.win.window, GLFW_KEY_TAB) == GLFW_PRESS && (now - time_tab > 0.2))
  { // let tab widgets know tab has been pressed
    time_tab = now;
    vkdt.wstate.tab_state = 1;
  }

  if(nk_begin(&vkdt.ctx, "dreggn",
        nk_rect(0, 0, 1, 1),
        NK_WINDOW_NO_SCROLLBAR))
  { // hack to fill background with something that still goes through our colour management shader
    struct nk_command_buffer *cmd = nk_window_get_canvas(&vkdt.ctx);
    if(cmd)
    {
      cmd->use_clipping = NK_CLIPPING_OFF;
      nk_fill_rect(cmd, (struct nk_rect){0, 0, vkdt.win.width, vkdt.win.height}, 0.0f, vkdt.style.colour[NK_COLOR_DT_BACKGROUND]);
    }
  }
  nk_end(&vkdt.ctx);

  switch(vkdt.view_mode)
  {
    case s_view_files:
      render_files();
      break;
    case s_view_lighttable:
      render_lighttable();
      break;
    case s_view_darkroom:
      render_darkroom();
      break;
    case s_view_nodes:
      render_nodes();
      break;
    default:;
  }

  // draw notification message
  threads_mutex_lock(&vkdt.wstate.notification_mutex);
  if(vkdt.wstate.notification_msg[0] &&
     now - vkdt.wstate.notification_time < 4.0)
  {
    if(nk_begin(&vkdt.ctx, "notification message",
          nk_rect(0, vkdt.state.center_y+vkdt.state.center_ht, 2*vkdt.state.center_x+vkdt.state.center_wd, 2*vkdt.state.center_y+vkdt.state.center_ht),
          NK_WINDOW_NO_SCROLLBAR))
    {
      nk_layout_row_dynamic(&vkdt.ctx, vkdt.state.center_y*0.55, 1);
      nk_label(&vkdt.ctx, vkdt.wstate.notification_msg, NK_TEXT_LEFT);
    }
    nk_end(&vkdt.ctx);
  }
  threads_mutex_unlock(&vkdt.wstate.notification_mutex);

  // nuklear requires the input, when modifying a text edit etc:
  vkdt.wstate.nk_active = vkdt.wstate.nk_active_next;
  vkdt.wstate.nk_active_next = 0;
}


void dt_gui_cleanup_nk()
{
  nk_buffer_free(&vkdt.global_buf);
  nk_free(&vkdt.ctx);
  render_nodes_cleanup();
  render_darkroom_cleanup();
  render_lighttable_cleanup();
  render_files_cleanup();
  QVKL(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[s_queue_graphics]].queue));
  nk_glfw3_shutdown();
}

void dt_gui_grab_mouse()
{
  glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  vkdt.wstate.grabbed = 1;
}

void dt_gui_ungrab_mouse()
{
  glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  vkdt.wstate.grabbed = 0;
  dt_gui_unset_fullscreen_view();
}

typedef struct dt_gamepadhelp_t
{
  int sp;
  const char *help[10][dt_gamepadhelp_cnt];
} dt_gamepadhelp_t;
static dt_gamepadhelp_t g_gamepadhelp = {0};

void dt_gamepadhelp_set(dt_gamepadhelp_input_t which, const char *str)
{
  if(which < 0 || which >= dt_gamepadhelp_cnt) return;
  g_gamepadhelp.help[g_gamepadhelp.sp][which] = str;
}
void dt_gamepadhelp_clear()
{
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
    g_gamepadhelp.help[g_gamepadhelp.sp][k] = 0;
}
void dt_gamepadhelp_push()
{
  int sp = 0;
  if(g_gamepadhelp.sp < 10) sp = ++g_gamepadhelp.sp;
  else return;
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
    g_gamepadhelp.help[sp][k] = g_gamepadhelp.help[sp-1][k];
}
void dt_gamepadhelp_pop()
{
  if(g_gamepadhelp.sp > 0) g_gamepadhelp.sp--;
}

void dt_gamepadhelp()
{
  GLFWgamepadstate gamepad = {0};
  if(vkdt.wstate.have_joystick) glfwGetGamepadState(vkdt.wstate.joystick_id, &gamepad);
  // static const char *dt_gamepadhelp_input_str[] = {
  //   "analog L", "analog R", "trigger L2", "trigger R2",
  //   "triangle", "circle", "cross", "square", "L1", "R1", // "∆", "o", "x", "□"
  //   "up", "left", "down", "right", // "↑", "←", "↓", "→",
  //   "start", "select", "ps", "L3", "R3",
  // };
  // const int map_axes[] = {GLFW_GAMEPAD_AXIS_LEFT_X, GLFW_GAMEPAD_
  const int map_button[] = { -1, -1, -1, -1,
    GLFW_GAMEPAD_BUTTON_Y, GLFW_GAMEPAD_BUTTON_B, GLFW_GAMEPAD_BUTTON_A, GLFW_GAMEPAD_BUTTON_X,
    GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
    GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
    -1, -1, -1, -1, -1};

  const float m[] = {
    vkdt.state.center_wd / 1200.0f, 0.0f, vkdt.state.center_wd * 0.30f,
    0.0f, vkdt.state.center_wd / 1200.0f, vkdt.state.center_ht * 0.50f,
  };
  struct nk_command_buffer *buf = nk_window_get_canvas(&vkdt.ctx);
  struct nk_color col = {0xff,0xff,0xff,0xff};
  dt_draw(&vkdt.ctx, dt_draw_list_gamepad, NK_LEN(dt_draw_list_gamepad), col, m);
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
  {
    if(g_gamepadhelp.help[g_gamepadhelp.sp][k])
    {
      struct nk_color c = col;
      if(k > 4 && map_button[k] >= 0 &&
         gamepad.buttons[map_button[k]])
        c = vkdt.style.colour[NK_COLOR_DT_ACCENT];
      if((k == 0 && fabsf(gamepad.axes[GLFW_GAMEPAD_AXIS_LEFT_X]) > 0.02) ||
         (k == 0 && fabsf(gamepad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) > 0.02) ||
         (k == 1 && fabsf(gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]) > 0.02) ||
         (k == 1 && fabsf(gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]) > 0.02) ||
         (k == 2 && gamepad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]  > -0.98) ||
         (k == 3 && gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > -0.98))
        c = vkdt.style.colour[NK_COLOR_DT_ACCENT];
      dt_draw(&vkdt.ctx, dt_draw_list_gamepad_arrow[k], NK_LEN(dt_draw_list_gamepad_arrow[k]), c, m);
      struct nk_vec2 v = {dt_draw_list_gamepad_arrow[k][8], dt_draw_list_gamepad_arrow[k][9]};
      struct nk_rect rect = {
        .x = v.x * m[3*0 + 0] + v.y * m[3*0 + 1] + m[3*0 + 2],
        .y = v.x * m[3*1 + 0] + v.y * m[3*1 + 1] + m[3*1 + 2],
      };
      rect.w = rect.x + 300;
      rect.h = rect.y + 300;
      rect.y -= 0.03*vkdt.state.center_ht;
      nk_draw_text(buf, rect, g_gamepadhelp.help[g_gamepadhelp.sp][k], strlen(g_gamepadhelp.help[g_gamepadhelp.sp][k]), nk_glfw3_font(1), nk_rgb(0,0,0), c);
    }
  }
}

int dt_gui_input_blocked()
{
  return vkdt.wstate.popup || vkdt.wstate.grabbed || vkdt.wstate.nk_active ||
    dt_radial_menu_dr_active(&vkdt.wstate.radial_menu_dr);
}
