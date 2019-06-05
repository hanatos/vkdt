#include "gui.h"
#include "qvk/qvk.h"
#include "render.h"

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

  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(qvk.physical_device, qvk.queue_idx_graphics, qvk.surface, &res);
  if (res != VK_TRUE)
  {
    dt_log(s_log_qvk|s_log_err, "no WSI support on physical device");
    return 1;
  }

  QVK(qvk_create_swapchain());
  QVK(qvk_create_command_pool_and_fences());
  // QVK(qvk_initialize_all(VKPT_INIT_DEFAULT));

  dt_gui_init_imgui();

  return 0;
}

void dt_gui_cleanup(dt_gui_t *gui)
{
  SDL_DestroyWindow(qvk.window);
  SDL_Quit();
}

// TODO: need per swap chain image:
// fence
// command_pool
// command_buffer
// frame buffer

void dt_gui_render()
{
  VkSemaphore image_acquired_semaphore  = vkdt.sem_image_acquired [vkdt.sem_index];
  VkSemaphore render_complete_semaphore = vkdt.sem_render_complete[vkdt.sem_index];
  QVK(vkAcquireNextImageKHR(qvk.device, qvk.swap_chain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &vkdt.frame_index));

  dt_gui_frame_t *fr = vkdt.frame + vkdt.frame_index;
  QVK(vkWaitForFences(qvk.device, 1, fr->fence, VK_TRUE, UINT64_MAX));    // wait indefinitely instead of periodically checking
  QVK(vkResetFences(qvk.device, 1, fr->fence));
  QVK(vkResetCommandPool(qvk.device, fr->command_pool, 0));
  VkCommandBufferBeginInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  QVK(vkBeginCommandBuffer(fr->command_buffer, &info));
  VkRenderPassBeginInfo rp_info = {
    .sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass               = vkdt.render_pass,
    .framebuffer              = fr->frame_buffer,
    .renderArea.extent.width  = qvk.win_width,
    .renderArea.extent.height = qvk.win_height,
    .clearValueCount          = 1,
    .pClearValues             = vkdt.clear_value,
  };
  vkCmdBeginRenderPass(command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

  // Record Imgui Draw Data and draw funcs into command buffer
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fr->command_buffer);

  // Submit command buffer
  vkCmdEndRenderPass(fr->command_buffer);
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo sub_info = {
    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = &image_acquired_semaphore,
    .pWaitDstStageMask    = &wait_stage,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &fr->command_buffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &render_complete_semaphore,
  };

  QVK(vkEndCommandBuffer(fr->command_buffer));
  QVK(vkQueueSubmit(qvk.queue_graphics, 1, &sub_info, fr->fence));
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
    .pImageIndices      = &gui.frame_index,
  };
  QVK(vkQueuePresentKHR(qvk.queue_graphics, &info));
  vkdt.sem_index = (vkdt.sem_index + 1) % vkdt.image_count;
}
