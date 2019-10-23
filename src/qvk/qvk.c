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

// #define QVK_ENABLE_VALIDATION

#include "qvk.h"
#include "core/log.h"

#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <execinfo.h>


typedef enum {
  QVK_INIT_DEFAULT            = 0,
  QVK_INIT_SWAPCHAIN_RECREATE = (1 << 0),
  QVK_INIT_RELOAD_SHADER      = (1 << 1),
}
qvk_init_flags_t;

// dynamic reloading facility, put our stuff in here!
#if 0
typedef struct qvk_init_t
{
  const char *name;
  VkResult (*initialize)();
  VkResult (*destroy)();
  qvk_init_flags_t flags;
  int is_initialized;
}
qvk_init_t;
qvk_init_t qvk_initialization[] = {
  { "profiler", vkpt_profiler_initialize,            vkpt_profiler_destroy,                QVK_INIT_DEFAULT,            0 },
  { "shader",   vkpt_load_shader_modules,            vkpt_destroy_shader_modules,          QVK_INIT_RELOAD_SHADER,      0 },
  { "vbo",      vkpt_vertex_buffer_create,           vkpt_vertex_buffer_destroy,           QVK_INIT_DEFAULT,            0 },
  { "ubo",      vkpt_uniform_buffer_create,          vkpt_uniform_buffer_destroy,          QVK_INIT_DEFAULT,            0 },
  { "textures", vkpt_textures_initialize,            vkpt_textures_destroy,                QVK_INIT_DEFAULT,            0 },
  { "images",   vkpt_create_images,                  vkpt_destroy_images,                  QVK_INIT_SWAPCHAIN_RECREATE, 0 },
  { "draw",     vkpt_draw_initialize,                vkpt_draw_destroy,                    QVK_INIT_DEFAULT,            0 },
  { "lh",       vkpt_lh_initialize,                  vkpt_lh_destroy,                      QVK_INIT_DEFAULT,            0 },
  { "pt",       vkpt_pt_init,                        vkpt_pt_destroy,                      QVK_INIT_DEFAULT,            0 },
  { "pt|",      vkpt_pt_create_pipelines,            vkpt_pt_destroy_pipelines,            QVK_INIT_SWAPCHAIN_RECREATE
                                                                                         | QVK_INIT_RELOAD_SHADER,      0 },
  { "draw|",    vkpt_draw_create_pipelines,          vkpt_draw_destroy_pipelines,          QVK_INIT_SWAPCHAIN_RECREATE
                                                                                         | QVK_INIT_RELOAD_SHADER,      0 },
  { "vbo|",     vkpt_vertex_buffer_create_pipelines, vkpt_vertex_buffer_destroy_pipelines, QVK_INIT_RELOAD_SHADER,      0 },
  { "asvgf",    vkpt_asvgf_initialize,               vkpt_asvgf_destroy,                   QVK_INIT_DEFAULT,            0 },
  { "asvgf|",   vkpt_asvgf_create_pipelines,         vkpt_asvgf_destroy_pipelines,         QVK_INIT_RELOAD_SHADER,      0 },
};

static VkResult
qvk_initialize_all(qvk_init_flags_t init_flags)
{
  vkDeviceWaitIdle(qvk.device);
  for(int i = 0; i < LENGTH(qvk_initialization); i++)
  {
    qvk_init_t *init = qvk_initialization + i;
    if((init->flags & init_flags) != init_flags)
      continue;
    dt_log(s_log_qvk, "initializing %s", qvk_initialization[i].name);
    assert(!init->is_initialized);
    init->is_initialized = init->initialize
      ? (init->initialize() == VK_SUCCESS)
      : 1;
    assert(init->is_initialized);
  }
  return VK_SUCCESS;
}

static VkResult
qvk_destroy_all(qvk_init_flags_t destroy_flags)
{
  vkDeviceWaitIdle(qvk.device);
  for(int i = LENGTH(qvk_initialization) - 1; i >= 0; i--)
  {
    qvk_init_t *init = qvk_initialization + i;
    if((init->flags & destroy_flags) != destroy_flags)
      continue;
    dt_log(s_log_qvk, "destroying %s", qvk_initialization[i].name);
    assert(init->is_initialized);
    init->is_initialized = init->destroy
      ? !(init->destroy() == VK_SUCCESS)
      : 0;
    assert(!init->is_initialized);
  }
  return VK_SUCCESS;
}
#endif

