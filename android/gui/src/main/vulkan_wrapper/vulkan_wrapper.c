// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// This file is generated.
#include "vulkan_wrapper.h"
#include <dlfcn.h>

int InitVulkan()
{
    void* libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan)
        return 0;

    // Vulkan supported, set function addresses
    vkCreateInstance = (dlsym(libvulkan, "vkCreateInstance"));
    vkDestroyInstance = (dlsym(libvulkan, "vkDestroyInstance"));
    vkEnumeratePhysicalDevices = (dlsym(libvulkan, "vkEnumeratePhysicalDevices"));
    vkGetPhysicalDeviceFeatures = (dlsym(libvulkan, "vkGetPhysicalDeviceFeatures"));
    vkGetPhysicalDeviceFormatProperties = (dlsym(libvulkan, "vkGetPhysicalDeviceFormatProperties"));
    vkGetPhysicalDeviceImageFormatProperties = (dlsym(libvulkan, "vkGetPhysicalDeviceImageFormatProperties"));
    vkGetPhysicalDeviceProperties = (dlsym(libvulkan, "vkGetPhysicalDeviceProperties"));
    vkGetPhysicalDeviceQueueFamilyProperties = (dlsym(libvulkan, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vkGetPhysicalDeviceMemoryProperties = (dlsym(libvulkan, "vkGetPhysicalDeviceMemoryProperties"));
    vkGetInstanceProcAddr = (dlsym(libvulkan, "vkGetInstanceProcAddr"));
    vkGetDeviceProcAddr = (dlsym(libvulkan, "vkGetDeviceProcAddr"));
    vkCreateDevice = (dlsym(libvulkan, "vkCreateDevice"));
    vkDestroyDevice = (dlsym(libvulkan, "vkDestroyDevice"));
    vkEnumerateInstanceExtensionProperties = (dlsym(libvulkan, "vkEnumerateInstanceExtensionProperties"));
    vkEnumerateDeviceExtensionProperties = (dlsym(libvulkan, "vkEnumerateDeviceExtensionProperties"));
    vkEnumerateInstanceLayerProperties = (dlsym(libvulkan, "vkEnumerateInstanceLayerProperties"));
    vkEnumerateDeviceLayerProperties = (dlsym(libvulkan, "vkEnumerateDeviceLayerProperties"));
    vkGetDeviceQueue = (dlsym(libvulkan, "vkGetDeviceQueue"));
    vkQueueSubmit = (dlsym(libvulkan, "vkQueueSubmit"));
    vkQueueWaitIdle = (dlsym(libvulkan, "vkQueueWaitIdle"));
    vkDeviceWaitIdle = (dlsym(libvulkan, "vkDeviceWaitIdle"));
    vkAllocateMemory = (dlsym(libvulkan, "vkAllocateMemory"));
    vkFreeMemory = (dlsym(libvulkan, "vkFreeMemory"));
    vkMapMemory = (dlsym(libvulkan, "vkMapMemory"));
    vkUnmapMemory = (dlsym(libvulkan, "vkUnmapMemory"));
    vkFlushMappedMemoryRanges = (dlsym(libvulkan, "vkFlushMappedMemoryRanges"));
    vkInvalidateMappedMemoryRanges = (dlsym(libvulkan, "vkInvalidateMappedMemoryRanges"));
    vkGetDeviceMemoryCommitment = (dlsym(libvulkan, "vkGetDeviceMemoryCommitment"));
    vkBindBufferMemory = (dlsym(libvulkan, "vkBindBufferMemory"));
    vkBindImageMemory = (dlsym(libvulkan, "vkBindImageMemory"));
    vkGetBufferMemoryRequirements = (dlsym(libvulkan, "vkGetBufferMemoryRequirements"));
    vkGetImageMemoryRequirements = (dlsym(libvulkan, "vkGetImageMemoryRequirements"));
    vkGetImageSparseMemoryRequirements = (dlsym(libvulkan, "vkGetImageSparseMemoryRequirements"));
    vkGetPhysicalDeviceSparseImageFormatProperties = (dlsym(libvulkan, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    vkQueueBindSparse = (dlsym(libvulkan, "vkQueueBindSparse"));
    vkCreateFence = (dlsym(libvulkan, "vkCreateFence"));
    vkDestroyFence = (dlsym(libvulkan, "vkDestroyFence"));
    vkResetFences = (dlsym(libvulkan, "vkResetFences"));
    vkGetFenceStatus = (dlsym(libvulkan, "vkGetFenceStatus"));
    vkWaitForFences = (dlsym(libvulkan, "vkWaitForFences"));
    vkCreateSemaphore = (dlsym(libvulkan, "vkCreateSemaphore"));
    vkDestroySemaphore = (dlsym(libvulkan, "vkDestroySemaphore"));
    vkGetSemaphoreCounterValue = (dlsym(libvulkan, "vkGetSemaphoreCounterValue"));
    vkWaitSemaphoes = (dlsym(libvulkan, "vkWaitSemaphores"));
    vkCreateEvent = (dlsym(libvulkan, "vkCreateEvent"));
    vkDestroyEvent = (dlsym(libvulkan, "vkDestroyEvent"));
    vkGetEventStatus = (dlsym(libvulkan, "vkGetEventStatus"));
    vkSetEvent = (dlsym(libvulkan, "vkSetEvent"));
    vkResetEvent = (dlsym(libvulkan, "vkResetEvent"));
    vkCreateQueryPool = (dlsym(libvulkan, "vkCreateQueryPool"));
    vkDestroyQueryPool = (dlsym(libvulkan, "vkDestroyQueryPool"));
    vkGetQueryPoolResults = (dlsym(libvulkan, "vkGetQueryPoolResults"));
    vkCreateBuffer = (dlsym(libvulkan, "vkCreateBuffer"));
    vkDestroyBuffer = (dlsym(libvulkan, "vkDestroyBuffer"));
    vkCreateBufferView = (dlsym(libvulkan, "vkCreateBufferView"));
    vkDestroyBufferView = (dlsym(libvulkan, "vkDestroyBufferView"));
    vkCreateImage = (dlsym(libvulkan, "vkCreateImage"));
    vkDestroyImage = (dlsym(libvulkan, "vkDestroyImage"));
    vkGetImageSubresourceLayout = (dlsym(libvulkan, "vkGetImageSubresourceLayout"));
    vkCreateImageView = (dlsym(libvulkan, "vkCreateImageView"));
    vkDestroyImageView = (dlsym(libvulkan, "vkDestroyImageView"));
    vkCreateShaderModule = (dlsym(libvulkan, "vkCreateShaderModule"));
    vkDestroyShaderModule = (dlsym(libvulkan, "vkDestroyShaderModule"));
    vkCreatePipelineCache = (dlsym(libvulkan, "vkCreatePipelineCache"));
    vkDestroyPipelineCache = (dlsym(libvulkan, "vkDestroyPipelineCache"));
    vkGetPipelineCacheData = (dlsym(libvulkan, "vkGetPipelineCacheData"));
    vkMergePipelineCaches = (dlsym(libvulkan, "vkMergePipelineCaches"));
    vkCreateGraphicsPipelines = (dlsym(libvulkan, "vkCreateGraphicsPipelines"));
    vkCreateComputePipelines = (dlsym(libvulkan, "vkCreateComputePipelines"));
    vkDestroyPipeline = (dlsym(libvulkan, "vkDestroyPipeline"));
    vkCreatePipelineLayout = (dlsym(libvulkan, "vkCreatePipelineLayout"));
    vkDestroyPipelineLayout = (dlsym(libvulkan, "vkDestroyPipelineLayout"));
    vkCreateSampler = (dlsym(libvulkan, "vkCreateSampler"));
    vkDestroySampler = (dlsym(libvulkan, "vkDestroySampler"));
    vkCreateDescriptorSetLayout = (dlsym(libvulkan, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout = (dlsym(libvulkan, "vkDestroyDescriptorSetLayout"));
    vkCreateDescriptorPool = (dlsym(libvulkan, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = (dlsym(libvulkan, "vkDestroyDescriptorPool"));
    vkResetDescriptorPool = (dlsym(libvulkan, "vkResetDescriptorPool"));
    vkAllocateDescriptorSets = (dlsym(libvulkan, "vkAllocateDescriptorSets"));
    vkFreeDescriptorSets = (dlsym(libvulkan, "vkFreeDescriptorSets"));
    vkUpdateDescriptorSets = (dlsym(libvulkan, "vkUpdateDescriptorSets"));
    vkCreateFramebuffer = (dlsym(libvulkan, "vkCreateFramebuffer"));
    vkDestroyFramebuffer = (dlsym(libvulkan, "vkDestroyFramebuffer"));
    vkCreateRenderPass = (dlsym(libvulkan, "vkCreateRenderPass"));
    vkDestroyRenderPass = (dlsym(libvulkan, "vkDestroyRenderPass"));
    vkGetRenderAreaGranularity = (dlsym(libvulkan, "vkGetRenderAreaGranularity"));
    vkCreateCommandPool = (dlsym(libvulkan, "vkCreateCommandPool"));
    vkDestroyCommandPool = (dlsym(libvulkan, "vkDestroyCommandPool"));
    vkResetCommandPool = (dlsym(libvulkan, "vkResetCommandPool"));
    vkAllocateCommandBuffers = (dlsym(libvulkan, "vkAllocateCommandBuffers"));
    vkFreeCommandBuffers = (dlsym(libvulkan, "vkFreeCommandBuffers"));
    vkBeginCommandBuffer = (dlsym(libvulkan, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = (dlsym(libvulkan, "vkEndCommandBuffer"));
    vkResetCommandBuffer = (dlsym(libvulkan, "vkResetCommandBuffer"));
    vkCmdBindPipeline = (dlsym(libvulkan, "vkCmdBindPipeline"));
    vkCmdSetViewport = (dlsym(libvulkan, "vkCmdSetViewport"));
    vkCmdSetScissor = (dlsym(libvulkan, "vkCmdSetScissor"));
    vkCmdSetLineWidth = (dlsym(libvulkan, "vkCmdSetLineWidth"));
    vkCmdSetDepthBias = (dlsym(libvulkan, "vkCmdSetDepthBias"));
    vkCmdSetBlendConstants = (dlsym(libvulkan, "vkCmdSetBlendConstants"));
    vkCmdSetDepthBounds = (dlsym(libvulkan, "vkCmdSetDepthBounds"));
    vkCmdSetStencilCompareMask = (dlsym(libvulkan, "vkCmdSetStencilCompareMask"));
    vkCmdSetStencilWriteMask = (dlsym(libvulkan, "vkCmdSetStencilWriteMask"));
    vkCmdSetStencilReference = (dlsym(libvulkan, "vkCmdSetStencilReference"));
    vkCmdBindDescriptorSets = (dlsym(libvulkan, "vkCmdBindDescriptorSets"));
    vkCmdBindIndexBuffer = (dlsym(libvulkan, "vkCmdBindIndexBuffer"));
    vkCmdBindVertexBuffers = (dlsym(libvulkan, "vkCmdBindVertexBuffers"));
    vkCmdDraw = (dlsym(libvulkan, "vkCmdDraw"));
    vkCmdDrawIndexed = (dlsym(libvulkan, "vkCmdDrawIndexed"));
    vkCmdDrawIndirect = (dlsym(libvulkan, "vkCmdDrawIndirect"));
    vkCmdDrawIndexedIndirect = (dlsym(libvulkan, "vkCmdDrawIndexedIndirect"));
    vkCmdDispatch = (dlsym(libvulkan, "vkCmdDispatch"));
    vkCmdDispatchIndirect = (dlsym(libvulkan, "vkCmdDispatchIndirect"));
    vkCmdCopyBuffer = (dlsym(libvulkan, "vkCmdCopyBuffer"));
    vkCmdCopyImage = (dlsym(libvulkan, "vkCmdCopyImage"));
    vkCmdBlitImage = (dlsym(libvulkan, "vkCmdBlitImage"));
    vkCmdCopyBufferToImage = (dlsym(libvulkan, "vkCmdCopyBufferToImage"));
    vkCmdCopyImageToBuffer = (dlsym(libvulkan, "vkCmdCopyImageToBuffer"));
    vkCmdUpdateBuffer = (dlsym(libvulkan, "vkCmdUpdateBuffer"));
    vkCmdFillBuffer = (dlsym(libvulkan, "vkCmdFillBuffer"));
    vkCmdClearColorImage = (dlsym(libvulkan, "vkCmdClearColorImage"));
    vkCmdClearDepthStencilImage = (dlsym(libvulkan, "vkCmdClearDepthStencilImage"));
    vkCmdClearAttachments = (dlsym(libvulkan, "vkCmdClearAttachments"));
    vkCmdResolveImage = (dlsym(libvulkan, "vkCmdResolveImage"));
    vkCmdSetEvent = (dlsym(libvulkan, "vkCmdSetEvent"));
    vkCmdResetEvent = (dlsym(libvulkan, "vkCmdResetEvent"));
    vkCmdWaitEvents = (dlsym(libvulkan, "vkCmdWaitEvents"));
    vkCmdPipelineBarrier = (dlsym(libvulkan, "vkCmdPipelineBarrier"));
    vkCmdBeginQuery = (dlsym(libvulkan, "vkCmdBeginQuery"));
    vkCmdEndQuery = (dlsym(libvulkan, "vkCmdEndQuery"));
    vkCmdResetQueryPool = (dlsym(libvulkan, "vkCmdResetQueryPool"));
    vkCmdWriteTimestamp = (dlsym(libvulkan, "vkCmdWriteTimestamp"));
    vkCmdCopyQueryPoolResults = (dlsym(libvulkan, "vkCmdCopyQueryPoolResults"));
    vkCmdPushConstants = (dlsym(libvulkan, "vkCmdPushConstants"));
    vkCmdBeginRenderPass = (dlsym(libvulkan, "vkCmdBeginRenderPass"));
    vkCmdNextSubpass = (dlsym(libvulkan, "vkCmdNextSubpass"));
    vkCmdEndRenderPass = (dlsym(libvulkan, "vkCmdEndRenderPass"));
    vkCmdExecuteCommands = (dlsym(libvulkan, "vkCmdExecuteCommands"));
    vkDestroySurfaceKHR = (dlsym(libvulkan, "vkDestroySurfaceKHR"));
    vkGetPhysicalDeviceSurfaceSupportKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vkGetPhysicalDeviceSurfaceFormatsKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vkGetPhysicalDeviceSurfacePresentModesKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    vkCreateSwapchainKHR = (dlsym(libvulkan, "vkCreateSwapchainKHR"));
    vkDestroySwapchainKHR = (dlsym(libvulkan, "vkDestroySwapchainKHR"));
    vkGetSwapchainImagesKHR = (dlsym(libvulkan, "vkGetSwapchainImagesKHR"));
    vkAcquireNextImageKHR = (dlsym(libvulkan, "vkAcquireNextImageKHR"));
    vkQueuePresentKHR = (dlsym(libvulkan, "vkQueuePresentKHR"));
    vkGetPhysicalDeviceDisplayPropertiesKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceDisplayPropertiesKHR"));
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR"));
    vkGetDisplayPlaneSupportedDisplaysKHR = (dlsym(libvulkan, "vkGetDisplayPlaneSupportedDisplaysKHR"));
    vkGetDisplayModePropertiesKHR = (dlsym(libvulkan, "vkGetDisplayModePropertiesKHR"));
    vkCreateDisplayModeKHR = (dlsym(libvulkan, "vkCreateDisplayModeKHR"));
    vkGetDisplayPlaneCapabilitiesKHR = (dlsym(libvulkan, "vkGetDisplayPlaneCapabilitiesKHR"));
    vkCreateDisplayPlaneSurfaceKHR = (dlsym(libvulkan, "vkCreateDisplayPlaneSurfaceKHR"));
    vkCreateSharedSwapchainsKHR = (dlsym(libvulkan, "vkCreateSharedSwapchainsKHR"));

#ifdef VK_USE_PLATFORM_XLIB_KHR
    vkCreateXlibSurfaceKHR = (dlsym(libvulkan, "vkCreateXlibSurfaceKHR"));
    vkGetPhysicalDeviceXlibPresentationSupportKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
    vkCreateXcbSurfaceKHR = (dlsym(libvulkan, "vkCreateXcbSurfaceKHR"));
    vkGetPhysicalDeviceXcbPresentationSupportKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceXcbPresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    vkCreateWaylandSurfaceKHR = (dlsym(libvulkan, "vkCreateWaylandSurfaceKHR"));
    vkGetPhysicalDeviceWaylandPresentationSupportKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceWaylandPresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_MIR_KHR
    vkCreateMirSurfaceKHR = (dlsym(libvulkan, "vkCreateMirSurfaceKHR"));
    vkGetPhysicalDeviceMirPresentationSupportKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceMirPresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    vkCreateAndroidSurfaceKHR = (dlsym(libvulkan, "vkCreateAndroidSurfaceKHR"));
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
    vkCreateWin32SurfaceKHR = (dlsym(libvulkan, "vkCreateWin32SurfaceKHR"));
    vkGetPhysicalDeviceWin32PresentationSupportKHR = (dlsym(libvulkan, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
#endif
#ifdef USE_DEBUG_EXTENTIONS
    vkCreateDebugReportCallbackEXT = (dlsym(libvulkan, "vkCreateDebugReportCallbackEXT"));
    vkDestroyDebugReportCallbackEXT = (dlsym(libvulkan, "vkDestroyDebugReportCallbackEXT"));
    vkDebugReportMessageEXT = (dlsym(libvulkan, "vkDebugReportMessageEXT"));
#endif
    return 1;
}

// No Vulkan support, do not set function addresses
PFN_vkCreateInstance vkCreateInstance;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
PFN_vkCreateDevice vkCreateDevice;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkFreeMemory vkFreeMemory;
PFN_vkMapMemory vkMapMemory;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
PFN_vkQueueBindSparse vkQueueBindSparse;
PFN_vkCreateFence vkCreateFence;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkResetFences vkResetFences;
PFN_vkGetFenceStatus vkGetFenceStatus;
PFN_vkWaitForFences vkWaitForFences;
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
PFN_vkWaitSemaphores vkWaitSemaphores;
PFN_vkCreateEvent vkCreateEvent;
PFN_vkDestroyEvent vkDestroyEvent;
PFN_vkGetEventStatus vkGetEventStatus;
PFN_vkSetEvent vkSetEvent;
PFN_vkResetEvent vkResetEvent;
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkDestroyBuffer vkDestroyBuffer;
PFN_vkCreateBufferView vkCreateBufferView;
PFN_vkDestroyBufferView vkDestroyBufferView;
PFN_vkCreateImage vkCreateImage;
PFN_vkDestroyImage vkDestroyImage;
PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
PFN_vkCreateImageView vkCreateImageView;
PFN_vkDestroyImageView vkDestroyImageView;
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
PFN_vkCreatePipelineCache vkCreatePipelineCache;
PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
PFN_vkMergePipelineCaches vkMergePipelineCaches;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkDestroyPipeline vkDestroyPipeline;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkCreateSampler vkCreateSampler;
PFN_vkDestroySampler vkDestroySampler;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkResetDescriptorPool vkResetDescriptorPool;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
PFN_vkResetCommandPool vkResetCommandPool;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkResetCommandBuffer vkResetCommandBuffer;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdSetViewport vkCmdSetViewport;
PFN_vkCmdSetScissor vkCmdSetScissor;
PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdCopyImage vkCmdCopyImage;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
PFN_vkCmdFillBuffer vkCmdFillBuffer;
PFN_vkCmdClearColorImage vkCmdClearColorImage;
PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
PFN_vkCmdClearAttachments vkCmdClearAttachments;
PFN_vkCmdResolveImage vkCmdResolveImage;
PFN_vkCmdSetEvent vkCmdSetEvent;
PFN_vkCmdResetEvent vkCmdResetEvent;
PFN_vkCmdWaitEvents vkCmdWaitEvents;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCmdBeginQuery vkCmdBeginQuery;
PFN_vkCmdEndQuery vkCmdEndQuery;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdNextSubpass vkCmdNextSubpass;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;

#ifdef VK_USE_PLATFORM_XLIB_KHR
PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_MIR_KHR
PFN_vkCreateMirSurfaceKHR vkCreateMirSurfaceKHR;
PFN_vkGetPhysicalDeviceMirPresentationSupportKHR vkGetPhysicalDeviceMirPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;

