#include "qvk/qvk.h"

#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/global.h"
#include "db/thumbnails.h"
#include "core/log.h"
#include "gui/gui.h"
#include "gui/render.h"
#include "gui/darkroom.h"
#include "gui/view.h"
#include "db/db.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdio.h>
#include <time.h>

dt_gui_t vkdt;

static void
handle_event(SDL_Event *event)
{
  switch(vkdt.view_mode)
  {
  case s_view_darkroom:
    darkroom_handle_event(event);
    break;
  default:;
  }
}

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_gui|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();

  const char *graphcfg = 0;
  const char *dirname  = 0;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graphcfg = argv[++i];
    else if(!strcmp(argv[i], "--dir") && i < argc-1)
      dirname = argv[++i];
  }
  if(!graphcfg && !dirname)
  {
    dt_log(s_log_gui, "usage: vkdt -g <graph.cfg> [-d verbosity]");
    exit(1);
  }
  if(dt_gui_init()) exit(1);

  // TODO: clean up view mode logic!

  // TODO: always init thumbnails and db
  // TODO: if graph is given, init with one image?
  // TODO: maybe take image as argument, not the graph

  if(dirname)
  {
    vkdt.view_mode = s_view_lighttable;
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3000, 1ul<<30);
    dt_db_init(&vkdt.db);
    dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, dirname);
  }

  if(graphcfg)
  {
    vkdt.view_mode = s_view_darkroom;
    darkroom_enter();
  }
  dt_gui_read_ui_ascii("darkroom.ui");

  // main loop
  int running = 1;
  clock_t beg = clock();
  while(running)
  {
    SDL_Event event;
    // block and wait for one event instead of polling all the time to save on
    // gpu workload. might need an interrupt for "render finished" etc. we might
    // do that via SDL_PushEvent().
    SDL_WaitEvent(&event);
    do
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
      else if(event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_RESIZED &&
          event.window.windowID == SDL_GetWindowID(qvk.window))
      {
        // XXX need to rebuild the swap chain!
      }
      else handle_event(&event);
    }
    while (SDL_PollEvent(&event));

    dt_gui_render_frame_imgui();

    dt_gui_render();
    dt_gui_present();

    switch(vkdt.view_mode)
    {
      case s_view_darkroom:
        darkroom_process();
        break;
      default:;
    }
    clock_t end  = clock();
    dt_log(s_log_perf, "total frame time %2.3f s", (end - beg)/(double)CLOCKS_PER_SEC);
    beg = end;
  }

  vkDeviceWaitIdle(qvk.device);

  // TODO: on exit dr mode!
  if(graphcfg)
    darkroom_leave();

  dt_gui_cleanup();
  dt_thumbnails_cleanup(&vkdt.thumbnails);
  dt_db_cleanup(&vkdt.db);
  exit(0);
}
