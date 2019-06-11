#include "qvk/qvk.h"

#include "pipe/graph.h"
#include "pipe/global.h"
#include "core/log.h"
#include "gui/gui.h"
#include "gui/render.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdio.h>

dt_gui_t vkdt;

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_gui|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();

  const char *graphcfg = 0;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graphcfg = argv[++i];
  }
  if(!graphcfg)
  {
    dt_log(s_log_gui, "usage: vkdt -g <graph.cfg> [-d verbosity]");
    exit(1);
  }
  if(dt_gui_init()) exit(1);

  dt_graph_init(&vkdt.graph_dev);
  int err = dt_graph_read_config_ascii(&vkdt.graph_dev, graphcfg);
  if(err)
  {
    dt_log(s_log_err|s_log_gui, "could not load graph configuration from '%s'!", graphcfg);
    exit(1);
  }

  dt_graph_run(&vkdt.graph_dev, s_graph_run_all);

  // main loop
  int running = 1;
  while(running)
  {
    SDL_Event event;
    // block and wait for one event instead of polling all the time to save on
    // gpu workload. might need an interrupt for "render finished" etc. we might
    // do that via SDL_PushEvent().
    SDL_WaitEvent(&event);
    do
    {
      dt_gui_poll_event_imgui(&event);
      if (event.type == SDL_QUIT)
        running = 0;
      else if (event.type == SDL_KEYDOWN)
      {
        running = 0; // TODO: grab esc
      }
      else if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_RESIZED &&
          event.window.windowID == SDL_GetWindowID(qvk.window))
      {
        // XXX need to rebuild the swap chain!
      }
    }
    while (SDL_PollEvent(&event));

    dt_gui_render_frame();

    dt_gui_render();
    dt_gui_present();

    // TODO: if params changed:
    VkResult err = dt_graph_run(&vkdt.graph_dev,
        s_graph_run_all);
       // (s_graph_run_download_sink  |
       //  s_graph_run_record_cmd_buf |
       //  s_graph_run_wait_done));
    if(err != VK_SUCCESS) break;
  }

  dt_graph_cleanup(&vkdt.graph_dev);
  dt_gui_cleanup();
  exit(0);
}
