#include "qvk/qvk.h"

#include "pipe/graph.h"
#include "pipe/global.h"
#include "core/log.h"
#include "gui/gui.h"
#include "gui/render.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdio.h>
#include <time.h>

dt_gui_t vkdt;

static void
handle_event(SDL_Event *event)
{
  // TODO: get from somewhere better
  float wd = (float)vkdt.graph_dev.output->connector[0].roi.roi_wd;
  float ht = (float)vkdt.graph_dev.output->connector[0].roi.roi_ht;
  static int m_x = -1, m_y = -1;
  static float old_look_x = -1.0f, old_look_y = -1.0f;
  if(event->type == SDL_MOUSEMOTION)
  {
    if(m_x >= 0 && vkdt.view_scale > 0.0f)
    {
      int dx = event->button.x - m_x;
      int dy = event->button.y - m_y;
      vkdt.view_look_at_x = old_look_x - dx / vkdt.view_scale;
      vkdt.view_look_at_y = old_look_y - dy / vkdt.view_scale;
      vkdt.view_look_at_x = CLAMP(vkdt.view_look_at_x, 0.0f, wd);
      vkdt.view_look_at_y = CLAMP(vkdt.view_look_at_y, 0.0f, ht);
    }
  }
  else if(event->type == SDL_MOUSEBUTTONUP)
  {
    m_x = m_y = -1;
  }
  else if(event->type == SDL_MOUSEBUTTONDOWN &&
      event->button.x < vkdt.view_x + vkdt.view_width)
  {
    if(event->button.button == SDL_BUTTON_LEFT)
    {
      m_x = event->button.x;
      m_y = event->button.y;
      old_look_x = vkdt.view_look_at_x;
      old_look_y = vkdt.view_look_at_y;
    }
    else if(event->button.button == SDL_BUTTON_MIDDLE)
    {
      // TODO: zoom 1:1
      // TODO: two things: one is the display node which has
      // TODO: parameters to be set to zoom and wd/ht so it'll affect the ROI.
      // TODO: the other is zoom/pan in the gui for 200% or more and to
      // TODO: move the image smoothly
      // where does the mouse look in the current image?
      float imwd = vkdt.view_width, imht = vkdt.view_height;
      float scale = vkdt.view_scale <= 0.0f ? MIN(imwd/wd, imht/ht) : vkdt.view_scale;
      float im_x = (event->button.x - (vkdt.view_x + imwd)/2.0f) / scale;
      float im_y = (event->button.y - (vkdt.view_y + imht)/2.0f) / scale;
      im_x += vkdt.view_look_at_x;
      im_y += vkdt.view_look_at_y;
      if(vkdt.view_scale <= 0.0f)
      {
        vkdt.view_scale = 1.0f;
        vkdt.view_look_at_x = im_x;
        vkdt.view_look_at_y = im_y;
      }
      else if(vkdt.view_scale >= 8.0f)
      {
        vkdt.view_scale = -1.0f;
        vkdt.view_look_at_x = wd/2.0f;
        vkdt.view_look_at_y = ht/2.0f;
      }
      else if(vkdt.view_scale >= 1.0f)
      {
        vkdt.view_scale *= 2.0f;
        vkdt.view_look_at_x = im_x;
        vkdt.view_look_at_y = im_y;
      }
    }
  }
  else if (event->type == SDL_KEYDOWN)
  {
    if(event->key.keysym.sym == SDLK_r)
    {
      // DEBUG: reload shaders
      dt_graph_cleanup(&vkdt.graph_dev);
      dt_pipe_global_cleanup();
      system("make debug"); // build shaders
      dt_pipe_global_init();
      dt_graph_init(&vkdt.graph_dev);
      int err = dt_graph_read_config_ascii(&vkdt.graph_dev, vkdt.graph_cfg);
      if(err) dt_log(s_log_err, "failed to reload_shaders!");
      // (TODO: re-init params from history)
      dt_graph_run(&vkdt.graph_dev, s_graph_run_all);
      dt_gui_read_ui_ascii("darkroom.ui");
    }
  }
}

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

  snprintf(vkdt.graph_cfg, sizeof(vkdt.graph_cfg), "%s", graphcfg);
  dt_graph_init(&vkdt.graph_dev);
  int err = dt_graph_read_config_ascii(&vkdt.graph_dev, graphcfg);
  if(err)
  {
    dt_log(s_log_err|s_log_gui, "could not load graph configuration from '%s'!", graphcfg);
    exit(1);
  }

  dt_graph_run(&vkdt.graph_dev, s_graph_run_all);
  // TODO: get from command line
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
      dt_gui_poll_event_imgui(&event);
      if (event.type == SDL_QUIT)
        running = 0;
      else if (event.type == SDL_KEYDOWN)
      {
        if(event.key.keysym.sym == SDLK_ESCAPE)
          running = 0;
      }
      else if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_RESIZED &&
          event.window.windowID == SDL_GetWindowID(qvk.window))
      {
        // XXX need to rebuild the swap chain!
      }
      handle_event(&event);
    }
    while (SDL_PollEvent(&event));

    // TODO: rename? only calls imgui:
    dt_gui_render_frame();

    dt_gui_render();
    dt_gui_present();

    // intel says:
    // ==
    // The pipeline is flushed when switching between 3D graphics rendering and
    // compute functions. Asynchronous compute functions are not supported at
    // this time. Batch the compute kernels into groups whenever possible.
    // ==
    // which is unfortunate for us :/

    // VkResult fence = vkGetFenceStatus(qvk.device, vkdt.graph_dev.command_fence);
    // if(fence == VK_SUCCESS)
    {
    // TODO: if params changed:
    VkResult err = dt_graph_run(&vkdt.graph_dev,
        // s_graph_run_all);
       (s_graph_run_download_sink
       |s_graph_run_record_cmd_buf
       |s_graph_run_wait_done)); // if we don't wait we can't resubmit because the fence would be used twice.
    if(err != VK_SUCCESS) break;
    }
    clock_t end  = clock();
    dt_log(s_log_perf, "total frame time %2.3f s", (end - beg)/(double)CLOCKS_PER_SEC);
    beg = end;
  }

  dt_graph_cleanup(&vkdt.graph_dev);
  dt_gui_cleanup();
  exit(0);
}
