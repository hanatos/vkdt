#include "gui.h"
#include "qvk/qvk.h"
#include "core/log.h"
#include "render.h"

#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_vulkan.h>
  
int dt_gui_init()
{
  qvk.win_width  = 1920;
  qvk.win_height = 1080;
  qvk.window = SDL_CreateWindow("vkdt", 20, 50,
      qvk.win_width, qvk.win_height,
      SDL_WINDOW_VULKAN);// | SDL_WINDOW_RESIZABLE); // XXX can't claim this until we implemented it!
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

  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(qvk.physical_device, qvk.queue_idx_graphics, qvk.surface, &res);
  if (res != VK_TRUE)
  {
    dt_log(s_log_qvk|s_log_err, "no WSI support on physical device");
    return 1;
  }

  // TODO: move these to here (need our own swapchain? but definitely our own command pool/things)
  // SDL_GetWindowSize(qvk.window, &qvk.win_width, &qvk.win_height);
  QVK(qvk_create_swapchain());
  // QVK(qvk_create_command_pool_and_fences());
  // QVK(qvk_initialize_all(VKPT_INIT_DEFAULT));

  // TODO: create our command pool and fences (see imgui_impl_vulkan.cpp)
  // ImGui_ImplVulkanH_CreateWindowSwapChain

  // create the render pass
  VkAttachmentDescription attachment_desc = {
    .format = qvk.surf_format.format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    // TODO clear enable?
    .loadOp = 1 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };
  VkAttachmentReference color_attachment = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };
  VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
  };
  VkSubpassDependency dependency = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };
  VkRenderPassCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &attachment_desc,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,
  };
  const VkAllocationCallbacks* allocator = 0;
  QVK(vkCreateRenderPass(qvk.device, &info, allocator, &vkdt.render_pass));

  // create framebuffers
  VkImageView attachment[1] = {};
  VkFramebufferCreateInfo fb_create_info = {
    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass      = vkdt.render_pass,
    .attachmentCount = 1,
    .pAttachments    = attachment,
    .width           = qvk.extent.width,
    .height          = qvk.extent.height,
    .layers          = 1,
  };
  vkdt.min_image_count = 2;
  vkdt.image_count = qvk.num_swap_chain_images;
  for(int i = 0; i < vkdt.image_count; i++)
  {
    attachment[0] = qvk.swap_chain_image_views[i];
    QVK(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, vkdt.framebuffer + i));
  }

  // create command pool and fences and semaphores
  for(int i = 0; i < vkdt.image_count; i++)
  {
    VkCommandPoolCreateInfo cmd_pool_create_info = {
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = qvk.queue_idx_graphics,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    QVK(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, vkdt.command_pool+i));

    VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = vkdt.command_pool[i],
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    QVK(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, vkdt.command_buffer+i));

    VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    QVK(vkCreateSemaphore(qvk.device, &semaphore_info, NULL, vkdt.sem_image_acquired + i));
    QVK(vkCreateSemaphore(qvk.device, &semaphore_info, NULL, vkdt.sem_render_complete + i));

    VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT, /* fence's initial state set to be signaled to make program not hang */
    };
    QVK(vkCreateFence(qvk.device, &fence_info, NULL, vkdt.fence + i));
  }
  vkdt.frame_index = 0;
  vkdt.sem_index = 0;
  vkdt.clear_value = (VkClearValue){{.float32={0.18f, 0.18f, 0.18f, 1.0f}}};

  vkdt.pipeline_cache = VK_NULL_HANDLE;
  VkDescriptorPoolSize pool_sizes[] =
  {
    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
  };
  VkDescriptorPoolCreateInfo pool_info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets       = 1000 * LENGTH(pool_sizes),
    .poolSizeCount = LENGTH(pool_sizes),
    .pPoolSizes    = pool_sizes,
  };
  QVK(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &vkdt.descriptor_pool));

  vkdt.view_look_at_x = FLT_MAX;
  vkdt.view_look_at_y = FLT_MAX;
  vkdt.view_scale = -1.0f;
  vkdt.view_x = 0;
  vkdt.view_y = 0;
  vkdt.view_width  = 1420;
  vkdt.view_height = 1080;
  dt_gui_init_imgui();

  return 0;
}

void dt_gui_cleanup()
{
  SDL_DestroyWindow(qvk.window);
  SDL_Quit();
}

void dt_gui_render()
{
  VkSemaphore image_acquired_semaphore  = vkdt.sem_image_acquired [vkdt.sem_index];
  VkSemaphore render_complete_semaphore = vkdt.sem_render_complete[vkdt.sem_index];
  QVK(vkAcquireNextImageKHR(qvk.device, qvk.swap_chain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &vkdt.frame_index));

  const int i = vkdt.frame_index;
  QVK(vkWaitForFences(qvk.device, 1, vkdt.fence+i, VK_TRUE, UINT64_MAX));    // wait indefinitely instead of periodically checking
  QVK(vkResetFences(qvk.device, 1, vkdt.fence+i));
  QVK(vkResetCommandPool(qvk.device, vkdt.command_pool[i], 0));
  VkCommandBufferBeginInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  QVK(vkBeginCommandBuffer(vkdt.command_buffer[i], &info));
  VkRenderPassBeginInfo rp_info = {
    .sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass               = vkdt.render_pass,
    .framebuffer              = vkdt.framebuffer[i],
    .renderArea.extent.width  = qvk.win_width,
    .renderArea.extent.height = qvk.win_height,
    .clearValueCount          = 1,
    .pClearValues             = &vkdt.clear_value,
  };
  vkCmdBeginRenderPass(vkdt.command_buffer[i], &rp_info, VK_SUBPASS_CONTENTS_INLINE);

  dt_gui_record_command_buffer_imgui(vkdt.command_buffer[i]);

  // Submit command buffer
  vkCmdEndRenderPass(vkdt.command_buffer[i]);
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo sub_info = {
    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = &image_acquired_semaphore,
    .pWaitDstStageMask    = &wait_stage,
    .commandBufferCount   = 1,
    .pCommandBuffers      = vkdt.command_buffer+i,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &render_complete_semaphore,
  };

  QVK(vkEndCommandBuffer(vkdt.command_buffer[i]));
  QVK(vkQueueSubmit(qvk.queue_graphics, 1, &sub_info, vkdt.fence[i]));
}

void dt_gui_present()
{
  VkSemaphore render_complete_semaphore = vkdt.sem_render_complete[vkdt.sem_index];
  VkPresentInfoKHR info = {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &render_complete_semaphore,
    .swapchainCount     = 1,
    .pSwapchains        = &qvk.swap_chain,
    .pImageIndices      = &vkdt.frame_index,
  };
  QVK(vkQueuePresentKHR(qvk.queue_graphics, &info));
  vkdt.sem_index = (vkdt.sem_index + 1) % vkdt.image_count;
}

void dt_gui_render_frame()
{
  dt_gui_render_frame_imgui();
}