// XXX currently unused, but we want this feature
#if 0
void
qvk_reload_shader()
{
  char buf[1024];
#ifdef _WIN32
  FILE *f = _popen("bash -c \"make -C/home/cschied/quake2-pt compile_shaders\"", "r");
#else
  FILE *f = popen("make -j compile_shaders", "r");
#endif
  if(f)
  {
    while(fgets(buf, sizeof buf, f))
      dt_log(s_log_qvk, "%s", buf);
    fclose(f);
  }
  qvk_destroy_all(QVK_INIT_RELOAD_SHADER);
  qvk_initialize_all(QVK_INIT_RELOAD_SHADER);
}
#endif

qvk_t qvk =
{
  .win_width          = 1920,
  .win_height         = 1080,
  .frame_counter      = 0,
};

#define _VK_EXTENSION_DO(a) PFN_##a q##a;
_VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

const char *vk_requested_layers[] = {
  "VK_LAYER_LUNARG_standard_validation"
};

const char *vk_requested_instance_extensions[] = {
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
};

static const VkApplicationInfo vk_app_info = {
  .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
  .pApplicationName   = "darktable ng",
  .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
  .pEngineName        = "vkdt",
  .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
  .apiVersion         = VK_API_VERSION_1_1,
};

static void
get_vk_extension_list(
    const char *layer,
    uint32_t *num_extensions,
    VkExtensionProperties **ext)
{
  QVK(vkEnumerateInstanceExtensionProperties(layer, num_extensions, NULL));
  *ext = malloc(sizeof(**ext) * *num_extensions);
  QVK(vkEnumerateInstanceExtensionProperties(layer, num_extensions, *ext));
}

static void
get_vk_layer_list(
    uint32_t *num_layers,
    VkLayerProperties **ext)
{
  QVK(vkEnumerateInstanceLayerProperties(num_layers, NULL));
  *ext = malloc(sizeof(**ext) * *num_layers);
  QVK(vkEnumerateInstanceLayerProperties(num_layers, *ext));
}

#if 0
static int
layer_supported(const char *name)
{
  assert(qvk.layers);
  for(int i = 0; i < qvk.num_layers; i++)
    if(!strcmp(name, qvk.layers[i].layerName))
      return 1;
  return 0;
}
#endif

static int
layer_requested(const char *name)
{
  for(int i = 0; i < LENGTH(vk_requested_layers); i++)
    if(!strcmp(name, vk_requested_layers[i]))
      return 1;
  return 0;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void *user_data)
{
  dt_log(s_log_qvk, "validation layer: %s", callback_data->pMessage);
#ifndef NDEBUG
  if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
  {
    // void *const buf[100];
    // backtrace_symbols_fd(buf, 100, 2);
    // assert(0);
  }
#endif
  return VK_FALSE;
}

