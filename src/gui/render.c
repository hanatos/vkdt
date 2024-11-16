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
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static inline int
style_name_to_colour(const char *name)
{
  if(!strcmp(name, "TEXT"))                        return  NK_COLOR_TEXT;
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
  if(!strcmp(name, "DT_ACCENT"))                   return  NK_COLOR_DT_ACCENT;
  if(!strcmp(name, "DT_ACCENT_HOVER"))             return  NK_COLOR_DT_ACCENT_HOVER;
  if(!strcmp(name, "DT_ACCENT_ACTIVE"))            return  NK_COLOR_DT_ACCENT_ACTIVE;
  return -1;
}

static inline void
read_style_colours(struct nk_context *ctx)
{
  // set default colours
  nk_style_default(ctx);
  // see if we can find 
  FILE *f = 0;
  char filename[256];
  if(sizeof(filename) <= snprintf(filename, sizeof(filename), "%s/style.txt", dt_pipe.homedir)) return;
  f = fopen(filename, "rb");
  // global basedir
  if(!f)
  {
    if(sizeof(filename) <= snprintf(filename, sizeof(filename), "%s/style.txt", dt_pipe.basedir)) return;
    f = fopen(filename, "rb");
  }
  if(!f) return;
  while(!feof(f))
  {
    int rgba[4];
    char name[128];
    fscanf(f, "%127[^:]:%d:%d:%d:%d%*[^\n]", name, rgba, rgba+1, rgba+2, rgba+3);
    if(!name[0]) break;
    fgetc(f);
    int idx = style_name_to_colour(name);
    if(idx >= 0) vkdt.style.colour[idx] = nk_rgba(rgba[0], rgba[1], rgba[2], rgba[3]);
  }
  // init from this table
  nk_style_from_table(ctx, vkdt.style.colour);
}

static struct nk_font *g_font[4] = {0}; // remember to add an image sampler in gui.c if adding more fonts!
struct nk_font *dt_gui_get_font(int which)
{
  return g_font[which];
}

void dt_gui_init_fonts()
{
  char tmp[PATH_MAX+100] = {0};
  const float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
  float fontsize = MAX(5, floorf(vkdt.win.height / 55.0f * dpi_scale));
  const char *fontfile = dt_rc_get(&vkdt.rc, "gui/font", "Roboto-Regular.ttf");
  if(fontfile[0] != '/')
    snprintf(tmp, sizeof(tmp), "%s/data/%s", dt_pipe.basedir, fontfile);
  else
    snprintf(tmp, sizeof(tmp), "%s", fontfile);

  struct nk_font_atlas *atlas;
  if(g_font[0]) nk_glfw3_font_cleanup();
  nk_glfw3_font_stash_begin(&atlas);
  struct nk_font_config cfg = nk_font_config(fontsize);
  cfg.oversample_h = 5;
  cfg.oversample_v = 5;
  g_font[0] = nk_font_atlas_add_from_file(atlas, tmp, fontsize, &cfg);
  g_font[1] = nk_font_atlas_add_from_file(atlas, tmp, floorf(1.5*fontsize), 0);
  g_font[2] = nk_font_atlas_add_from_file(atlas, tmp, 2*fontsize, 0);
  snprintf(tmp, sizeof(tmp), "%s/data/MaterialIcons-Regular.ttf", dt_pipe.basedir);
  cfg.oversample_h = 3;
  cfg.oversample_v = 1;
  static nk_rune ranges_icons[] = {
    0xe01f, 0xe048, // play pause
    0xe15b, 0xe15c, // stuff for disabled button
    0xe5cc, 0xe5d0, // expander e5cc e5cf
    0xe612, 0xe613,
    0xe836, 0xe839, // star in material icons
    0,
  };
  cfg.range = ranges_icons;
  cfg.size = fontsize;
  cfg.fallback_glyph = 0xe15b;
  g_font[3] = nk_font_atlas_add_from_file(atlas, tmp, fontsize, &cfg);

  threads_mutex_lock(&qvk.queue[qvk.qid[s_queue_graphics]].mutex);
  nk_glfw3_font_stash_end(&vkdt.ctx, vkdt.win.command_buffer[vkdt.win.frame_index%DT_GUI_MAX_IMAGES], qvk.queue[qvk.qid[s_queue_graphics]].queue);
  threads_mutex_unlock(&qvk.queue[qvk.qid[s_queue_graphics]].mutex);
  /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
  /*nk_style_set_font(ctx, &droid->handle);*/
  nk_style_set_font(&vkdt.ctx, &g_font[0]->handle);
}

