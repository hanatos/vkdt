/*
Copyright (C) 2018 Christoph Schied
Copyright (C) 2019 johannes hanika

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

#include "qvk.h"
#include "core/log.h"

#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <execinfo.h>

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
  "VK_LAYER_LUNARG_standard_validation",
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
    assert(0);
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

  /* create swapchain */
  VkSurfaceCapabilitiesKHR surf_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(qvk.physical_device, qvk.surface, &surf_capabilities);

  uint32_t num_formats = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(qvk.physical_device, qvk.surface, &num_formats, NULL);
  VkSurfaceFormatKHR *avail_surface_formats = alloca(sizeof(VkSurfaceFormatKHR) * num_formats);
  vkGetPhysicalDeviceSurfaceFormatsKHR(qvk.physical_device, qvk.surface, &num_formats, avail_surface_formats);
  dt_log(s_log_qvk, "num surface formats: %d", num_formats);

  dt_log(s_log_qvk, "available surface formats:");
  for(int i = 0; i < num_formats; i++)
    dt_log(s_log_qvk, qvk_format_to_string(avail_surface_formats[i].format));


  VkFormat acceptable_formats[] = {
    // XXX when using srgb buffers, we don't need to apply the curve in f2srgb,
    // XXX but can probably let fixed function hardware do the job. faster?
    // XXX would need to double check that export does the right thing then.
    // VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_B8G8R8_UNORM,
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
  qvk.present_mode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be there, but has vsync frame time jitter

  if(surf_capabilities.currentExtent.width != ~0u)
  {
    qvk.extent = surf_capabilities.currentExtent;
  }
  else
  {
    qvk.extent.width  = MIN(surf_capabilities.maxImageExtent.width,  qvk.win_width);
    qvk.extent.height = MIN(surf_capabilities.maxImageExtent.height, qvk.win_height);

    qvk.extent.width  = MAX(surf_capabilities.minImageExtent.width,  qvk.extent.width);
    qvk.extent.height = MAX(surf_capabilities.minImageExtent.height, qvk.extent.height);
  }

  // this is stupid, but it seems if the window manager does not allow going fullscreen
  // it crashes otherwise. sometimes you need to first make the window floating in dwm
  // before going F11 -> fullscreen works. sigh.
  qvk.win_width  = qvk.extent.width;
  qvk.win_height = qvk.extent.height;

  uint32_t num_images = surf_capabilities.minImageCount;
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
    .oldSwapchain          = old_swap_chain, /* need to provide previous swapchain in case of window resize */
  };

  QVKR(vkCreateSwapchainKHR(qvk.device, &swpch_create_info, NULL, &qvk.swap_chain));

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
      .subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
      }
    };

    QVKR(vkCreateImageView(qvk.device, &img_create_info, NULL, qvk.swap_chain_image_views + i));
  }

  if(old_swap_chain)
    vkDestroySwapchainKHR(qvk.device, old_swap_chain, 0);

  return VK_SUCCESS;
}