static VkResult
qvkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pCallback)
{
  PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if(func)
    return func(instance, pCreateInfo, pAllocator, pCallback);
  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static VkResult
qvkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT callback,
    const VkAllocationCallbacks* pAllocator)
{
  PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if(func) {
    func(instance, callback, pAllocator);
    return VK_SUCCESS;
  }
  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VkResult
qvk_create_swapchain()
{
  VkSwapchainKHR old_swap_chain = qvk.swap_chain;
  QVK(vkDeviceWaitIdle(qvk.device));

  if(old_swap_chain)
    for(int i = 0; i < qvk.num_swap_chain_images; i++)
      vkDestroyImageView(qvk.device, qvk.swap_chain_image_views[i], 0);

  /* create swapchain (query details and ignore them afterwards :-) )*/
  VkSurfaceCapabilitiesKHR surf_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(qvk.physical_device, qvk.surface, &surf_capabilities);

  uint32_t num_formats = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(qvk.physical_device, qvk.surface, &num_formats, NULL);
  VkSurfaceFormatKHR *avail_surface_formats = alloca(sizeof(VkSurfaceFormatKHR) * num_formats);
  vkGetPhysicalDeviceSurfaceFormatsKHR(qvk.physical_device, qvk.surface, &num_formats, avail_surface_formats);
  dt_log(s_log_qvk, "num surface formats: %d", num_formats);

  dt_log(s_log_qvk, "available surface formats:");
  for(int i = 0; i < num_formats; i++)
    dt_log(s_log_qvk, vk_format_to_string(avail_surface_formats[i].format));


  VkFormat acceptable_formats[] = {
    // XXX when using srgb buffers, we don't need to apply the curve in f2srgb,
    // XXX but can probably let fixed function hardware do the job. faster?
    // XXX would need to double check that export does the right thing then.
    // VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM
  };

  for(int i = 0; i < LENGTH(acceptable_formats); i++) {
    for(int j = 0; j < num_formats; j++)
      if(acceptable_formats[i] == avail_surface_formats[j].format) {
        qvk.surf_format = avail_surface_formats[j];
        dt_log(s_log_qvk, "colour space: %u", qvk.surf_format.colorSpace);
        goto out;
      }
  }
out:;

  uint32_t num_present_modes = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(qvk.physical_device, qvk.surface, &num_present_modes, NULL);
  VkPresentModeKHR *avail_present_modes = alloca(sizeof(VkPresentModeKHR) * num_present_modes);
  vkGetPhysicalDeviceSurfacePresentModesKHR(qvk.physical_device, qvk.surface, &num_present_modes, avail_present_modes);
  //qvk.present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  qvk.present_mode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be there, but has vsync frame time jitter
  // qvk.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
  //qvk.present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

  if(surf_capabilities.currentExtent.width != ~0u)
  {
    qvk.extent = surf_capabilities.currentExtent;
  }
  else {
    qvk.extent.width  = MIN(surf_capabilities.maxImageExtent.width,  qvk.win_width);
    qvk.extent.height = MIN(surf_capabilities.maxImageExtent.height, qvk.win_height);

    qvk.extent.width  = MAX(surf_capabilities.minImageExtent.width,  qvk.extent.width);
    qvk.extent.height = MAX(surf_capabilities.minImageExtent.height, qvk.extent.height);
  }

  uint32_t num_images = 2;
  //uint32_t num_images = surf_capabilities.minImageCount + 1;
  if(surf_capabilities.maxImageCount > 0)
    num_images = MIN(num_images, surf_capabilities.maxImageCount);

  VkSwapchainCreateInfoKHR swpch_create_info = {
    .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface               = qvk.surface,
    .minImageCount         = num_images,
    .imageFormat           = qvk.surf_format.format,
    .imageColorSpace       = qvk.surf_format.colorSpace,
    .imageExtent           = qvk.extent,
    .imageArrayLayers      = 1, /* only needs to be changed for stereoscopic rendering */ 
    .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                           | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE, /* VK_SHARING_MODE_CONCURRENT if not using same queue */
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = NULL,
    .preTransform          = surf_capabilities.currentTransform,
    .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, /* no alpha for window transparency */
    .presentMode           = qvk.present_mode,
    .clipped               = VK_FALSE, /* do not render pixels that are occluded by other windows */
    //.clipped               = VK_TRUE, /* do not render pixels that are occluded by other windows */
    .oldSwapchain          = old_swap_chain, /* need to provide previous swapchain in case of window resize */
  };

  if(vkCreateSwapchainKHR(qvk.device, &swpch_create_info, NULL, &qvk.swap_chain) != VK_SUCCESS)
  {
    dt_log(s_log_qvk, "error creating swapchain");
    return 1;
  }

  vkGetSwapchainImagesKHR(qvk.device, qvk.swap_chain, &qvk.num_swap_chain_images, NULL);
  assert(qvk.num_swap_chain_images < QVK_MAX_SWAPCHAIN_IMAGES);
  vkGetSwapchainImagesKHR(qvk.device, qvk.swap_chain, &qvk.num_swap_chain_images, qvk.swap_chain_images);

  for(int i = 0; i < qvk.num_swap_chain_images; i++)
  {
    VkImageViewCreateInfo img_create_info = {
      .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image      = qvk.swap_chain_images[i],
      .viewType   = VK_IMAGE_VIEW_TYPE_2D,
      .format     = qvk.surf_format.format,
#if 1
      .components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A
      },
#endif
      .subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
      }
    };

    if(vkCreateImageView(qvk.device, &img_create_info, NULL, qvk.swap_chain_image_views + i) != VK_SUCCESS)
    {
      dt_log(s_log_qvk|s_log_err, "error creating image view!");
      return 1;
    }
  }

  if(old_swap_chain)
    vkDestroySwapchainKHR(qvk.device, old_swap_chain, 0);

  return VK_SUCCESS;
}

