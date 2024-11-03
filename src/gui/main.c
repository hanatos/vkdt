#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/global.h"
#include "pipe/modules/api.h"
#include "db/thumbnails.h"
#include "core/log.h"
#include "core/signal.h"
#include "core/version.h"
#include "core/tools.h"
#include "gui/gui.h"
#include "gui/render.h"
#include "gui/view.h"
#include "db/db.h"
#include "nk.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

dt_gui_t vkdt = {0};

int g_fullscreen = 0;

// from a stackoverflow answer. get the monitor that currently covers most of
// the window area.
static GLFWmonitor*
get_current_monitor(GLFWwindow *window)
{
  int bestoverlap = 0;
  GLFWmonitor *bestmonitor = NULL;

  int wx, wy, ww, wh;
  glfwGetWindowPos(window, &wx, &wy);
  glfwGetFramebufferSize(window, &ww, &wh);

  int nmonitors;
  GLFWmonitor **monitors = glfwGetMonitors(&nmonitors);

  for (int i = 0; i < nmonitors; i++)
  {
    const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
    int mx, my;
    glfwGetMonitorPos(monitors[i], &mx, &my);
    int mw = mode->width;
    int mh = mode->height;

    int overlap =
      MAX(0, MIN(wx + ww, mx + mw) - MAX(wx, mx)) *
      MAX(0, MIN(wy + wh, my + mh) - MAX(wy, my));

    if (bestoverlap < overlap)
    {
      bestoverlap = overlap;
      bestmonitor = monitors[i];
    }
  }
  return bestmonitor;
}

static inline int
gamepad_changed(
    GLFWgamepadstate *last,
    GLFWgamepadstate *curr)
{
  for(int i=0;i<sizeof(curr->buttons)/sizeof(curr->buttons[0]);i++)
    if(curr->buttons[i] != last->buttons[i])
      return 1;
  for(int i=0;i<sizeof(curr->axes)/sizeof(curr->axes[0]);i++)
    if(fabsf(curr->axes[i] - last->axes[i]) > 0.1)
      return 1;
  return 0;
}

// since in glfw, joysticks can only be polled and have no event interface
// (see this pull request: https://github.com/glfw/glfw/pull/1590)
// we need to look for changes in a busy loop in this dedicated thread.
// once we find activity, we'll step outside the glfwWaitEvents call by
// posting an empty event from here.
// note that the actual joystick callbacks will be handled there, in
// the gui thread. here, we only detect change and raise the flag.
static void*
joystick_active(void *unused)
{
  static GLFWgamepadstate last = {0};
  GLFWgamepadstate curr;
  while(!glfwWindowShouldClose(vkdt.win.window))
  {
    if(!glfwGetGamepadState(GLFW_JOYSTICK_1, &curr)) break; // no more joystick?
    if(gamepad_changed(&last, &curr))
    {
      last = curr;
      vkdt.wstate.busy = 20; // make sure we'll stay awake for a few frames
      glfwPostEmptyEvent();
    }
    struct timespec req = { .tv_sec = 0, .tv_nsec = 16000 }, rem;
    nanosleep(&req, &rem);
  }
  return 0;
}

static void
toggle_fullscreen()
{
  GLFWmonitor* monitor = get_current_monitor(vkdt.win.window);
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  if(g_fullscreen)
  {
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowMonitor(vkdt.win.window, 0, mode->width/8, mode->height/8, mode->width/4 * 3, mode->height/4 * 3, mode->refreshRate);
    g_fullscreen = 0;
  }
  else
  {
    glfwSetWindowMonitor(vkdt.win.window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    g_fullscreen = 1;
  }
  dt_gui_recreate_swapchain(&vkdt.win);
  dt_gui_init_fonts();
}

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  dt_view_keyboard(window, key, scancode, action, mods);
  if(!vkdt.wstate.grabbed)
    nk_glfw3_keyboard_callback(&vkdt.ctx, window, key, scancode, action, mods);

  if(key == GLFW_KEY_ESCAPE) // TODO: or gamepad equivalent
  {
    if(vkdt.wstate.popup) vkdt.wstate.popup = 0; // close any popup
  }
  else if(key == GLFW_KEY_X && action == GLFW_PRESS && mods == GLFW_MOD_CONTROL)
  {
    glfwSetWindowShouldClose(vkdt.win.window, GLFW_TRUE);
  }
  else if(key == GLFW_KEY_F11 && action == GLFW_PRESS)
  {
    toggle_fullscreen();
  }
}

static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  dt_view_mouse_button(window, button, action, mods);
  if(!vkdt.wstate.grabbed)
    nk_glfw3_mouse_button_callback(window, button, action, mods);
}

