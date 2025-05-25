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

#include "pipe/token.h"
#include "core/core.h"
#include "core/threads.h"
#include "qvk_util.h"

#include <vulkan/vulkan.h>


#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))

#define QVK(...) \
  do { \
    VkResult _res = __VA_ARGS__; \
    if(_res != VK_SUCCESS) { \
      dt_log(s_log_qvk, "error %s executing %s!", qvk_result_to_string(_res), # __VA_ARGS__); \
    } \
  } while(0)

// check error and also return it if fail
#define QVKR(...) \
  do { \
    VkResult _res = __VA_ARGS__; \
    if(_res != VK_SUCCESS) { \
      dt_log(s_log_qvk, "error %s executing %s!", qvk_result_to_string(_res), # __VA_ARGS__); \
      return _res; \
    } \
  } while(0)

#define QVKL(mutex, ...) \
  do { \
    if(mutex) threads_mutex_lock(mutex);\
    VkResult _res = __VA_ARGS__; \
    if(mutex) threads_mutex_unlock(mutex);\
    if(_res != VK_SUCCESS) { \
      dt_log(s_log_qvk, "error %s executing %s!", qvk_result_to_string(_res), # __VA_ARGS__); \
    } \
  } while(0)

// lock mutex, check error, return if fail
#define QVKLR(mutex, ...) \
  do { \
    if(mutex) threads_mutex_lock(mutex);\
    VkResult _res = __VA_ARGS__; \
    if(mutex) threads_mutex_unlock(mutex);\
    if(_res != VK_SUCCESS) { \
      dt_log(s_log_qvk, "error %s executing %s!", qvk_result_to_string(_res), # __VA_ARGS__); \
      return _res; \
    } \
  } while(0)

#define QVK_MAX_SWAPCHAIN_IMAGES 4

#define QVK_LOAD(FUNCTION_NAME) PFN_##FUNCTION_NAME q##FUNCTION_NAME = (PFN_##FUNCTION_NAME) vkGetInstanceProcAddr(qvk.instance, #FUNCTION_NAME)

// forward declare
typedef struct GLFWwindow GLFWwindow;

typedef enum qvk_queue_name_t
{
  s_queue_graphics = 0,
  s_queue_compute  = 1,
  s_queue_work0    = 2,
  s_queue_work1    = 3,
  s_queue_cnt      = 4,
}
qvk_queue_name_t;

typedef struct qvk_queue_t
{ // wraps all we need to identify and talk to a vulkan queue
  threads_mutex_t mutex;
  VkQueue         queue;
  int32_t         family;
  int32_t         idx;
}
qvk_queue_t;

typedef struct qvk_t
{
  VkInstance                  instance;
  VkPhysicalDevice            physical_device;
  VkPhysicalDeviceMemoryProperties mem_properties;
  VkDevice                    device;
  int                         qid[s_queue_cnt];   // map queue names to actual indices in the queue array (they may be the same)
  qvk_queue_t                 queue[s_queue_cnt];

  VkSampler                   tex_sampler;
  VkSampler                   tex_sampler_dspy;
  VkSampler                   tex_sampler_nearest;
  VkSampler                   tex_sampler_yuv;
  VkSamplerYcbcrConversion    yuv_conversion;

  uint32_t                    num_extensions;
  VkExtensionProperties       *extensions;

  uint32_t                    num_layers;
  VkLayerProperties           *layers;

  VkDebugUtilsMessengerEXT    dbg_messenger;

  uint64_t                    frame_counter;

  uint32_t                    num_glfw_extensions;
  const char                  **glfw_extensions;

  float                       ticks_to_nanoseconds;
  uint64_t                    uniform_alignment;
  uint64_t                    raytracing_acc_min_align;

  int                         raytracing_supported;
  int                         float_atomics_supported;
  int                         coopmat_supported;
  int                         blit_supported;
}
qvk_t;

#ifndef VKDT_DSO_BUILD
VKDT_API extern qvk_t qvk;
#endif

#ifdef QVK_ENABLE_VALIDATION
#define _VK_EXTENSION_LIST \
  _VK_EXTENSION_DO(vkDebugMarkerSetObjectNameEXT)
#else
#define _VK_EXTENSION_LIST
#endif

#define _VK_EXTENSION_DO(a) extern PFN_##a q##a;
_VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

// global initialisation. pick device by that name if it is not null,
// same for the direct id if it is not negative (use for multiple identical devices)
VkResult qvk_init(const char *preferred_device_name, int preferred_device_id, int window, int hdr);
VkResult qvk_cleanup();