// this function works without gui and consequently does not init SDL
int
qvk_init()
{
  threads_mutex_init(&qvk.queue_mutex, 0);
  /* layers */
  get_vk_layer_list(&qvk.num_layers, &qvk.layers);
  dt_log(s_log_qvk, "available vulkan layers:");
  for(int i = 0; i < qvk.num_layers; i++) {
    int requested = layer_requested(qvk.layers[i].layerName);
    dt_log(s_log_qvk, "%s%s", qvk.layers[i].layerName, requested ? " (requested)" : "");
  }

  /* instance extensions */
  int num_inst_ext_combined = qvk.num_sdl2_extensions + LENGTH(vk_requested_instance_extensions);
  char **ext = alloca(sizeof(char *) * num_inst_ext_combined);
  memcpy(ext, qvk.sdl2_extensions, qvk.num_sdl2_extensions * sizeof(*qvk.sdl2_extensions));
  memcpy(ext + qvk.num_sdl2_extensions, vk_requested_instance_extensions, sizeof(vk_requested_instance_extensions));

  get_vk_extension_list(NULL, &qvk.num_extensions, &qvk.extensions); /* valid here? */
  dt_log(s_log_qvk, "supported vulkan instance extensions:");
  for(int i = 0; i < qvk.num_extensions; i++)
  {
    int requested = 0;
    for(int j = 0; j < num_inst_ext_combined; j++)
    {
      if(!strcmp(qvk.extensions[i].extensionName, ext[j]))
      {
        requested = 1;
        break;
      }
    }
    dt_log(s_log_qvk, "%s%s", qvk.extensions[i].extensionName, requested ? " (requested)" : "");
  }

  /* create instance */
  VkInstanceCreateInfo inst_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo        = &vk_app_info,
#ifdef QVK_ENABLE_VALIDATION
    .enabledLayerCount       = LENGTH(vk_requested_layers),
    .ppEnabledLayerNames     = vk_requested_layers,
#endif
    .enabledExtensionCount   = num_inst_ext_combined,
    .ppEnabledExtensionNames = (const char * const*)ext,
  };

  QVK(vkCreateInstance(&inst_create_info, NULL, &qvk.instance));

  /* setup debug callback */
  VkDebugUtilsMessengerCreateInfoEXT dbg_create_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = vk_debug_callback,
    .pUserData = NULL
  };

  QVK(qvkCreateDebugUtilsMessengerEXT(qvk.instance, &dbg_create_info, NULL, &qvk.dbg_messenger));

  /* pick physical device (iterate over all but pick device 0 anyways) */
  uint32_t num_devices = 0;
  QVK(vkEnumeratePhysicalDevices(qvk.instance, &num_devices, NULL));
  if(num_devices == 0)
    return 1;
  VkPhysicalDevice *devices = alloca(sizeof(VkPhysicalDevice) *num_devices);
  QVK(vkEnumeratePhysicalDevices(qvk.instance, &num_devices, devices));
  // can probably remove a few more here:
  VkPhysicalDeviceFeatures required_features = {
      .robustBufferAccess = 1,
      .fullDrawIndexUint32 = 1,
      .imageCubeArray = 1,
      .independentBlend = 1,
      .geometryShader = 1,
      .tessellationShader = 1,
      .dualSrcBlend = 1,
      .logicOp = 1,
      .multiDrawIndirect = 1,
      .drawIndirectFirstInstance = 1,
      .depthClamp = 1,
      .depthBiasClamp = 1,
      .depthBounds = 1,
      .samplerAnisotropy = 1,
      .textureCompressionBC = 1,
      .pipelineStatisticsQuery = 1,
      .vertexPipelineStoresAndAtomics = 1,
      .fragmentStoresAndAtomics = 1,
      .shaderTessellationAndGeometryPointSize = 1,
      .shaderImageGatherExtended = 1,
      .shaderStorageImageExtendedFormats = 1,
      .shaderStorageImageMultisample = 1,
      .shaderStorageImageReadWithoutFormat = 1,
      .shaderStorageImageWriteWithoutFormat = 1,
      .shaderUniformBufferArrayDynamicIndexing = 1,
      .shaderSampledImageArrayDynamicIndexing = 1,
      .shaderStorageBufferArrayDynamicIndexing = 1,
      .shaderStorageImageArrayDynamicIndexing = 1,
      .shaderClipDistance = 1,
      .shaderCullDistance = 1,
      .shaderFloat64 = 1,
      .shaderInt64 = 1,
      .shaderResourceResidency = 1,
      .shaderResourceMinLod = 1,
      .sparseBinding = 1,
      .inheritedQueries = 1,
    };