// this function works without gui and consequently does not init glfw
VkResult
qvk_init()
{
  threads_mutex_init(&qvk.queue_mutex, 0);
  /* layers */
  get_vk_layer_list(&qvk.num_layers, &qvk.layers);

  /* instance extensions */
  int num_inst_ext_combined = qvk.num_glfw_extensions + LENGTH(vk_requested_instance_extensions);
  char **ext = alloca(sizeof(char *) * num_inst_ext_combined);
  memcpy(ext, qvk.glfw_extensions, qvk.num_glfw_extensions * sizeof(*qvk.glfw_extensions));
  memcpy(ext + qvk.num_glfw_extensions, vk_requested_instance_extensions, sizeof(vk_requested_instance_extensions));

  get_vk_extension_list(NULL, &qvk.num_extensions, &qvk.extensions);

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

  QVKR(vkCreateInstance(&inst_create_info, NULL, &qvk.instance));

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

  QVKR(qvkCreateDebugUtilsMessengerEXT(qvk.instance, &dbg_create_info, NULL, &qvk.dbg_messenger));

  /* pick physical device (iterate over all but pick device 0 anyways) */
  uint32_t num_devices = 0;
  QVKR(vkEnumeratePhysicalDevices(qvk.instance, &num_devices, NULL));
  if(num_devices == 0)
    return 1;
  VkPhysicalDevice *devices = alloca(sizeof(VkPhysicalDevice) *num_devices);
  QVKR(vkEnumeratePhysicalDevices(qvk.instance, &num_devices, devices));

  // can probably remove a few more here:
#define QVK_FEATURE_LIST \
QVK_FEATURE_DO(robustBufferAccess, 1)\
QVK_FEATURE_DO(fullDrawIndexUint32, 1)\
QVK_FEATURE_DO(imageCubeArray, 1)\
QVK_FEATURE_DO(independentBlend, 1)\
QVK_FEATURE_DO(geometryShader, 1)\
QVK_FEATURE_DO(tessellationShader, 1)\
QVK_FEATURE_DO(sampleRateShading, 0)\
QVK_FEATURE_DO(dualSrcBlend, 1)\
QVK_FEATURE_DO(logicOp, 1)\
QVK_FEATURE_DO(multiDrawIndirect, 1)\
QVK_FEATURE_DO(drawIndirectFirstInstance, 1)\
QVK_FEATURE_DO(depthClamp, 1)\
QVK_FEATURE_DO(depthBiasClamp, 1)\
QVK_FEATURE_DO(fillModeNonSolid, 0)\
QVK_FEATURE_DO(depthBounds, 0)\
QVK_FEATURE_DO(wideLines, 0)\
QVK_FEATURE_DO(largePoints, 0)\
QVK_FEATURE_DO(alphaToOne, 0)\
QVK_FEATURE_DO(multiViewport, 0)\
QVK_FEATURE_DO(samplerAnisotropy, 1)\
QVK_FEATURE_DO(textureCompressionETC2, 0)\
QVK_FEATURE_DO(textureCompressionASTC_LDR, 0)\
QVK_FEATURE_DO(textureCompressionBC, 1)\
QVK_FEATURE_DO(occlusionQueryPrecise, 0)\
QVK_FEATURE_DO(pipelineStatisticsQuery, 1)\
QVK_FEATURE_DO(vertexPipelineStoresAndAtomics, 1)\
QVK_FEATURE_DO(fragmentStoresAndAtomics, 1)\
QVK_FEATURE_DO(shaderTessellationAndGeometryPointSize, 1)\
QVK_FEATURE_DO(shaderImageGatherExtended, 1)\
QVK_FEATURE_DO(shaderStorageImageExtendedFormats, 1)\
QVK_FEATURE_DO(shaderStorageImageMultisample, 0)\
QVK_FEATURE_DO(shaderStorageImageReadWithoutFormat, 1)\
QVK_FEATURE_DO(shaderStorageImageWriteWithoutFormat, 1)\
QVK_FEATURE_DO(shaderUniformBufferArrayDynamicIndexing, 1)\
QVK_FEATURE_DO(shaderSampledImageArrayDynamicIndexing, 1)\
QVK_FEATURE_DO(shaderStorageBufferArrayDynamicIndexing, 1)\
QVK_FEATURE_DO(shaderStorageImageArrayDynamicIndexing, 1)\
QVK_FEATURE_DO(shaderClipDistance, 1)\
QVK_FEATURE_DO(shaderCullDistance, 1)\
QVK_FEATURE_DO(shaderFloat64, 1)\
QVK_FEATURE_DO(shaderInt64, 1)\
QVK_FEATURE_DO(shaderInt16, 0)\
QVK_FEATURE_DO(shaderResourceResidency, 0)\
QVK_FEATURE_DO(shaderResourceMinLod, 0)\
QVK_FEATURE_DO(sparseBinding, 0)\
QVK_FEATURE_DO(sparseResidencyBuffer, 0)\
QVK_FEATURE_DO(sparseResidencyImage2D, 0)\
QVK_FEATURE_DO(sparseResidencyImage3D, 0)\
QVK_FEATURE_DO(sparseResidency2Samples, 0)\
QVK_FEATURE_DO(sparseResidency4Samples, 0)\
QVK_FEATURE_DO(sparseResidency8Samples, 0)\
QVK_FEATURE_DO(sparseResidency16Samples, 0)\
QVK_FEATURE_DO(sparseResidencyAliased, 0)\
QVK_FEATURE_DO(variableMultisampleRate, 0)\
QVK_FEATURE_DO(inheritedQueries, 1)

  int picked_device = -1;
  VkPhysicalDeviceFeatures dev_features;
  for(int i = 0; i < num_devices; i++) {
    VkPhysicalDeviceProperties dev_properties;
    vkGetPhysicalDeviceProperties(devices[i], &dev_properties);
    vkGetPhysicalDeviceFeatures  (devices[i], &dev_features);
    qvk.ticks_to_nanoseconds = dev_properties.limits.timestampPeriod;
    qvk.uniform_alignment    = dev_properties.limits.minUniformBufferOffsetAlignment;

    dt_log(s_log_qvk, "dev %d: vendorid 0x%x", i, dev_properties.vendorID);
    dt_log(s_log_qvk, "dev %d: %s", i, dev_properties.deviceName);
    dt_log(s_log_qvk, "max number of allocations %d", dev_properties.limits.maxMemoryAllocationCount);
    dt_log(s_log_qvk, "max image allocation size %u x %u",
        dev_properties.limits.maxImageDimension2D, dev_properties.limits.maxImageDimension2D);
    dt_log(s_log_qvk, "max uniform buffer range %u", dev_properties.limits.maxUniformBufferRange);
#define QVK_FEATURE_DO(F, R)\
    if(dev_features.F == 0 && R == 1) {\
      dev_features.F = 1;\
      dt_log(s_log_qvk|s_log_err, "device does not support requested feature " #F ", trying anyways");}
    QVK_FEATURE_LIST
#undef QVK_FEATURE_DO
#undef QVK_FEATURE_LIST
    uint32_t num_ext;
    vkEnumerateDeviceExtensionProperties(devices[i], NULL, &num_ext, NULL);

    VkExtensionProperties *ext_properties = alloca(sizeof(VkExtensionProperties) * num_ext);
    vkEnumerateDeviceExtensionProperties(devices[i], NULL, &num_ext, ext_properties);

    // dt_log(s_log_qvk, "supported extensions:");
    // for(int j = 0; j < num_ext; j++) {
      // dt_log(s_log_qvk, ext_properties[j].extensionName);
      // no ray tracing needed:
      // if(!strcmp(ext_properties[j].extensionName, VK_NV_RAY_TRACING_EXTENSION_NAME)) {
    // vendor ids are: nvidia 0x10de, intel 0x8086
    if(picked_device < 0 || dev_properties.vendorID == 0x10de)
      picked_device = i;
      // }
    // }
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

  int queue_family_index = -1;
  int queue_cnt = 0;
  for(int i = 0; i < num_queue_families; i++)
  {
    if((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
       (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
       (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT))
    {
      queue_cnt = queue_families[i].queueCount;
      queue_family_index = i;
      break;
    }
  }

  if(queue_family_index < 0)
  {
    dt_log(s_log_err|s_log_qvk, "could not find suitable queue family!");
    return 1;
  }

  float queue_priorities = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info = {
    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueCount       = MIN(queue_cnt, 3),
    .pQueuePriorities = &queue_priorities,
    .queueFamilyIndex = queue_family_index,
  };

  VkPhysicalDeviceDescriptorIndexingFeaturesEXT idx_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    .runtimeDescriptorArray = 1,
    .shaderSampledImageArrayNonUniformIndexing = 1,
    // .descriptorBindingPartiallyBound = 1, // might need this for variably sized texture arrays
  };
  VkPhysicalDeviceFeatures2 device_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    .pNext = &idx_features,
    .features = dev_features,
  };

  const char *vk_requested_device_extensions[] = {
    // VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, // intel doesn't have it pre 2015 (hd 520)
#ifdef QVK_ENABLE_VALIDATION
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, // goes last because we might not want it without gui
  };
  const int len = LENGTH(vk_requested_device_extensions) - (qvk.window ? 0 : 1);
  VkDeviceCreateInfo dev_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext                   = &device_features,
    .pQueueCreateInfos       = &queue_create_info,
    .queueCreateInfoCount    = 1,
    .enabledExtensionCount   = len,
    .ppEnabledExtensionNames = vk_requested_device_extensions,
  };

  /* create device and queue */
  QVKR(vkCreateDevice(qvk.physical_device, &dev_create_info, NULL, &qvk.device));

  vkGetDeviceQueue(qvk.device, qvk.queue_idx_graphics, 0, &qvk.queue_graphics);
  vkGetDeviceQueue(qvk.device, qvk.queue_idx_compute,  0, &qvk.queue_compute);
  vkGetDeviceQueue(qvk.device, qvk.queue_idx_work0,    MIN(queue_cnt-1, 1), &qvk.queue_work0);
  vkGetDeviceQueue(qvk.device, qvk.queue_idx_work1,    MIN(queue_cnt-1, 2), &qvk.queue_work1);

#define _VK_EXTENSION_DO(a) \
    q##a = (PFN_##a) vkGetDeviceProcAddr(qvk.device, #a); \
    if(!q##a) { dt_log(s_log_qvk|s_log_err, "could not load function %s. do you have validation layers setup correctly?", #a); return VK_INCOMPLETE; }
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
  QVKR(vkCreateSampler(qvk.device, &sampler_info, NULL, &qvk.tex_sampler));
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
  QVKR(vkCreateSampler(qvk.device, &sampler_nearest_info, NULL, &qvk.tex_sampler_nearest));
  ATTACH_LABEL_VARIABLE(qvk.tex_sampler_nearest, SAMPLER);

  // initialise a safe fallback for cli mode ("dspy" format is going to look here):
  qvk.surf_format.format = VK_FORMAT_R8G8B8A8_UNORM;

  return VK_SUCCESS;
}

static VkResult
destroy_swapchain()
{
  for(int i = 0; i < qvk.num_swap_chain_images; i++)
    vkDestroyImageView(qvk.device, qvk.swap_chain_image_views[i], NULL);

  vkDestroySwapchainKHR(qvk.device, qvk.swap_chain, NULL);
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
