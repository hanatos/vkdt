#include "qvk/qvk.h"

#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/global.h"
#include "pipe/modules/api.h"
#include "db/thumbnails.h"
#include "core/log.h"
#include "gui/gui.h"
#include "gui/render.h"
#include "gui/view.h"
#include "db/db.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

dt_gui_t vkdt = {0};

int g_running = 1;
int g_busy = 3;
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
  glfwGetWindowSize(window, &ww, &wh);
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

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  dt_view_keyboard(window, key, scancode, action, mods);
  dt_gui_imgui_keyboard(window, key, scancode, action, mods);

  if(key == GLFW_KEY_X && action == GLFW_PRESS && mods == GLFW_MOD_CONTROL)
  {
    g_running = 0;
  }
  else if(key == GLFW_KEY_F11 && action == GLFW_PRESS)
  {
    GLFWmonitor* monitor = get_current_monitor(qvk.window);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if(g_fullscreen)
    {
      glfwSetWindowPos(qvk.window, mode->width/8, mode->height/8);
      glfwSetWindowSize(qvk.window, mode->width/4 * 3, mode->height/4 * 3);
      g_fullscreen = 0;
    }
    else
    {
      glfwSetWindowPos(qvk.window, 0, 0);
      glfwSetWindowSize(qvk.window, mode->width, mode->height);
      g_fullscreen = 1;
    }
    dt_gui_recreate_swapchain();
  }
}

static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  dt_view_mouse_button(window, button, action, mods);
  dt_gui_imgui_mouse_button(window, button, action, mods);
}

static void
mouse_position_callback(GLFWwindow* window, double x, double y)
{
  dt_view_mouse_position(window, x, y);
}

static void
window_size_callback(GLFWwindow* window, int width, int height)
{
  // window resized, need to rebuild our swapchain:
  dt_gui_recreate_swapchain();
}

static void
window_pos_callback(GLFWwindow* window, int xpos, int ypos)
{
  dt_gui_imgui_window_position(window, xpos, ypos);
}

static void
char_callback(GLFWwindow* window, unsigned int c)
{
  dt_gui_imgui_character(window, c);
}

static void
scroll_callback(GLFWwindow *window, double xoff, double yoff)
{
  dt_view_mouse_scrolled(window, xoff, yoff);
  dt_gui_imgui_scrolled(window, xoff, yoff);
}

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_err|s_log_gui|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();
  threads_global_init();

  char *filename = 0;
  if(argc > 1) filename = realpath(argv[argc-1], 0);
  else         filename = realpath("~/Pictures", 0);
  if(dt_gui_init())
  {
    dt_log(s_log_gui|s_log_err, "failed to init gui/swapchain");
    exit(1);
  }

  // start fullscreen on current monitor, we only know which one is that after
  // we created the window in dt_gui_init(). maybe should be a config option:
  key_callback(qvk.window, GLFW_KEY_F11, 0, GLFW_PRESS, 0);

  glfwSetKeyCallback(qvk.window, key_callback);
  glfwSetWindowSizeCallback(qvk.window, window_size_callback);
  glfwSetWindowPosCallback(qvk.window, window_pos_callback);
  glfwSetMouseButtonCallback(qvk.window, mouse_button_callback);
  glfwSetCursorPosCallback(qvk.window, mouse_position_callback);
  glfwSetCharCallback(qvk.window, char_callback);
  glfwSetScrollCallback(qvk.window, scroll_callback);

  vkdt.view_mode = s_view_cnt;
  // this is done so we can mess with the thumbnails struct in
  // an asynchronous manner in a background thread. it will know
  // it is not serving thumbnails but it only writes bc1 to disk.
  // also we have a temporary thumbnails struct and background threads
  // to create thumbnails, if necessary.
  // only width/height will matter here
  dt_thumbnails_init(&vkdt.thumbnail_gen, 400, 400, 0, 0);
  struct stat statbuf = {0};
  if(filename) stat(filename, &statbuf);
  if(!filename || ((statbuf.st_mode & S_IFMT) == S_IFDIR))
  {
    vkdt.view_mode = s_view_lighttable;
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3000, 1ul<<30);
    dt_db_init(&vkdt.db);
    dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, filename);
    dt_view_switch(s_view_lighttable);
    dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db);
  }
  else
  {
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3, 1ul<<20);
    dt_db_init(&vkdt.db);
    if(dt_db_load_image(&vkdt.db, &vkdt.thumbnails, filename))
    {
      dt_log(s_log_err, "image could not be loaded!");
      goto out;
    }
    dt_db_selection_add(&vkdt.db, 0);
    dt_view_switch(s_view_darkroom);
  }
  dt_gui_read_tags();

  // main loop
  vkdt.graph_dev.frame = vkdt.state.anim_frame = 0;
  while(g_running)
  {
    // block and wait for one event instead of polling all the time to save on
    // gpu workload. might need an interrupt for "render finished" etc. we might
    // do that via glfwPostEmptyEvent()
    if(g_busy > 0) g_busy--;
    // g_busy = 100; // do these two lines instead if profiling in nvidia gfx insight or so.
    // vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    if(vkdt.state.anim_playing) // should redraw because animation is playing?
      g_busy = vkdt.state.anim_max_frame == 0 ? 3 : vkdt.state.anim_max_frame - vkdt.state.anim_frame + 1;
    if(g_busy > 0) glfwPostEmptyEvent();
    else g_busy = 3;
    glfwWaitEvents();
    if(glfwWindowShouldClose(qvk.window)) g_running = 0;

    clock_t beg_rf = clock();
    dt_gui_render_frame_imgui();
    clock_t end_rf  = clock();
    dt_log(s_log_perf, "ui time %2.3fs", (end_rf - beg_rf)/(double)CLOCKS_PER_SEC);

    if(dt_gui_render() == VK_SUCCESS)
      dt_gui_present();
    else
      dt_gui_recreate_swapchain();

    dt_view_process();
  }

  vkDeviceWaitIdle(qvk.device);

out:
  // leave whatever view we're still in:
  dt_view_switch(s_view_cnt);

  threads_shutdown();
  threads_global_cleanup(); // join worker threads before killing their resources
  dt_gui_cleanup();
  dt_thumbnails_cleanup(&vkdt.thumbnails);
  dt_thumbnails_cleanup(&vkdt.thumbnail_gen);
  dt_db_cleanup(&vkdt.db);
  dt_pipe_global_cleanup();
  free(filename);
  exit(0);
}