#define QVK_FEATURE_LIST \
QVK_FEATURE_DO(robustBufferAccess)\
QVK_FEATURE_DO(fullDrawIndexUint32)\
QVK_FEATURE_DO(imageCubeArray)\
QVK_FEATURE_DO(independentBlend)\
QVK_FEATURE_DO(geometryShader)\
QVK_FEATURE_DO(tessellationShader)\
QVK_FEATURE_DO(sampleRateShading)\
QVK_FEATURE_DO(dualSrcBlend)\
QVK_FEATURE_DO(logicOp)\
QVK_FEATURE_DO(multiDrawIndirect)\
QVK_FEATURE_DO(drawIndirectFirstInstance)\
QVK_FEATURE_DO(depthClamp)\
QVK_FEATURE_DO(depthBiasClamp)\
QVK_FEATURE_DO(fillModeNonSolid)\
QVK_FEATURE_DO(depthBounds)\
QVK_FEATURE_DO(wideLines)\
QVK_FEATURE_DO(largePoints)\
QVK_FEATURE_DO(alphaToOne)\
QVK_FEATURE_DO(multiViewport)\
QVK_FEATURE_DO(samplerAnisotropy)\
QVK_FEATURE_DO(textureCompressionETC2)\
QVK_FEATURE_DO(textureCompressionASTC_LDR)\
QVK_FEATURE_DO(textureCompressionBC)\
QVK_FEATURE_DO(occlusionQueryPrecise)\
QVK_FEATURE_DO(pipelineStatisticsQuery)\
QVK_FEATURE_DO(vertexPipelineStoresAndAtomics)\
QVK_FEATURE_DO(fragmentStoresAndAtomics)\
QVK_FEATURE_DO(shaderTessellationAndGeometryPointSize)\
QVK_FEATURE_DO(shaderImageGatherExtended)\
QVK_FEATURE_DO(shaderStorageImageExtendedFormats)\
QVK_FEATURE_DO(shaderStorageImageMultisample)\
QVK_FEATURE_DO(shaderStorageImageReadWithoutFormat)\
QVK_FEATURE_DO(shaderStorageImageWriteWithoutFormat)\
QVK_FEATURE_DO(shaderUniformBufferArrayDynamicIndexing)\
QVK_FEATURE_DO(shaderSampledImageArrayDynamicIndexing)\
QVK_FEATURE_DO(shaderStorageBufferArrayDynamicIndexing)\
QVK_FEATURE_DO(shaderStorageImageArrayDynamicIndexing)\
QVK_FEATURE_DO(shaderClipDistance)\
QVK_FEATURE_DO(shaderCullDistance)\
QVK_FEATURE_DO(shaderFloat64)\
QVK_FEATURE_DO(shaderInt64)\
QVK_FEATURE_DO(shaderInt16)\
QVK_FEATURE_DO(shaderResourceResidency)\
QVK_FEATURE_DO(shaderResourceMinLod)\
QVK_FEATURE_DO(sparseBinding)\
QVK_FEATURE_DO(sparseResidencyBuffer)\
QVK_FEATURE_DO(sparseResidencyImage2D)\
QVK_FEATURE_DO(sparseResidencyImage3D)\
QVK_FEATURE_DO(sparseResidency2Samples)\
QVK_FEATURE_DO(sparseResidency4Samples)\
QVK_FEATURE_DO(sparseResidency8Samples)\
QVK_FEATURE_DO(sparseResidency16Samples)\
QVK_FEATURE_DO(sparseResidencyAliased)\
QVK_FEATURE_DO(variableMultisampleRate)\
QVK_FEATURE_DO(inheritedQueries)

  int picked_device = -1;
  for(int i = 0; i < num_devices; i++) {
    VkPhysicalDeviceProperties dev_properties;
    VkPhysicalDeviceFeatures   dev_features;
    vkGetPhysicalDeviceProperties(devices[i], &dev_properties);
    vkGetPhysicalDeviceFeatures  (devices[i], &dev_features);
    qvk.ticks_to_nanoseconds = dev_properties.limits.timestampPeriod;

    dt_log(s_log_qvk, "dev %d: %s", i, dev_properties.deviceName);
    dt_log(s_log_qvk, "max number of allocations %d", dev_properties.limits.maxMemoryAllocationCount);
    dt_log(s_log_qvk, "max image allocation size %u x %u",
        dev_properties.limits.maxImageDimension2D, dev_properties.limits.maxImageDimension2D);
#define QVK_FEATURE_DO(F)\
    if(dev_features.F == 0 && required_features.F == 1)\
      dt_log(s_log_qvk|s_log_err, "device does not support requested feature " #F );
    QVK_FEATURE_LIST
#undef QVK_FEATURE_DO
#undef QVK_FEATURE_LIST
    uint32_t num_ext;
    vkEnumerateDeviceExtensionProperties(devices[i], NULL, &num_ext, NULL);

    VkExtensionProperties *ext_properties = alloca(sizeof(VkExtensionProperties) * num_ext);
    vkEnumerateDeviceExtensionProperties(devices[i], NULL, &num_ext, ext_properties);

    dt_log(s_log_qvk, "supported extensions:");
    for(int j = 0; j < num_ext; j++) {
      dt_log(s_log_qvk, ext_properties[j].extensionName);
      // no ray tracing needed:
      // if(!strcmp(ext_properties[j].extensionName, VK_NV_RAY_TRACING_EXTENSION_NAME)) {
        if(picked_device < 0)
          picked_device = i;
      // }
    }
  }

  if(picked_device < 0) {
    dt_log(s_log_qvk, "could not find any suitable device supporting " VK_NV_RAY_TRACING_EXTENSION_NAME"!");
    return 1;
  }

  dt_log(s_log_qvk, "picked device %d", picked_device);

  qvk.physical_device = devices[picked_device];


  vkGetPhysicalDeviceMemoryProperties(qvk.physical_device, &qvk.mem_properties);

  /* queue family and create physical device */
  uint32_t num_queue_families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(qvk.physical_device, &num_queue_families, NULL);
  VkQueueFamilyProperties *queue_families = alloca(sizeof(VkQueueFamilyProperties) * num_queue_families);
  vkGetPhysicalDeviceQueueFamilyProperties(qvk.physical_device, &num_queue_families, queue_families);

  dt_log(s_log_qvk, "num queue families: %d", num_queue_families);

  qvk.queue_idx_graphics = -1;
  qvk.queue_idx_compute  = -1;
  qvk.queue_idx_transfer = -1;

  for(int i = 0; i < num_queue_families; i++) {
    if(!queue_families[i].queueCount)
      continue;
    if((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT && qvk.queue_idx_graphics < 0) {
      qvk.queue_idx_graphics = i;
    }
    if((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT && qvk.queue_idx_compute < 0) {
      qvk.queue_idx_compute = i;
    }
    if((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT && qvk.queue_idx_transfer < 0) {
      qvk.queue_idx_transfer = i;
    }
  }

  if(qvk.queue_idx_graphics < 0 || qvk.queue_idx_compute < 0 || qvk.queue_idx_transfer < 0)
  {
    dt_log(s_log_err|s_log_qvk, "could not find suitable queue family!");
    return 1;
  }

  float queue_priorities = 1.0f;
  int num_create_queues = 0;
  VkDeviceQueueCreateInfo queue_create_info[3];

  {
    VkDeviceQueueCreateInfo q = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priorities,
      .queueFamilyIndex = qvk.queue_idx_graphics,
    };

    queue_create_info[num_create_queues++] = q;
  };
  if(qvk.queue_idx_compute != qvk.queue_idx_graphics) {
    VkDeviceQueueCreateInfo q = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priorities,
      .queueFamilyIndex = qvk.queue_idx_compute,
    };
    queue_create_info[num_create_queues++] = q;
  };
  if(qvk.queue_idx_transfer != qvk.queue_idx_graphics && qvk.queue_idx_transfer != qvk.queue_idx_compute) {
    VkDeviceQueueCreateInfo q = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priorities,
      .queueFamilyIndex = qvk.queue_idx_transfer,
    };
    queue_create_info[num_create_queues++] = q;
  };

  VkPhysicalDeviceDescriptorIndexingFeaturesEXT idx_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    .runtimeDescriptorArray = 1,
    .shaderSampledImageArrayNonUniformIndexing = 1,
  };
  VkPhysicalDeviceFeatures2 device_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    .pNext = &idx_features,
    .features = required_features,
  };

  const char *vk_requested_device_extensions[] = {
    // VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, // :( intel doesn't have it
#ifdef QVK_ENABLE_VALIDATION
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, // goes last because we might not want it without gui
  };
  const int len = LENGTH(vk_requested_device_extensions) - (qvk.window ? 0 : 1);
  VkDeviceCreateInfo dev_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext                   = &device_features,
    .pQueueCreateInfos       = queue_create_info,
    .queueCreateInfoCount    = num_create_queues,
    .enabledExtensionCount   = len,
    .ppEnabledExtensionNames = vk_requested_device_extensions,
  };

  /* create device and queue */
  QVK(vkCreateDevice(qvk.physical_device, &dev_create_info, NULL, &qvk.device));

  vkGetDeviceQueue(qvk.device, qvk.queue_idx_graphics, 0, &qvk.queue_graphics);
  vkGetDeviceQueue(qvk.device, qvk.queue_idx_compute,  0, &qvk.queue_compute);
  vkGetDeviceQueue(qvk.device, qvk.queue_idx_transfer, 0, &qvk.queue_transfer);

