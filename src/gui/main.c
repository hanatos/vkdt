#include "qvk/qvk.h"

#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/global.h"
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

dt_gui_t vkdt;

static int g_running = 1;
static int g_busy = 3;
static int g_fullscreen = 0;

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
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
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
char_callback(GLFWwindow* window, unsigned int c)
{
  dt_gui_imgui_character(window, c);
}

static void
scroll_callback(GLFWwindow *window, double xoff, double yoff)
{
  dt_gui_imgui_scrolled(window, xoff, yoff);
}

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_gui|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();
  threads_global_init();

  const char *fname = 0;
  if(argc > 1) fname = argv[argc-1];
  struct stat statbuf;
  if(!fname || stat(fname, &statbuf))
  {
    dt_log(s_log_gui, "usage: vkdt [-d verbosity] directory|rawfile");
    exit(1);
  }
  if(dt_gui_init())
  {
    dt_log(s_log_gui|s_log_err, "failed to init gui/swapchain");
    exit(1);
  }

  glfwSetKeyCallback(qvk.window, key_callback);
  glfwSetWindowSizeCallback(qvk.window, window_size_callback);
  glfwSetMouseButtonCallback(qvk.window, mouse_button_callback);
  glfwSetCursorPosCallback(qvk.window, mouse_position_callback);
  glfwSetCharCallback(qvk.window, char_callback);
  glfwSetScrollCallback(qvk.window, scroll_callback);

  dt_thumbnails_t *tmp_tn = 0;
  vkdt.view_mode = s_view_cnt;
  if((statbuf.st_mode & S_IFMT) == S_IFDIR)
  {
    vkdt.view_mode = s_view_lighttable;
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3000, 1ul<<30);
    dt_db_init(&vkdt.db);
    dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, fname);
    dt_view_switch(s_view_lighttable);

    // this is done so we can mess with the thumbnails struct in
    // an asynchronous manner in a background thread. it will know
    // it is not serving thumbnails but it only writes bc1 to disk.
    // also we have a temporary thumbnails struct and background threads
    // to create thumbnails, if necessary.
    // we'll elegantly leak all this memory:
    tmp_tn = malloc(sizeof(dt_thumbnails_t));
    // only width/height will matter here
    dt_thumbnails_init(tmp_tn, 400, 400, 0, 0);
    dt_thumbnails_cache_collection(tmp_tn, &vkdt.db);
  }
  else
  {
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3, 1ul<<20);
    dt_db_init(&vkdt.db);
    dt_db_load_image(&vkdt.db, &vkdt.thumbnails, fname);
    vkdt.db.current_image = 0;
    dt_view_switch(s_view_darkroom);
  }

  // main loop
  clock_t beg = clock();
  while(g_running)
  {
    // block and wait for one event instead of polling all the time to save on
    // gpu workload. might need an interrupt for "render finished" etc. we might
    // do that via glfwPostEmptyEvent()
    if(g_busy > 0) g_busy--;
    if(vkdt.state.anim_playing) // should redraw because animation is playing?
      g_busy = vkdt.state.anim_max_frame - vkdt.state.anim_frame - 1;
    if(g_busy > 0) glfwPostEmptyEvent();
    glfwWaitEvents();

    clock_t beg_rf = clock();
    dt_gui_render_frame_imgui();
    clock_t end_rf  = clock();
    dt_log(s_log_perf, "ui time %2.3fs", (end_rf - beg_rf)/(double)CLOCKS_PER_SEC);

    dt_gui_render();
    dt_gui_present();

    if(vkdt.state.anim_playing)
    {
      vkdt.graph_dev.frame = vkdt.state.anim_frame;
      if(vkdt.state.anim_frame < vkdt.state.anim_max_frame)
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    }
    dt_view_process();
    if(!vkdt.state.anim_playing)
      vkdt.state.anim_frame = 0;
    else if(vkdt.state.anim_frame < vkdt.state.anim_max_frame-1)
      vkdt.state.anim_frame++;

    clock_t end  = clock();
    dt_log(s_log_perf, "total frame time %2.3fs", (end - beg)/(double)CLOCKS_PER_SEC);
    beg = end;
  }

  vkDeviceWaitIdle(qvk.device);

  // leave whatever view we're still in:
  dt_view_switch(s_view_cnt);

  threads_shutdown();
  dt_gui_cleanup();
  dt_thumbnails_cleanup(&vkdt.thumbnails);
  if(tmp_tn) dt_thumbnails_cleanup(tmp_tn);
  dt_db_cleanup(&vkdt.db);
  dt_pipe_global_cleanup();
  threads_global_cleanup();
  exit(0);
}
