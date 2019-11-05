#pragma once
/*
Copyright (C) 2018 Christoph Schied

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "core/core.h"
#include "core/threads.h"
#include "qvk_util.h"

#include <vulkan/vulkan.h>


#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))

#define QVK_LOG_FUNC_(f) do {} while(0)
//#define QVK_LOG_FUNC_(f) fprintf(stderr, "[qvk] %s\n", f)
#define QVK_LOG_FUNC() QVK_LOG_FUNC_(__func__)

#define QVK_FUNC_UNIMPLEMENTED_(f) dt_log(s_log_qvk, "calling unimplemented function %s", f)
#define QVK_FUNC_UNIMPLEMENTED() QVK_FUNC_UNIMPLEMENTED_(__func__)

#define QVK(...) \
  do { \
    VkResult _res = __VA_ARGS__; \
    if(_res != VK_SUCCESS) { \
      dt_log(s_log_qvk, "error %d executing %s!", _res, # __VA_ARGS__); \
    } \
  } while(0)

#define QVKR(...) \
  do { \
    VkResult _res = __VA_ARGS__; \
    if(_res != VK_SUCCESS) { \
      dt_log(s_log_qvk, "error %d executing %s!", _res, # __VA_ARGS__); \
      return _res; \
    } \
  } while(0)


#define QVK_MAX_FRAMES_IN_FLIGHT 2
#define QVK_MAX_SWAPCHAIN_IMAGES 4

// forward declare
typedef struct SDL_Window SDL_Window;

typedef struct qvk_t
{
  VkInstance                  instance;
  VkPhysicalDevice            physical_device;
  VkPhysicalDeviceMemoryProperties mem_properties;
  VkDevice                    device;
  threads_mutex_t             queue_mutex;
  VkQueue                     queue_graphics;
  VkQueue                     queue_compute;
  VkQueue                     queue_transfer;
  int32_t                     queue_idx_graphics;
  int32_t                     queue_idx_compute;
  int32_t                     queue_idx_transfer;
  VkSurfaceKHR                surface;
  VkSwapchainKHR              swap_chain;
  VkSurfaceFormatKHR          surf_format;
  VkPresentModeKHR            present_mode;
  VkExtent2D                  extent;
  VkCommandPool               command_pool;
  uint32_t                    num_swap_chain_images;
  VkImage                     swap_chain_images[QVK_MAX_SWAPCHAIN_IMAGES];
  VkImageView                 swap_chain_image_views[QVK_MAX_SWAPCHAIN_IMAGES];

  VkSampler                   tex_sampler;
  VkSampler                   tex_sampler_nearest;
  uint32_t                    num_command_buffers;
  VkCommandBuffer             *command_buffers;
  VkCommandBuffer             cmd_buf_current;

  uint32_t                    num_extensions;
  VkExtensionProperties       *extensions;

  uint32_t                    num_layers;
  VkLayerProperties           *layers;

  VkDebugUtilsMessengerEXT    dbg_messenger;

  int                         win_width;
  int                         win_height;
  uint64_t                    frame_counter;

  SDL_Window                  *window;
  uint32_t                    num_sdl2_extensions;
  const char                  **sdl2_extensions;

  uint32_t                    current_image_index;

  float                       ticks_to_nanoseconds;
}
qvk_t;

extern qvk_t qvk;

#define _VK_EXTENSION_LIST \
  _VK_EXTENSION_DO(vkDebugMarkerSetObjectNameEXT)

#define _VK_EXTENSION_DO(a) extern PFN_##a q##a;
_VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

// global initialisation:
VkResult qvk_init();
VkResult qvk_cleanup();

VkResult qvk_create_swapchain();