int dt_gui_init_nk()
{
  vkdt.wstate.lod = dt_rc_get_int(&vkdt.rc, "gui/lod", 1); // set finest lod by default

  nk_init_default(&vkdt.ctx, 0);
  nk_glfw3_init(
      &vkdt.ctx,
      vkdt.win.render_pass,
      vkdt.win.window,
      qvk.device, qvk.physical_device,
      vkdt.win.num_swap_chain_images * 2560*1024,
      vkdt.win.num_swap_chain_images * 640*1024);

  read_style_colours(&vkdt.ctx);

  nk_buffer_init_default(&vkdt.global_buf);
  nk_command_buffer_init(&vkdt.global_cmd, &vkdt.global_buf, NK_CLIPPING_OFF);

  char tmp[PATH_MAX+100] = {0};
  {
    int monitors_cnt;
    GLFWmonitor** monitors = glfwGetMonitors(&monitors_cnt);
    if(monitors_cnt > 2)
      dt_log(s_log_gui, "you have more than 2 monitors attached! only the first two will be colour managed!");
    const char *name0 = glfwGetMonitorName(monitors[0]);
    const char *name1 = glfwGetMonitorName(monitors[MIN(monitors_cnt-1, 1)]);
    int xpos0, xpos1, ypos;
    glfwGetMonitorPos(monitors[0], &xpos0, &ypos);
    glfwGetMonitorPos(monitors[MIN(monitors_cnt-1, 1)], &xpos1, &ypos);
    float gamma0[] = {1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f};
    float rec2020_to_dspy0[] = { // to linear sRGB D65
       1.66022709, -0.58754775, -0.07283832,
      -0.12455356,  1.13292608, -0.0083496,
      -0.01815511, -0.100603  ,  1.11899813 };
    float gamma1[] = {1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f};
    float rec2020_to_dspy1[] = { // to linear sRGB D65
       1.66022709, -0.58754775, -0.07283832,
      -0.12455356,  1.13292608, -0.0083496,
      -0.01815511, -0.100603  ,  1.11899813 };
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.homedir, name0);
    FILE *f = fopen(tmp, "r");
    if(!f)
    {
      snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.basedir, name0);
      f = fopen(tmp, "r");
    }
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
      snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.homedir, name1);
      f = fopen(tmp, "r");
      if(!f)
      {
        snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.basedir, name1);
        f = fopen(tmp, "r");
      }
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
    int bitdepth = 8; // the display output will be dithered according to this
    if(vkdt.win.surf_format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
       vkdt.win.surf_format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      bitdepth = 10;
    nk_glfw3_setup_display_colour_management(gamma0, rec2020_to_dspy0, gamma1, rec2020_to_dspy1, xpos1, bitdepth);
  }

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
  dt_gui_dr_unset_fullscreen_view();
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
  // static const char *dt_gamepadhelp_input_str[] = {
  //   "analog L", "analog R", "trigger L2", "trigger R2",
  //   "triangle", "circle", "cross", "square", "L1", "R1", // "∆", "o", "x", "□"
  //   "up", "left", "down", "right", // "↑", "←", "↓", "→",
  //   "start", "select", "ps", "L3", "R3",
  // };

  const float m[] = {
    vkdt.state.center_wd / 1200.0f, 0.0f, vkdt.state.center_wd * 0.30f,
    0.0f, vkdt.state.center_wd / 1200.0f, vkdt.state.center_ht * 0.50f,
  };
  struct nk_command_buffer *buf = nk_window_get_canvas(&vkdt.ctx);
  dt_draw(&vkdt.ctx, dt_draw_list_gamepad, NK_LEN(dt_draw_list_gamepad), m);
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
  {
    if(g_gamepadhelp.help[g_gamepadhelp.sp][k])
    {
      dt_draw(&vkdt.ctx, dt_draw_list_gamepad_arrow[k], NK_LEN(dt_draw_list_gamepad_arrow[k]), m);
      struct nk_vec2 v = {dt_draw_list_gamepad_arrow[k][8], dt_draw_list_gamepad_arrow[k][9]};
      struct nk_rect rect = {
        .x = v.x * m[3*0 + 0] + v.y * m[3*0 + 1] + m[3*0 + 2],
        .y = v.x * m[3*1 + 0] + v.y * m[3*1 + 1] + m[3*1 + 2],
      };
      rect.w = rect.x + 300;
      rect.h = rect.y + 300;
      rect.y -= 0.03*vkdt.state.center_ht;
      nk_draw_text(buf, rect, g_gamepadhelp.help[g_gamepadhelp.sp][k], strlen(g_gamepadhelp.help[g_gamepadhelp.sp][k]), &dt_gui_get_font(1)->handle, nk_rgb(0,0,0), nk_rgb(255,255,255));
      // rect.y -= 0.03*vkdt.state.center_ht;
      // nk_draw_text(buf, rect, dt_gamepadhelp_input_str[k], strlen(dt_gamepadhelp_input_str[k]), &dt_gui_get_font(1)->handle, nk_rgb(0,0,0), nk_rgb(255,255,255));
    }
  }
}