static void
mouse_position_callback(GLFWwindow* window, double x, double y)
{
  float xscale, yscale;
  dt_gui_content_scale(window, &xscale, &yscale);
  dt_view_mouse_position(window, x*xscale, y*yscale);
}

static void
window_close_callback(GLFWwindow* window)
{
  glfwSetWindowShouldClose(vkdt.win.window, GLFW_TRUE);
}

static void
char_callback(GLFWwindow* window, unsigned int c)
{
  if(!vkdt.wstate.grabbed)
    nk_glfw3_char_callback(window, c);
}

static void
scroll_callback(GLFWwindow *window, double xoff, double yoff)
{
  float xscale, yscale;
  dt_gui_content_scale(window, &xscale, &yscale);
  xoff *= xscale;
  yoff *= yscale;
  dt_view_mouse_scrolled(window, xoff, yoff);
  if(!vkdt.wstate.grabbed)
    nk_glfw3_scroll_callback(window, xoff, yoff);
}

#if VKDT_USE_PENTABLET==1
static void
pentablet_data_callback(double x, double y, double z, double pressure, double pitch, double yaw, double roll)
{
  float xscale, yscale;
  dt_gui_content_scale(vkdt.win.window, &xscale, &yscale);
  x *= xscale; y *= yscale;
  dt_view_pentablet_data(x, y, z, pressure, pitch, yaw, roll);
}

static void
pentablet_proximity_callback(int entering)
{
  dt_view_pentablet_proximity(entering);
}

static void
pentablet_cursor_callback(unsigned int cursor)
{ }
#endif

static void
dt_gui_print_usage()
{
  printf("gui usage: vkdt [options] [file | directory]\n"
         "  options:\n"
         "       -d <log layer>   switch on log layer\n"
         "       -D <log layer>   mute log layer\n"
         "  log layers:\n"
         "  none qvk pipe gui db cli snd perf mem err all\n");
}

