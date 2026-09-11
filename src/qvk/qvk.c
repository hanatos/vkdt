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
#ifndef NDEBUG
#ifdef __GLIBC__
#include <execinfo.h>
#endif
#endif

qvk_t qvk =
{
  .frame_counter = 0,
};

#define _VK_EXTENSION_DO(a) PFN_##a q##a;
_VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

const char *vk_requested_instance_extensions[] = {
  // debugging:
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#ifdef __APPLE__
  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
};

static const VkApplicationInfo vk_app_info = {
  .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
  .pApplicationName   = "vkdt",
  .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
  .pEngineName        = "vkdt",
  .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
  .apiVersion         = VK_API_VERSION_1_4,
};

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
#ifdef __GLIBC__
  if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
  {
    // void *const buf[100];
    // backtrace_symbols_fd(buf, 100, 2);
    // assert(0);
  }
#endif
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

// this function works without gui and consequently does not init glfw
VkResult
qvk_init(const char *preferred_device_name, int preferred_device_id, int window, int enable_hdr_wsi, int allow_hdr)
{
  get_vk_layer_list(&qvk.num_layers, &qvk.layers);

  const char *vk_hdr_instance_extensions[] = {
    // colour management:
    VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, // some nvidia/debian vk 1.3 doesn't have it, so we make it optional/depend on the hdr kill switch
  };

  /* instance extensions */
  int num_inst_ext_combined = qvk.num_glfw_extensions +
    LENGTH(vk_requested_instance_extensions) +
    (allow_hdr ?
    LENGTH(vk_hdr_instance_extensions) : 0);
  assert(num_inst_ext_combined <= LENGTH(qvk.inst_extension));
  memcpy(qvk.inst_extension, qvk.glfw_extensions, qvk.num_glfw_extensions * sizeof(*qvk.glfw_extensions));
  memcpy(qvk.inst_extension + qvk.num_glfw_extensions, vk_requested_instance_extensions, sizeof(vk_requested_instance_extensions));
  if(allow_hdr)
    memcpy(qvk.inst_extension + qvk.num_glfw_extensions + LENGTH(vk_requested_instance_extensions), vk_hdr_instance_extensions, sizeof(vk_hdr_instance_extensions));
  qvk.inst_extension_cnt = num_inst_ext_combined;

  const char *vk_requested_layers[] = {
#ifdef QVK_ENABLE_VALIDATION
    "VK_LAYER_KHRONOS_validation",
#endif
    "VK_LAYER_hdr_wsi",
  };
  int num_layers = LENGTH(vk_requested_layers) -1;//XXX+ (enable_hdr_wsi ? 0 : -1);
  qvk.hdr_supported = enable_hdr_wsi;

  /* create instance */
  VkInstanceCreateInfo inst_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo        = &vk_app_info,
    .enabledLayerCount       = num_layers,
    .ppEnabledLayerNames     = num_layers ? vk_requested_layers : 0,
    .enabledExtensionCount   = qvk.inst_extension_cnt,
    .ppEnabledExtensionNames = (const char * const*)qvk.inst_extension,
#ifdef __APPLE__
    .flags                   = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
  };

  {
    VkResult res = vkCreateInstance(&inst_create_info, NULL, &qvk.instance);
    if(res != VK_SUCCESS)
    {
      dt_log(s_log_qvk|s_log_err, "error %s executing vkCreateInstance!", qvk_result_to_string(res));
      if(res == VK_ERROR_LAYER_NOT_PRESENT)
#ifdef QVK_ENABLE_VALIDATION
        dt_log(s_log_qvk|s_log_err, "did you install the vulkan validation layer package?");
#else
        dt_log(s_log_qvk|s_log_err, "are you trying to run in HDR mode and the compositor lacks support?");
#endif
      return res;
    }
  }

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

  qvk.raytracing_supported = 0;
  qvk.subgroup_size_control_supported = 0;
  qvk.shader64bit_indexing_supported = 0;
  qvk.unified_image_layouts_supported = 0;
  int picked_device = -1;
  for(int i = 0; i < num_devices; i++)
  {
    VkPhysicalDeviceProperties dev_properties;
    VkPhysicalDeviceFeatures dev_features;
    vkGetPhysicalDeviceProperties(devices[i], &dev_properties);
    vkGetPhysicalDeviceFeatures  (devices[i], &dev_features);

    dt_log(s_log_qvk, "dev %d: vendorid 0x%x", i, dev_properties.vendorID);
    dt_log(s_log_qvk, "dev %d: %s", i, dev_properties.deviceName);
    dt_log(s_log_qvk, "max number of allocations %d", dev_properties.limits.maxMemoryAllocationCount);
    dt_log(s_log_qvk, "max image allocation size %u x %u",
        dev_properties.limits.maxImageDimension2D, dev_properties.limits.maxImageDimension2D);
    dt_log(s_log_qvk, "max uniform buffer range %u", dev_properties.limits.maxUniformBufferRange);
    uint32_t num_ext;
    vkEnumerateDeviceExtensionProperties(devices[i], NULL, &num_ext, NULL);

    VkExtensionProperties *ext_properties = malloc(sizeof(VkExtensionProperties) * num_ext);
    vkEnumerateDeviceExtensionProperties(devices[i], NULL, &num_ext, ext_properties);

    if((preferred_device_id == i) ||
       (preferred_device_name && !strcmp(preferred_device_name, dev_properties.deviceName)) ||
      (!preferred_device_name && preferred_device_id < 0 && (picked_device < 0 || dev_properties.vendorID == 0x10de)))
    { // vendor ids are: nvidia 0x10de, intel 0x8086
      qvk.ticks_to_nanoseconds = dev_properties.limits.timestampPeriod;
      qvk.uniform_alignment    = dev_properties.limits.minUniformBufferOffsetAlignment;
      qvk.vendorID = dev_properties.vendorID;
      snprintf(qvk.device_name, sizeof(qvk.device_name), "%s", dev_properties.deviceName);
      qvk.raytracing_supported = 0;
      qvk.float_atomics_supported = 0;
      qvk.coopmat_supported = 0;
      qvk.subgroup_size_control_supported = 0;
      qvk.shader64bit_indexing_supported = 0;
      qvk.unified_image_layouts_supported = 0;
      for(int k=0;k<num_ext;k++)
      {
        if (!strcmp(ext_properties[k].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME))
          qvk.raytracing_supported = 1;
        else if (!strcmp(ext_properties[k].extensionName, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME))
          qvk.float_atomics_supported = 1;
        else if (!strcmp(ext_properties[k].extensionName, VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME))
          qvk.coopmat_supported = 1;
        else if (!strcmp(ext_properties[k].extensionName, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME))
          qvk.subgroup_size_control_supported = 1;
        // else if (!strcmp(ext_properties[k].extensionName, VK_EXT_SHADER_64BIT_INDEXING_EXTENSION_NAME))
          // qvk.shader64bit_indexing_supported = 1;
        else if (!strcmp(ext_properties[k].extensionName, VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME))
          qvk.unified_image_layouts_supported = 1;
      }
      picked_device = i;
      if(preferred_device_name)
        dt_log(s_log_qvk, "selecting device %s by explicit request", preferred_device_name);
      if(preferred_device_id == i)
        dt_log(s_log_qvk, "selecting device %d by explicit request", preferred_device_id);
    }
    free(ext_properties);
  }

  if(picked_device < 0)
  {
    dt_log(s_log_qvk|s_log_err, "could not find any suitable device!");
    return 1;
  }

  qvk.physical_device = devices[picked_device];
  vkGetPhysicalDeviceMemoryProperties(qvk.physical_device, &qvk.mem_properties);

  if(qvk.coopmat_supported)
  { // check coopmat configuration required by us:
    uint32_t cnt;
    QVK_LOAD(vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR);
    VkResult res = qvkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(qvk.physical_device, &cnt, 0);
    qvk.coopmat_supported = 0;
    if(res == VK_SUCCESS && cnt > 0)
    {
      VkCooperativeMatrixPropertiesKHR *p = alloca(sizeof(*p)*cnt);
      memset(p, 0, sizeof(*p)*cnt);
      for(int k=0;k<cnt;k++) p[k].sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR;
      VkResult res = qvkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(qvk.physical_device, &cnt, p);
      if(res == VK_SUCCESS) for(int i=0;i<cnt&&qvk.coopmat_supported==0;i++)
      {
        dt_log(s_log_qvk, "found support for cooperative matrices %dx%dx%d", p[i].MSize, p[i].NSize, p[i].KSize);
        if(p[i].MSize == 16 &&
           p[i].NSize == 16 &&
           p[i].KSize == 16 &&
           p[i].AType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
           p[i].BType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
           p[i].CType == VK_COMPONENT_TYPE_FLOAT16_KHR)
          qvk.coopmat_supported = 1;
      }
      if(!qvk.coopmat_supported)
        dt_log(s_log_qvk, "disabling cooperative matrices because 16x16x16 is not supported");
    }
  }

  /* queue family and create physical device */
  uint32_t num_queue_families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(qvk.physical_device, &num_queue_families, NULL);
  VkQueueFamilyProperties *queue_families = alloca(sizeof(VkQueueFamilyProperties) * num_queue_families);
  vkGetPhysicalDeviceQueueFamilyProperties(qvk.physical_device, &num_queue_families, queue_families);

  dt_log(s_log_qvk, "num queue families: %d", num_queue_families);

  int queue_family_index = -1;
  int queue_cnt = 0, queue_compute_cnt = 0, queue_vid_dec_cnt = 0;
  for(int i = 0; i < num_queue_families; i++)
  {
    if((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
       (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
    {
      queue_cnt = queue_families[i].queueCount;
      queue_family_index = i;
      break;
    }
  }

  int compute_family_index = queue_family_index;
  for(int i = 0; i < num_queue_families; i++)
  {
    if((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
      !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
    {
      queue_compute_cnt = queue_families[i].queueCount;
      compute_family_index = i;
      break;
    }
  }
  
  int vid_dec_family_index = queue_family_index;
  for(int i = 0; i < num_queue_families; i++)
  {
    if(queue_families[i].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
    {
      queue_vid_dec_cnt = queue_families[i].queueCount;
      vid_dec_family_index = i;
      break;
    }
  }

  qvk.queue_family_graphics = queue_family_index;
  qvk.queue_family_compute  = compute_family_index;
  qvk.queue_family_vid_dec  = vid_dec_family_index;

  if(queue_family_index < 0)
  {
    dt_log(s_log_err|s_log_qvk, "could not find suitable queue family!");
    return 1;
  }

  float queue_priorities[s_queue_cnt] = {1.0f};
  VkDeviceQueueCreateInfo queue_create_info = {
    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueCount       = MIN(queue_cnt, s_queue_cnt),
    .pQueuePriorities = queue_priorities,
    .queueFamilyIndex = queue_family_index,
  };

  VkFormatProperties format_properties;
  vkGetPhysicalDeviceFormatProperties(qvk.physical_device, VK_FORMAT_R16G16B16A16_SFLOAT, &format_properties);
  if(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
    qvk.blit_supported = 1;
  else qvk.blit_supported = 0;

#if 0
  qvk.df_cluster_bvh = (VkPhysicalDeviceClusterAccelerationStructureFeaturesNV) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_ACCELERATION_STRUCTURE_FEATURES_NV,
    .clusterAccelerationStructure = VK_TRUE,
  };
#endif
#if 0
  qvk.df_sync = (VkPhysicalDeviceInternallySynchronizedQueuesFeaturesKHR) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INTERNALLY_SYNCHRONIZED_QUEUES_FEATURES_KHR,
    // .pNext = &qvk.df_cluster_bvh,
    .internallySynchronizedQueues = VK_TRUE,
  };
#endif
  qvk.df_accel = (VkPhysicalDeviceAccelerationStructureFeaturesKHR) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    // .pNext = &qvk.df_sync,
    .accelerationStructure = VK_TRUE,
  };
  qvk.df_ray_query = (VkPhysicalDeviceRayQueryFeaturesKHR) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .pNext = &qvk.df_accel,
    .rayQuery = VK_TRUE,
  };
  qvk.df_v14 = (VkPhysicalDeviceVulkan14Features) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
    .pNext = qvk.raytracing_supported ? &qvk.df_ray_query : 0,
    .maintenance5 = VK_TRUE,
  };
  qvk.df_v13 = (VkPhysicalDeviceVulkan13Features) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &qvk.df_v14,
    .dynamicRendering     = VK_TRUE,
    .subgroupSizeControl  = VK_TRUE,
    .computeFullSubgroups = VK_TRUE,
  };
  qvk.df_v12 = (VkPhysicalDeviceVulkan12Features) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = &qvk.df_v13,
    .descriptorIndexing                        = VK_TRUE,
    .uniformAndStorageBuffer8BitAccess         = VK_TRUE,
    .runtimeDescriptorArray                    = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .bufferDeviceAddress                       = VK_TRUE,
  };
  qvk.df_atomics = (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
    .pNext = &qvk.df_v12,
    .shaderImageFloat32Atomics   = VK_TRUE,
    .shaderImageFloat32AtomicAdd = VK_TRUE,
  };
  qvk.df_v11 = (VkPhysicalDeviceVulkan11Features) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = qvk.float_atomics_supported ? &qvk.df_atomics : qvk.df_atomics.pNext,
    .samplerYcbcrConversion = 1,
  };
  qvk.df_coopmat = (VkPhysicalDeviceCooperativeMatrixFeaturesKHR) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR,
    .pNext = &qvk.df_v11,
  };
  qvk.df_layouts = (VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR) {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR,
    .pNext = qvk.coopmat_supported ? &qvk.df_coopmat : qvk.df_coopmat.pNext,
    .unifiedImageLayouts = VK_TRUE,
    .unifiedImageLayoutsVideo = VK_TRUE,
  };
  VkPhysicalDeviceFeatures2 device_features = {
    .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .pNext    = qvk.unified_image_layouts_supported ? &qvk.df_layouts : qvk.df_layouts.pNext,
  };
  vkGetPhysicalDeviceFeatures2(qvk.physical_device, &device_features);
  // now find out whether we *really* support 32-bit floating point atomic adds:
  if(qvk.df_atomics.shaderImageFloat32AtomicAdd == VK_FALSE)
  {
    qvk.float_atomics_supported = 0;
  }
  if(qvk.subgroup_size_control_supported &&
    (!qvk.df_v13.subgroupSizeControl || !qvk.df_v13.computeFullSubgroups))
  {
    qvk.subgroup_size_control_supported = 0;
  }

  dt_log(s_log_qvk, "picked device %d %s ray tracing and %s float atomics and %s coopmat support", picked_device,
      qvk.raytracing_supported    ? "with" : "without",
      qvk.float_atomics_supported ? "with" : "without",
      qvk.coopmat_supported       ? "with" : "without");

  int len = 0;
  qvk.dev_extension[len++] = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;   // intel doesn't have it pre 2015 (hd 520)
  if(qvk.shader64bit_indexing_supported)
    qvk.dev_extension[len++] = VK_EXT_SHADER_64BIT_INDEXING_EXTENSION_NAME; // 64 bit ssbo addresses
  if(qvk.unified_image_layouts_supported)
    qvk.dev_extension[len++] = VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME; // general image layouts
  qvk.dev_extension[len++] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
  if(qvk.raytracing_supported)
  {
    qvk.dev_extension[len++] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_RAY_QUERY_EXTENSION_NAME;
  }
  if(qvk.float_atomics_supported) qvk.dev_extension[len++] = VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME;
  if(qvk.coopmat_supported) qvk.dev_extension[len++] = VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME;
  if(qvk.subgroup_size_control_supported) qvk.dev_extension[len++] = VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME;
