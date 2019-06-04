#include "gui.h"
#include "qvk/qvk.h"

#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_vulkan.h>
  
int dt_gui_init(dt_gui_t *gui)
{
  qvk.win_width  = 1152;
  qvk.win_height = 786;
  qvk.window = SDL_CreateWindow("vkdt", 20, 50,
      qvk.win_width, qvk.win_height,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if(!qvk.window)
  {
    dt_log(s_log_gui|s_log_err, "could not create SDL window `%s'", SDL_GetError());
    return 1;
  }

  if(!SDL_Vulkan_GetInstanceExtensions(qvk.window, &qvk.num_sdl2_extensions, NULL))
  {
    dt_log(s_log_gui|s_log_err, "couldn't get SDL extension count");
    return 1;
  }
  qvk.sdl2_extensions = malloc(sizeof(char*) * qvk.num_sdl2_extensions);
  if(!SDL_Vulkan_GetInstanceExtensions(qvk.window, &qvk.num_sdl2_extensions, qvk.sdl2_extensions)) {
    dt_log(s_log_err|s_log_gui, "couldn't get SDL extensions");
    return 1;
  }

  dt_log(s_log_gui, "vk extension required by SDL2:");
  for(int i = 0; i < qvk.num_sdl2_extensions; i++)
    dt_log(s_log_gui, "  %s", qvk.sdl2_extensions[i]);

  if(qvk_init())
  {
    dt_log(s_log_err|s_log_gui, "init vulkan failed");
    return 1;
  }

  /* create surface */
  if(!SDL_Vulkan_CreateSurface(qvk.window, qvk.instance, &qvk.surface))
  {
    dt_log(s_log_qvk|s_log_err, "could not create surface!");
    return 1;
  }

  QVK(qvk_create_swapchain());
  QVK(qvk_create_command_pool_and_fences());
  // QVK(qvk_initialize_all(VKPT_INIT_DEFAULT));


  // TODO: main loop and imgui!
  // while(1)
  // {
  // TODO: call into render.cpp stuff here
  // }

  return 0;
}


void dt_gui_cleanup(dt_gui_t *gui)
{
}