int main(int argc, char *argv[])
{
  if(argc > 1)
  {
    if(!strcmp(argv[1], "--version"))
    {
      printf("vkdt "VKDT_VERSION" (c) 2020--2024 johannes hanika\n");
      exit(0);
    }
    else if(!strcmp(argv[1], "--help"))
    {
      printf("vkdt "VKDT_VERSION" (c) 2020--2024 johannes hanika\n");
      dt_tool_print_usage();
      dt_gui_print_usage();
      exit(0);
    }
    dt_tool_dispatch(argc, argv);
  }
  // init global things, log and pipeline:
  dt_log_init(s_log_err|s_log_gui);
  int lastarg = dt_log_init_arg(argc, argv);
  dt_log(s_log_gui, "vkdt "VKDT_VERSION" (c) 2020--2024 johannes hanika");
  dt_pipe_global_init();
  threads_global_init();
  dt_set_signal_handlers();

  if(dt_gui_init())
  {
    dt_log(s_log_gui|s_log_err, "failed to init gui/swapchain");
    exit(1);
  }

  // start un-fullscreen on current monitor, we only know which one is that after
  // we created the window in dt_gui_init(). maybe should be a config option:
  g_fullscreen = 1;
  toggle_fullscreen();

  glfwSetKeyCallback(vkdt.win.window, key_callback);
  glfwSetMouseButtonCallback(vkdt.win.window, mouse_button_callback);
  glfwSetCursorPosCallback(vkdt.win.window, mouse_position_callback);
  glfwSetCharCallback(vkdt.win.window, char_callback);
  glfwSetScrollCallback(vkdt.win.window, scroll_callback);
  glfwSetWindowCloseCallback(vkdt.win.window, window_close_callback);
#if VKDT_USE_PENTABLET==1
  glfwSetPenTabletDataCallback(pentablet_data_callback);
  glfwSetPenTabletCursorCallback(pentablet_cursor_callback);
  glfwSetPenTabletProximityCallback(pentablet_proximity_callback);
#endif

  vkdt.view_mode = s_view_cnt;
  // this is done so we can mess with the thumbnails struct in
  // an asynchronous manner in a background thread. it will know
  // it is not serving thumbnails but it only writes bc1 to disk.
  // also we have a temporary thumbnails struct and background threads
  // to create thumbnails, if necessary.
  // only width/height will matter here
  dt_thumbnails_init(&vkdt.thumbnail_gen, 400, 400, 0, 0);
  dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3000, 1ul<<30);
  dt_db_init(&vkdt.db);
  char *filename = 0;
  {
    char defpath[1024];
    const char *mru = dt_rc_get(&vkdt.rc, "gui/ruc_entry00", "null");
    if(strcmp(mru, "null"))
    {
      snprintf(defpath, sizeof(defpath), "%s", mru);
      for(int i=0;i<sizeof(defpath) && defpath[i];i++) if(defpath[i] == '&') { defpath[i] = 0; break; }
    }
    else fs_picturesdir(defpath, sizeof(defpath));
    if(argc > lastarg+1) filename = fs_realpath(argv[lastarg+1], 0);
    else                 filename = fs_realpath(defpath, 0);
  }
  if(!filename || fs_isdir_file(filename))
  {
    vkdt.view_mode = s_view_lighttable;
    dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, filename);
    dt_view_switch(s_view_lighttable);
    dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
  }
  else
  {
    if(dt_db_load_image(&vkdt.db, &vkdt.thumbnails, filename))
    {
      dt_log(s_log_err, "image `%s' could not be loaded!", filename);
      dt_gui_notification("image `%s' could not be loaded!", filename);
      dt_view_switch(s_view_files);
    }
    else
    {
      dt_db_selection_add(&vkdt.db, 0);
      dt_view_switch(s_view_darkroom);
    }
  }
  dt_gui_read_tags();

  // joystick
  pthread_t joystick_thread;
  const int joystick_present = vkdt.wstate.have_joystick;
  if(joystick_present) pthread_create(&joystick_thread, 0, joystick_active, 0);

  // main loop
  double beg_rf = dt_time();
  const int frame_limiter = dt_rc_get_int(&vkdt.rc, "gui/frame_limiter", 6); // default: cap ui at 160fps
  vkdt.wstate.busy = 3;
  vkdt.graph_dev.frame = vkdt.state.anim_frame = 0;
  GLFWgamepadstate gamepad_last = {0};
  while(!glfwWindowShouldClose(vkdt.win.window))
  {
    // block and wait for one event instead of polling all the time to save on
    // gpu workload. might need an interrupt for "render finished" etc. we might
    // do that via glfwPostEmptyEvent()
    if(vkdt.wstate.busy > 0) vkdt.wstate.busy--;
    // vkdt.wstate.busy = 100; // do these two lines instead if profiling in nvidia gfx insight or so.
    // vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    if(vkdt.state.anim_playing) // should redraw because animation is playing?
      vkdt.wstate.busy = vkdt.state.anim_max_frame == -1 ? 3 : vkdt.state.anim_max_frame - vkdt.state.anim_frame + 1;
    if(vkdt.wstate.busy > 0) glfwPostEmptyEvent();
    else vkdt.wstate.busy = 3;

    glfwWaitEvents();

    if(vkdt.wstate.have_joystick)
    {
      GLFWgamepadstate gamepad_curr;
      if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepad_curr)) vkdt.wstate.have_joystick = 0;
      else if(gamepad_changed(&gamepad_last, &gamepad_curr))
      {
        dt_view_gamepad(vkdt.win.window, &gamepad_last, &gamepad_curr);
        gamepad_last = gamepad_curr;
      }
    }

    if(frame_limiter || (dt_log_global.mask & s_log_perf))
    { // artificially limit frames rate to frame_limiter milliseconds/frame as minimum.
      double end_rf = dt_time();
      if(frame_limiter && end_rf - beg_rf < frame_limiter / 1000.0)
      {
        usleep(frame_limiter * 1000);
        continue;
      }
      dt_log(s_log_perf, "fps %.2g", 1.0/(end_rf - beg_rf));
      beg_rf = end_rf;
    }

    dt_view_process(); // process before render/preset because this might swap the output image backbuffers
    if(vkdt.graph_dev.gui_msg && vkdt.graph_dev.gui_msg[0]) dt_gui_notification(vkdt.graph_dev.gui_msg);

    if(dt_gui_render() == VK_SUCCESS)
      dt_gui_present();
    else
      dt_gui_recreate_swapchain(&vkdt.win);
  }
  if(joystick_present) pthread_join(joystick_thread, 0);

  for(int q=0;q<s_queue_cnt;q++)
    if(qvk.qid[q] == q)
      QVKL(&qvk.queue[qvk.qid[q]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[q]].queue));

  // leave whatever view we're still in:
  dt_view_switch(s_view_cnt);

  threads_shutdown();
  threads_global_cleanup(); // join worker threads before killing their resources
  dt_thumbnails_cleanup(&vkdt.thumbnails);
  dt_thumbnails_cleanup(&vkdt.thumbnail_gen);
  dt_gui_cleanup();
  dt_db_cleanup(&vkdt.db);
  dt_pipe_global_cleanup();
  free(filename);
  exit(0);
}