#define _VK_EXTENSION_DO(a) \
    q##a = (PFN_##a) vkGetDeviceProcAddr(qvk.device, #a); \
    if(!q##a) { dt_log(s_log_qvk|s_log_err, "could not load function %s", #a); }
  _VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

  // create texture samplers
  VkSamplerCreateInfo sampler_info = {
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter               = VK_FILTER_LINEAR,
    .minFilter               = VK_FILTER_LINEAR,
    .addressModeU            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeV            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeW            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .anisotropyEnable        = VK_FALSE,
    .maxAnisotropy           = 16,
    .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .minLod                  = 0.0f,
    .maxLod                  = 128.0f,
  };
  QVK(vkCreateSampler(qvk.device, &sampler_info, NULL, &qvk.tex_sampler));
  ATTACH_LABEL_VARIABLE(qvk.tex_sampler, SAMPLER);
  VkSamplerCreateInfo sampler_nearest_info = {
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter               = VK_FILTER_NEAREST,
    .minFilter               = VK_FILTER_NEAREST,
    .addressModeU            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeV            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeW            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .anisotropyEnable        = VK_FALSE,
    .maxAnisotropy           = 16,
    .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
  };
  QVK(vkCreateSampler(qvk.device, &sampler_nearest_info, NULL, &qvk.tex_sampler_nearest));
  ATTACH_LABEL_VARIABLE(qvk.tex_sampler_nearest, SAMPLER);

  return 0;
}

