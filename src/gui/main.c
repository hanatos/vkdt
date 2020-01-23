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

#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

dt_gui_t vkdt;

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
    dt_thumbnails_cache_directory(tmp_tn, fname);
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
  int running = 1;
  int fullscreen = 0;
  int busy = 1;   // go to idle only after a few frames have passed since interaction
  clock_t beg = clock();
  while(running)
  {
    SDL_Event event;
    // block and wait for one event instead of polling all the time to save on
    // gpu workload. might need an interrupt for "render finished" etc. we might
    // do that via SDL_PushEvent().
    int have_event = 1;
    if(busy >= 0)
      have_event = SDL_PollEvent(&event);
    else           SDL_WaitEvent(&event);
    if(have_event) do
    {
      if(dt_gui_poll_event_imgui(&event))
        ;
      else if(event.type == SDL_QUIT)
        running = 0;
      else if(event.type == SDL_KEYDOWN &&
           event.key.keysym.sym == SDLK_ESCAPE)
      {
        running = 0;
      }
      else if(event.type == SDL_KEYDOWN &&
              event.key.keysym.sym == SDLK_F11)
      {
        if(fullscreen)
        {
          SDL_SetWindowSize(qvk.window, 1600, 800);
          SDL_SetWindowPosition(qvk.window, 30, 40);
          fullscreen = 0;
        }
        else
        {
          SDL_DisplayMode mode;
          SDL_GetCurrentDisplayMode(0, &mode); // TODO get current screen
          dt_log(s_log_gui, "display mode %d %d\n", mode.w, mode.h);
          SDL_RestoreWindow(qvk.window);
          SDL_SetWindowSize(qvk.window, mode.w, mode.h);
          SDL_SetWindowPosition(qvk.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
          fullscreen = 1;
        }
        dt_gui_recreate_swapchain();
      }
      else if(event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_RESIZED &&
          event.window.windowID == SDL_GetWindowID(qvk.window))
      { // window resized, need to rebuild our swapchain:
        dt_gui_recreate_swapchain();
      }
      else dt_view_handle_event(&event);
      if(vkdt.state.anim_playing)
        busy = vkdt.state.anim_max_frame;
      else
        busy = 2;
    }
    while (SDL_PollEvent(&event));
    else busy--;

    clock_t beg_rf = clock();
    dt_gui_render_frame_imgui();
    clock_t end_rf  = clock();
    dt_log(s_log_perf, "ui time %2.3fs", (end_rf - beg_rf)/(double)CLOCKS_PER_SEC);

    dt_gui_render();
    dt_gui_present();

    if(vkdt.state.anim_playing)
    {
      vkdt.graph_dev.frame = vkdt.state.anim_frame;
      vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    }
    dt_view_process();
    if(vkdt.state.anim_playing)
      vkdt.state.anim_frame = MIN(vkdt.state.anim_frame+1, vkdt.state.anim_max_frame);
    else vkdt.state.anim_frame = 0;

    clock_t end  = clock();
    dt_log(s_log_perf, "total frame time %2.3fs", (end - beg)/(double)CLOCKS_PER_SEC);
    beg = end;
  }

  vkDeviceWaitIdle(qvk.device);

  // leave whatever view we're still in:
  dt_view_switch(s_view_cnt);

  dt_gui_cleanup();
  dt_thumbnails_cleanup(&vkdt.thumbnails);
  dt_db_cleanup(&vkdt.db);
  threads_global_cleanup();
  if(tmp_tn) dt_thumbnails_cleanup(tmp_tn);
  exit(0);
}
