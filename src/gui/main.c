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

dt_gui_t vkdt;

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_gui|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();

  const char *imgname = 0;
  const char *dirname = 0;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-i") && i < argc-1)
      imgname = argv[++i];
    else if(!strcmp(argv[i], "--dir") && i < argc-1)
      dirname = argv[++i];
  }
  if(!imgname && !dirname)
  {
    dt_log(s_log_gui, "usage: vkdt -i <image.raw> [-d verbosity]");
    exit(1);
  }
  if(dt_gui_init()) exit(1);

  // TODO: clean up view mode logic!

  vkdt.view_mode = s_view_cnt;
  // TODO: select based on stat?
  if(dirname)
  {
    vkdt.view_mode = s_view_lighttable;
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3000, 1ul<<30);
    dt_db_init(&vkdt.db);
    dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, dirname);
    dt_view_switch(s_view_lighttable);
  }
  else if(imgname)
  {
    dt_thumbnails_init(&vkdt.thumbnails, 400, 400, 3, 1ul<<20);
    dt_db_init(&vkdt.db);
    dt_db_load_image(&vkdt.db, &vkdt.thumbnails, imgname);
    vkdt.db.current_image = 0;
    dt_view_switch(s_view_darkroom);
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
      else dt_view_handle_event(&event);
    }
    while (SDL_PollEvent(&event));

    dt_gui_render_frame_imgui();

    dt_gui_render();
    dt_gui_present();

    dt_view_process();

    clock_t end  = clock();
    dt_log(s_log_perf, "total frame time %2.3f s", (end - beg)/(double)CLOCKS_PER_SEC);
    beg = end;
  }

  vkDeviceWaitIdle(qvk.device);

  // leave whatever view we're still in:
  dt_view_switch(s_view_cnt);

  dt_gui_cleanup();
  dt_thumbnails_cleanup(&vkdt.thumbnails);
  dt_db_cleanup(&vkdt.db);
  exit(0);
}
