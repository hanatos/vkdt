#define NK_IMPLEMENTATION
#define NK_GLFW_VULKAN_IMPLEMENTATION
#include "nk.h"
#include "gui.h"
#include "view.h"
#include "core/log.h"
#include "qvk/qvk.h"
#include "qvk/sub.h"
#include "pipe/graph-export.h"
#include "pipe/graph-io.h"
#include "pipe/graph-history.h"
#include "api.h"
#include "render.h"
#include "widget_draw.h"
#include "render_view.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

enum theme {THEME_BLACK, THEME_WHITE, THEME_RED, THEME_BLUE, THEME_DARK};

static void
set_style(struct nk_context *ctx, enum theme theme)
{
    struct nk_color table[NK_COLOR_COUNT];
    if (theme == THEME_WHITE) {
        table[NK_COLOR_TEXT] = nk_rgba(70, 70, 70, 255);
        table[NK_COLOR_WINDOW] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_HEADER] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_BORDER] = nk_rgba(0, 0, 0, 255);
        table[NK_COLOR_BUTTON] = nk_rgba(185, 185, 185, 255);
        table[NK_COLOR_BUTTON_HOVER] = nk_rgba(170, 170, 170, 255);
        table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(160, 160, 160, 255);
        table[NK_COLOR_TOGGLE] = nk_rgba(150, 150, 150, 255);
        table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(120, 120, 120, 255);
        table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_SELECT] = nk_rgba(190, 190, 190, 255);
        table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_SLIDER] = nk_rgba(190, 190, 190, 255);
        table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(80, 80, 80, 255);
        table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(70, 70, 70, 255);
        table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(60, 60, 60, 255);
        table[NK_COLOR_PROPERTY] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_EDIT] = nk_rgba(150, 150, 150, 255);
        table[NK_COLOR_EDIT_CURSOR] = nk_rgba(0, 0, 0, 255);
        table[NK_COLOR_COMBO] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_CHART] = nk_rgba(160, 160, 160, 255);
        table[NK_COLOR_CHART_COLOR] = nk_rgba(45, 45, 45, 255);
        table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
        table[NK_COLOR_SCROLLBAR] = nk_rgba(180, 180, 180, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(140, 140, 140, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(150, 150, 150, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(160, 160, 160, 255);
        table[NK_COLOR_TAB_HEADER] = nk_rgba(180, 180, 180, 255);
        nk_style_from_table(ctx, table);
    } else if (theme == THEME_RED) {
        table[NK_COLOR_TEXT] = nk_rgba(190, 190, 190, 255);
        table[NK_COLOR_WINDOW] = nk_rgba(30, 33, 40, 215);
        table[NK_COLOR_HEADER] = nk_rgba(181, 45, 69, 220);
        table[NK_COLOR_BORDER] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_BUTTON] = nk_rgba(181, 45, 69, 255);
        table[NK_COLOR_BUTTON_HOVER] = nk_rgba(190, 50, 70, 255);
        table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(195, 55, 75, 255);
        table[NK_COLOR_TOGGLE] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 60, 60, 255);
        table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(181, 45, 69, 255);
        table[NK_COLOR_SELECT] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(181, 45, 69, 255);
        table[NK_COLOR_SLIDER] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(181, 45, 69, 255);
        table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(186, 50, 74, 255);
        table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(191, 55, 79, 255);
        table[NK_COLOR_PROPERTY] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_EDIT] = nk_rgba(51, 55, 67, 225);
        table[NK_COLOR_EDIT_CURSOR] = nk_rgba(190, 190, 190, 255);
        table[NK_COLOR_COMBO] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_CHART] = nk_rgba(51, 55, 67, 255);
        table[NK_COLOR_CHART_COLOR] = nk_rgba(170, 40, 60, 255);
        table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
        table[NK_COLOR_SCROLLBAR] = nk_rgba(30, 33, 40, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
        table[NK_COLOR_TAB_HEADER] = nk_rgba(181, 45, 69, 220);
        nk_style_from_table(ctx, table);
    } else if (theme == THEME_BLUE) {
        table[NK_COLOR_TEXT] = nk_rgba(20, 20, 20, 255);
        table[NK_COLOR_WINDOW] = nk_rgba(202, 212, 214, 215);
        table[NK_COLOR_HEADER] = nk_rgba(137, 182, 224, 220);
        table[NK_COLOR_BORDER] = nk_rgba(140, 159, 173, 255);
        table[NK_COLOR_BUTTON] = nk_rgba(137, 182, 224, 255);
        table[NK_COLOR_BUTTON_HOVER] = nk_rgba(142, 187, 229, 255);
        table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(147, 192, 234, 255);
        table[NK_COLOR_TOGGLE] = nk_rgba(177, 210, 210, 255);
        table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(182, 215, 215, 255);
        table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(137, 182, 224, 255);
        table[NK_COLOR_SELECT] = nk_rgba(177, 210, 210, 255);
        table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(137, 182, 224, 255);
        table[NK_COLOR_SLIDER] = nk_rgba(177, 210, 210, 255);
        table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(137, 182, 224, 245);
        table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(142, 188, 229, 255);
        table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(147, 193, 234, 255);
        table[NK_COLOR_PROPERTY] = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_EDIT] = nk_rgba(210, 210, 210, 225);
        table[NK_COLOR_EDIT_CURSOR] = nk_rgba(20, 20, 20, 255);
        table[NK_COLOR_COMBO] = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_CHART] = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_CHART_COLOR] = nk_rgba(137, 182, 224, 255);
        table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
        table[NK_COLOR_SCROLLBAR] = nk_rgba(190, 200, 200, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
        table[NK_COLOR_TAB_HEADER] = nk_rgba(156, 193, 220, 255);
        nk_style_from_table(ctx, table);
    } else if (theme == THEME_DARK) {
        table[NK_COLOR_TEXT] = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 215);
        table[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
        table[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 255);
        table[NK_COLOR_BUTTON] = nk_rgba(48, 83, 111, 255);
        table[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
        table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
        table[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
        table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
        table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
        table[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
        table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
        table[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
        table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
        table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
        table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
        table[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
        table[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
        table[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
        table[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
        table[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
        table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
        table[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
        table[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
        nk_style_from_table(ctx, table);
    } else {
        nk_style_default(ctx);
    }
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
  float fontsize = floorf(qvk.win_height / 55.0f * dpi_scale);
  const char *fontfile = dt_rc_get(&vkdt.rc, "gui/font", "Roboto-Regular.ttf");
  if(fontfile[0] != '/')
    snprintf(tmp, sizeof(tmp), "%s/data/%s", dt_pipe.basedir, fontfile);
  else
    snprintf(tmp, sizeof(tmp), "%s", fontfile);

  struct nk_font_atlas *atlas;
  nk_glfw3_font_stash_begin(&atlas);
  // TODO: last parameter is glyph ranges, maybe keep from imgui version?
  g_font[0] = nk_font_atlas_add_from_file(atlas, tmp, fontsize, 0);
  g_font[1] = nk_font_atlas_add_from_file(atlas, tmp, floorf(1.5*fontsize), 0);
  g_font[2] = nk_font_atlas_add_from_file(atlas, tmp, 2*fontsize, 0);
  snprintf(tmp, sizeof(tmp), "%s/data/MaterialIcons-Regular.ttf", dt_pipe.basedir);
  g_font[3] = nk_font_atlas_add_from_file(atlas, tmp, fontsize, 0);
  nk_glfw3_font_stash_end(vkdt.command_buffer[vkdt.frame_index%DT_GUI_MAX_IMAGES], qvk.queue_graphics);
  /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
  /*nk_style_set_font(ctx, &droid->handle);*/
}


int dt_gui_init_nk()
{
  vkdt.wstate.lod = dt_rc_get_int(&vkdt.rc, "gui/lod", 1); // set finest lod by default

  nk_init_default(&vkdt.ctx, 0);
  // nk_init_default(&vkdt.ctx1, 0); // TODO secondary screen
  nk_glfw3_init(
      &vkdt.ctx,
      qvk.window,
      qvk.device, qvk.physical_device,
      512*1024, 128*1024);

     // XXX setup keyboard and gamepad nav!
     // https://github.com/smallbasic/smallbasic.plugins/blob/master/nuklear/nkbd.h
     // XXX setup multi viewport for dual screen! (requires a second ctx and manual handling of the second viewport/command buffer)
     // XXX setup colour management! (use our imgui shaders and port to nuklear)

  set_style(&vkdt.ctx, THEME_DARK); 

#if 0 // TODO: see above init call and put things
  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForVulkan(qvk.window, false); // don't install callbacks
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance         = qvk.instance;
  init_info.PhysicalDevice   = qvk.physical_device;
  init_info.Device           = qvk.device;
  init_info.QueueFamily      = qvk.queue_idx_graphics;
  init_info.Queue            = qvk.queue_graphics;
  init_info.PipelineCache    = vkdt.pipeline_cache;
  init_info.DescriptorPool   = vkdt.descriptor_pool;
  init_info.Allocator        = 0;
  init_info.MinImageCount    = vkdt.min_image_count;
  init_info.ImageCount       = vkdt.image_count;
  init_info.CheckVkResultFn  = 0;//check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, vkdt.render_pass);
#endif

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
      fscanf(f, "%f %f %f\n", gamma0, gamma0+1, gamma0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+0, rec2020_to_dspy0+1, rec2020_to_dspy0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+3, rec2020_to_dspy0+4, rec2020_to_dspy0+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+6, rec2020_to_dspy0+7, rec2020_to_dspy0+8);
      fclose(f);
    }
    else dt_log(s_log_gui, "no display profile file display.%s, using sRGB!", name0);
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.homedir, name1);
    f = fopen(tmp, "r");
    if(!f)
    {
      snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.basedir, name1);
      f = fopen(tmp, "r");
    }
    if(f)
    {
      fscanf(f, "%f %f %f\n", gamma1, gamma1+1, gamma1+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+0, rec2020_to_dspy1+1, rec2020_to_dspy1+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+3, rec2020_to_dspy1+4, rec2020_to_dspy1+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+6, rec2020_to_dspy1+7, rec2020_to_dspy1+8);
      fclose(f);
    }
    else dt_log(s_log_gui, "no display profile file display.%s, using sRGB!", name1);
#if 0
    int bitdepth = 8; // the display output will be dithered according to this
    if(qvk.surf_format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
       qvk.surf_format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      bitdepth = 10;
    // XXX ImGui_ImplVulkan_SetDisplayProfile(gamma0, rec2020_to_dspy0, gamma1, rec2020_to_dspy1, xpos1, bitdepth);
    // TODO: store in common place in nk context or our own
#endif
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
  // render_darkroom_init();
  // render_nodes_init();
  return 0;
}

// call from main loop:
// XXX TODO call this from dt_gui_render after acquiring the image, or factor out the call to nk_glfw3_render!
void dt_gui_render_frame_nk()
{
  nk_glfw3_new_frame();
  double now = glfwGetTime();

  static double button_pressed_time = 0.0;
  if(now - button_pressed_time > 0.1)
  { // the glfw backend does not support the "ps" button, so we do the stupid thing:
    int buttons_count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
    if(buttons && buttons[10]) vkdt.wstate.show_gamepadhelp ^= 1;
    button_pressed_time = now;
  }

  switch(vkdt.view_mode)
  {
    // case s_view_files:
      // render_files();
      // break;
    case s_view_lighttable:
      render_lighttable();
      break;
    // case s_view_darkroom:
      // render_darkroom();
      // break;
    // case s_view_nodes:
      // render_nodes();
      // break;
    default:;
  }

  // draw notification message
  threads_mutex_lock(&vkdt.wstate.notification_mutex);
  if(vkdt.wstate.notification_msg[0] &&
     now - vkdt.wstate.notification_time < 4.0)
  {
    if (nk_begin(&vkdt.ctx, "notification message",
          nk_rect(vkdt.state.center_x, vkdt.state.center_y/2, vkdt.state.center_wd, 0.05*vkdt.state.center_ht),
          NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
      // TODO yeah no essentially none of the above please :)
    {
      nk_label(&vkdt.ctx, vkdt.wstate.notification_msg, NK_TEXT_LEFT);
    }
    nk_end(&vkdt.ctx);
  }
  threads_mutex_unlock(&vkdt.wstate.notification_mutex);

  // TODO: now render second screen context, if any (this will only be a full res image widget, we could put the code here)
}

void dt_gui_record_command_buffer_nk(VkCommandBuffer cmd_buf)
{
  nk_glfw3_create_cmd(&vkdt.ctx, cmd_buf, NK_ANTI_ALIASING_ON);
}

void dt_gui_cleanup_nk()
{
  nk_free(&vkdt.ctx);
  // render_nodes_cleanup();
  // render_darkroom_cleanup();
  render_lighttable_cleanup();
  threads_mutex_lock(&qvk.queue_mutex);
  vkDeviceWaitIdle(qvk.device);
  threads_mutex_unlock(&qvk.queue_mutex);
  nk_glfw3_shutdown();
}

int dt_gui_nk_want_keyboard()
{
  // XXX caveat: only works after drawing the full frame. i.e. keyboard accelerators should be evaluated at the very end!
  return nk_item_is_any_active(&vkdt.ctx);
}

void dt_gui_grab_mouse()
{
  // TODO vkdt.ctx.input.mouse.grab (then takes care of cursor too)
  // glfwSetInputMode(qvk.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  // XXX need this mirror?
  vkdt.wstate.grabbed = 1;
}

void dt_gui_ungrab_mouse()
{
  // glfwSetInputMode(qvk.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  // XXX need this mirror?
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
  static const char *dt_gamepadhelp_input_str[] = {
    "analog L", "analog R", "trigger L2", "trigger R2",
    "triangle", "circle", "cross", "square", "L1", "R1", // "∆", "o", "x", "□"
    "up", "left", "down", "right", // "↑", "←", "↓", "→",
    "start", "select", "ps", "L3", "R3",
  };

  const float m[] = {
    vkdt.state.center_wd / 1200.0f, 0.0f, vkdt.state.center_wd * 0.30f,
    0.0f, vkdt.state.center_wd / 1200.0f, vkdt.state.center_ht * 0.50f,
  };
  dt_draw(&vkdt.ctx, dt_draw_list_gamepad, sizeof(dt_draw_list_gamepad)/sizeof(dt_draw_list_gamepad[0]), m);
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
  {
    if(g_gamepadhelp.help[g_gamepadhelp.sp][k])
    {
      dt_draw(&vkdt.ctx, dt_draw_list_gamepad_arrow[k], sizeof(dt_draw_list_gamepad_arrow[k])/sizeof(dt_draw_list_gamepad_arrow[k][0]), m);
      // XXX FIXME:
      // nk_draw_text(buf, rect, *text, len, nk_user_font, nk_col bg, nu_col fg)
#if 0
      ImVec2 v = ImVec2(dt_draw_list_gamepad_arrow[k][8], dt_draw_list_gamepad_arrow[k][9]);
      ImVec2 pos = ImVec2(
          v.x * m[3*0 + 0] + v.y * m[3*0 + 1] + m[3*0 + 2],
          v.x * m[3*1 + 0] + v.y * m[3*1 + 1] + m[3*1 + 2]);
      pos.y -= vkdt.wstate.fontsize * 1.1;
      ImGui::GetWindowDrawList()->AddText(pos, IM_COL32_WHITE, dt_gamepadhelp_input_str[k]);
      pos.y += vkdt.wstate.fontsize * 1.1;
      ImGui::GetWindowDrawList()->AddText(pos, IM_COL32_WHITE, g_gamepadhelp.help[g_gamepadhelp.sp][k]);
#endif
    }
  }
}
