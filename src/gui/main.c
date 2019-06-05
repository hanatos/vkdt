#include "qvk/qvk.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include "pipe/graph.h"
#include "pipe/global.h"
#include "core/log.h"
#include "gui.h"

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
  if(dt_gui_init(&vkdt)) exit(1);

  dt_graph_t graph;
  dt_graph_init(&graph);
  int err = dt_graph_read_config_ascii(&graph, graphcfg);
  if(err)
  {
    dt_log(s_log_err|s_log_gui, "could not load graph configuration from '%s'!", graphcfg);
    exit(1);
  }

  dt_graph_setup_pipeline(&graph);

  // main loop
  int running = 1;
  while(running)
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      // ImGui_ImplSDL2_ProcessEvent(&event);
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

    dt_gui_render_frame();
    // TODO: equivalent of frame render and frame present
  }

  dt_gui_cleanup(&vkdt);
  exit(0);
}