#ifdef QVK_ENABLE_VALIDATION
  qvk.dev_extension[len++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
#endif
#ifdef __APPLE__
  qvk.dev_extension[len++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
#endif
  int vid_dec = 1;
  if(vid_dec)
  {
    qvk.dev_extension[len++] = VK_KHR_VIDEO_QUEUE_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
    qvk.dev_extension[len++] = VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME;
    // qvk.dev_extension[len++] = VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME;
  }
  if(window) qvk.dev_extension[len++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  if(enable_hdr_wsi) qvk.dev_extension[len++] = VK_EXT_HDR_METADATA_EXTENSION_NAME;

  VkDeviceQueueCreateInfo queue_create_infos[3];
  uint32_t num_queue_create_infos = 1;
  queue_create_infos[0] = queue_create_info;
  
  if(qvk.queue_family_compute != qvk.queue_family_graphics)
  {
    queue_create_infos[num_queue_create_infos++] = (VkDeviceQueueCreateInfo) {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueCount       = 1,
      .pQueuePriorities = queue_priorities,
      .queueFamilyIndex = qvk.queue_family_compute,
    };
  }
  if(qvk.queue_family_vid_dec != qvk.queue_family_graphics)
  {
    queue_create_infos[num_queue_create_infos++] = (VkDeviceQueueCreateInfo) {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      // ffmpeg doesn't get this:
      // .flags            = VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR, // XXX also if it *is* the graphics queue?
      .queueCount       = 1,
      .pQueuePriorities = queue_priorities,
      .queueFamilyIndex = qvk.queue_family_vid_dec,
    };
  }

  VkDeviceCreateInfo dev_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext                   = &device_features,
    .pQueueCreateInfos       = queue_create_infos,
    .queueCreateInfoCount    = num_queue_create_infos,
    .enabledExtensionCount   = len,
    .ppEnabledExtensionNames = qvk.dev_extension,
  };
  qvk.dev_extension_cnt = len;

  /* create device and queue */
  QVKR(vkCreateDevice(qvk.physical_device, &dev_create_info, NULL, &qvk.device));

  for(int k=0;k<s_queue_cnt;k++)
  {
    if(k == s_queue_compute && qvk.queue_family_compute != qvk.queue_family_graphics)
    {
      qvk.queue[k].idx = 0;
      qvk.queue[k].num = queue_compute_cnt;
      qvk.qid[k] = 1; // map queue name k to actual qvk.queue[.] index
      vkGetDeviceQueue(qvk.device, qvk.queue_family_compute, 0, &qvk.queue[k].queue);
      threads_mutex_init(&qvk.queue[k].mutex, 0);
      qvk.queue[k].family = qvk.queue_family_compute;
      dt_log(s_log_qvk, "queue %d is idx %d family %d (async compute)", k, qvk.qid[k], qvk.queue_family_compute);
    }
    else if(k == s_queue_vid_dec && qvk.queue_family_vid_dec != qvk.queue_family_graphics)
    {
      qvk.queue[k].idx = 0;
      qvk.queue[k].num = queue_vid_dec_cnt;
      qvk.qid[k] = k;
      VkDeviceQueueInfo2 qinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
        // ffmpeg 9 doesn't pick it up:
        // .flags = VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR,
        .queueFamilyIndex = qvk.queue_family_vid_dec,
        .queueIndex = 0,
      };
      vkGetDeviceQueue2(qvk.device, &qinfo,
         //  qvk.queue_family_vid_dec, VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR,
          &qvk.queue[k].queue);
      threads_mutex_init(&qvk.queue[k].mutex, 0);
      qvk.queue[k].family = qvk.queue_family_vid_dec;
      dt_log(s_log_qvk, "queue %d is idx %d family %d (video decode)", k, qvk.qid[k], qvk.queue_family_vid_dec);
    }
    else
    {
      qvk.queue[k].idx = MIN(queue_cnt-1, k);
      qvk.queue[k].num = queue_cnt;
      qvk.qid[k] = qvk.queue[k].idx;
      if(k == qvk.queue[k].idx)
      { // new unique index, need to construct all the things
        dt_log(s_log_qvk, "queue %d is idx %d family %d", k, qvk.qid[k], qvk.queue_family_graphics);
        vkGetDeviceQueue(qvk.device, qvk.queue_family_graphics, qvk.queue[k].idx, &qvk.queue[k].queue);
        threads_mutex_init(&qvk.queue[k].mutex, 0);
        qvk.queue[k].family = qvk.queue_family_graphics;
      }
      else qvk.queue[k].idx = -1; // mark as not initialised
    }
  }

#define _VK_EXTENSION_DO(a) \
    q##a = (PFN_##a) vkGetDeviceProcAddr(qvk.device, #a); \
    if(!q##a) { dt_log(s_log_qvk|s_log_err, "could not load function %s. do you have validation layers setup correctly?", #a); return VK_INCOMPLETE; }
  _VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

#ifdef __APPLE__
  VkPhysicalDevicePortabilitySubsetPropertiesKHR subprop = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR,
  };
#endif
  VkPhysicalDeviceVulkan11Properties prop11 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
#ifdef __APPLE__
    .pNext = &subprop,
#endif
  };
  VkPhysicalDeviceSubgroupProperties subgroup = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
    .pNext = &prop11,
  };
  VkPhysicalDeviceAccelerationStructurePropertiesKHR devprop_acc = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
    .pNext = &subgroup,
  };
  VkPhysicalDeviceProperties2 devprop = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = &devprop_acc,
  };
  vkGetPhysicalDeviceProperties2(qvk.physical_device, &devprop);
  qvk.raytracing_acc_min_align = devprop_acc.minAccelerationStructureScratchOffsetAlignment;
  qvk.max_allocation_size = prop11.maxMemoryAllocationSize;
  qvk.subgroup_size = subgroup.subgroupSize;
  qvk.subgroup_ops = subgroup.supportedOperations;

  // create texture samplers
  VkSamplerCreateInfo sampler_dspy_info = {
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter               = VK_FILTER_NEAREST,
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
  QVKR(vkCreateSampler(qvk.device, &sampler_dspy_info, NULL, &qvk.tex_sampler_dspy));
  ATTACH_LABEL_VARIABLE(qvk.tex_sampler_dspy, SAMPLER);
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

  return VK_SUCCESS;
}

int
qvk_cleanup()
{
  for(int k=0;k<s_queue_cnt;k++)
  {
    if(qvk.queue[k].idx >= 0)
    {
      QVKL(&qvk.queue[k].mutex, vkQueueWaitIdle(qvk.queue[k].queue));
      threads_mutex_destroy(&qvk.queue[k].mutex);
    }
  }
  vkDestroySampler(qvk.device, qvk.tex_sampler, 0);
  vkDestroySampler(qvk.device, qvk.tex_sampler_dspy, 0);
  vkDestroySampler(qvk.device, qvk.tex_sampler_nearest, 0);
  vkDestroySampler(qvk.device, qvk.tex_sampler_yuv, 0);
  vkDestroySamplerYcbcrConversion(qvk.device, qvk.yuv_conversion, 0);

  vkDestroyDevice      (qvk.device,   NULL);
  QVK(qvkDestroyDebugUtilsMessengerEXT(qvk.instance, qvk.dbg_messenger, NULL));
  vkDestroyInstance    (qvk.instance, NULL);

  free(qvk.layers);
  qvk.layers = NULL;
  qvk.num_layers = 0;

  return 0;
}