static VkResult
destroy_swapchain()
{
  for(int i = 0; i < qvk.num_swap_chain_images; i++)
    vkDestroyImageView  (qvk.device, qvk.swap_chain_image_views[i], NULL);

  vkDestroySwapchainKHR(qvk.device,   qvk.swap_chain, NULL);
  return VK_SUCCESS;
}

int
qvk_cleanup()
{
  vkDeviceWaitIdle(qvk.device);
  threads_mutex_destroy(&qvk.queue_mutex);
  vkDestroySampler(qvk.device, qvk.tex_sampler, 0);
  vkDestroySampler(qvk.device, qvk.tex_sampler_nearest, 0);

  if(qvk.window)  destroy_swapchain();
  if(qvk.surface) vkDestroySurfaceKHR(qvk.instance, qvk.surface, NULL);

  vkDestroyCommandPool (qvk.device, qvk.command_pool,     NULL);

  vkDestroyDevice      (qvk.device,   NULL);
  QVK(qvkDestroyDebugUtilsMessengerEXT(qvk.instance, qvk.dbg_messenger, NULL));
  vkDestroyInstance    (qvk.instance, NULL);

  free(qvk.extensions);
  qvk.extensions = NULL;
  qvk.num_extensions = 0;

  free(qvk.layers);
  qvk.layers = NULL;
  qvk.num_layers = 0;

  return 0;
}

// currently unused:
#if 0
static void
recreate_swapchain()
{
  vkDeviceWaitIdle(qvk.device);
  qvk_destroy_all(QVK_INIT_SWAPCHAIN_RECREATE);
  qvk_destroy_swapchain();
  SDL_GetWindowSize(qvk.window, &qvk.win_width, &qvk.win_height);
  qvk_create_swapchain();
  qvk_initialize_all(QVK_INIT_SWAPCHAIN_RECREATE);
}
#endif

