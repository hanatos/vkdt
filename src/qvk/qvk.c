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
  "VK_LAYER_KHRONOS_validation",
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
  .apiVersion         = VK_API_VERSION_1_2,
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
  if(strncmp(callback_data->pMessage, "Device Extension", sizeof(*"Device Extension"))) // avoid excessive spam
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

  dt_log(s_log_qvk, "available surface formats:");
  for(int i = 0; i < num_formats; i++)
    dt_log(s_log_qvk, qvk_format_to_string(avail_surface_formats[i].format));


  VkFormat acceptable_formats[] = {
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
    qvk.extent = surf_capabilities.minImageExtent;
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
  assert(qvk.num_swap_chain_images <= QVK_MAX_SWAPCHAIN_IMAGES);
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
qvk_init(const char *preferred_device_name)
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
QVK_FEATURE_DO(shaderStorageImageReadWithoutFormat, 0)\
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

  qvk.raytracing_supported = 0;
  int picked_device = -1;
  VkPhysicalDeviceFeatures dev_features;
  for(int i = 0; i < num_devices; i++) {
    VkPhysicalDeviceProperties dev_properties;
    vkGetPhysicalDeviceProperties(devices[i], &dev_properties);
    vkGetPhysicalDeviceFeatures  (devices[i], &dev_features);

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

    if((preferred_device_name && !strcmp(preferred_device_name, dev_properties.deviceName)) ||
       (picked_device < 0 || dev_properties.vendorID == 0x10de))
    { // vendor ids are: nvidia 0x10de, intel 0x8086
      qvk.ticks_to_nanoseconds = dev_properties.limits.timestampPeriod;
      qvk.uniform_alignment    = dev_properties.limits.minUniformBufferOffsetAlignment;
      for(int k=0;k<num_ext;k++)
        if (!strcmp(ext_properties[k].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME))
          qvk.raytracing_supported = 1;
        else if (!strcmp(ext_properties[k].extensionName, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME))
          qvk.float_atomics_supported = 1;
      if(dev_properties.vendorID == 0x1002) // this is AMD
        qvk.float_atomics_supported = 0;    // it seems they lie to us about support.
      picked_device = i;
      if(preferred_device_name)
        dt_log(s_log_qvk, "selecting device %s by explicit request", preferred_device_name);
    }
  }

  if(picked_device < 0)
  {
    dt_log(s_log_qvk|s_log_err, "could not find any suitable device!");
    return 1;
  }

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

  float queue_priorities[3] = {1.0f};
  VkDeviceQueueCreateInfo queue_create_info = {
    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueCount       = MIN(queue_cnt, 3),
    .pQueuePriorities = queue_priorities,
    .queueFamilyIndex = queue_family_index,
  };

  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {
    .sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure = VK_TRUE,
  };
  VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {
    .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .pNext    = &acceleration_structure_features,
    .rayQuery = VK_TRUE,
  };
  VkPhysicalDeviceVulkan12Features v12f = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext                                     = qvk.raytracing_supported ? &ray_query_features : 0,
    .descriptorIndexing                        = VK_TRUE,
    .uniformAndStorageBuffer8BitAccess         = VK_TRUE,
    .runtimeDescriptorArray                    = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .bufferDeviceAddress                       = VK_TRUE,
  };
  VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_features = {
    .sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
    .shaderImageFloat32Atomics   = VK_TRUE,
    .shaderImageFloat32AtomicAdd = VK_TRUE,
    .pNext                       = &v12f,
  };
  VkPhysicalDeviceVulkan11Features v11f = {
    .sType                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .samplerYcbcrConversion = 1,
    .pNext                  = &atomic_features,
  };
  VkPhysicalDeviceFeatures2 device_features = {
    .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .features = dev_features,
    .pNext    = &v11f,
  };
  vkGetPhysicalDeviceFeatures2(qvk.physical_device, &device_features);
  VkPhysicalDeviceFeatures2 *tmp = &device_features;
  while(tmp)
  { // now find out whether we *really* support 32-bit floating point atomic adds:
    if(tmp->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT)
    {
      VkPhysicalDeviceShaderAtomicFloatFeaturesEXT *af =
        (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT*)tmp;
      if(af->shaderImageFloat32AtomicAdd == VK_FALSE)
        qvk.float_atomics_supported = 0;
    }
    tmp = tmp->pNext;
  }

  dt_log(s_log_qvk, "picked device %d %s ray tracing and %s float atomics support", picked_device,
      qvk.raytracing_supported ? "with" : "without",
      qvk.float_atomics_supported ? "with" : "without");

  const char *requested_device_extensions[30] = {
    // ray tracing
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, // intel doesn't have it pre 2015 (hd 520)
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    // end of ray tracing
    // VK_NV_SHADER_SUBGROUP_PARTITIONED_EXTENSION_NAME, // ballot voting in work group
  };
  int len = (qvk.raytracing_supported ? 7 : 0);
  if(qvk.float_atomics_supported) requested_device_extensions[len++] = VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME;
#ifdef QVK_ENABLE_VALIDATION
  requested_device_extensions[len++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
#endif
  if(qvk.window) requested_device_extensions[len++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

  VkDeviceCreateInfo dev_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext                   = &device_features,
    .pQueueCreateInfos       = &queue_create_info,
    .queueCreateInfoCount    = 1,
    .enabledExtensionCount   = len,
    .ppEnabledExtensionNames = requested_device_extensions,
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

  // also one for webcams/yuv:
  VkFormatProperties format_properties;
  vkGetPhysicalDeviceFormatProperties(qvk.physical_device, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, &format_properties);
  int cosited = (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) != 0;
  int midpoint = !cosited && ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) != 0);
  if (!cosited && !midpoint)
  {
    dt_log(s_log_err|s_log_qvk, "yuv (G8_B8R8_2PLANE_420) not supported!");
    return 1;
  }
  VkSamplerYcbcrConversionCreateInfo conversion_info = {
    .sType         = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
    .format        = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
    .ycbcrModel    = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
    .ycbcrRange    = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
    .xChromaOffset = cosited ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT,
    .yChromaOffset = cosited ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT,
    .chromaFilter  = VK_FILTER_LINEAR,
    .forceExplicitReconstruction = VK_FALSE,
  };
  QVKR(vkCreateSamplerYcbcrConversion(qvk.device, &conversion_info, 0, &qvk.yuv_conversion));
  VkSamplerYcbcrConversionInfo ycbcr_info = {
    .sType      = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
    .conversion = qvk.yuv_conversion,
  };
  VkSamplerCreateInfo sampler_yuv_info = {
    .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext        = &ycbcr_info,
    .magFilter    = VK_FILTER_LINEAR,
    .minFilter    = VK_FILTER_LINEAR,
    .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .minLod       = 0.0f,
    .maxLod       = 1.0f,
    .borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };
  QVKR(vkCreateSampler(qvk.device, &sampler_yuv_info, NULL, &qvk.tex_sampler_yuv));

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
  vkDestroySampler(qvk.device, qvk.tex_sampler_yuv, 0);
  vkDestroySamplerYcbcrConversion(qvk.device, qvk.yuv_conversion, 0);

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
