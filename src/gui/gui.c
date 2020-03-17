#include "gui.h"
#include "qvk/qvk.h"
#include "core/log.h"
#include "core/threads.h"
#include "render.h"
#include "pipe/io.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

static void
style_to_state()
{
  const float pwd = vkdt.style.panel_width_frac * (16.0/9.0) * qvk.win_height;
  vkdt.state = (dt_gui_state_t) {
    .look_at_x = FLT_MAX,
    .look_at_y = FLT_MAX,
    .scale = -1.0f,
    .center_x = vkdt.style.border_frac * qvk.win_width,
    .center_y = vkdt.style.border_frac * qvk.win_width,
    .panel_wd = pwd,
    .center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - pwd,
    .center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width,
    .panel_ht = qvk.win_height,
    .anim_frame = vkdt.state.anim_frame,
    .anim_max_frame = vkdt.state.anim_max_frame,
  };
}
  
int dt_gui_init()
{
  memset(&vkdt, 0, sizeof(vkdt));
  glfwInit();
  if(!glfwVulkanSupported())
  {
    dt_log(s_log_gui|s_log_err, "no vulkan support found!");
    return 1;
  }

  vkdt.style.panel_width_frac = 0.2f;
  vkdt.style.border_frac = 0.02f;
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  // start "full screen"
  qvk.win_width  = mode->width;  //1920;
  qvk.win_height = mode->height; //1080;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  qvk.window = glfwCreateWindow(qvk.win_width, qvk.win_height, "vkdt", NULL, NULL);
  glfwSetWindowPos(qvk.window, 0, 0);

  if(!qvk.window)
  {
    dt_log(s_log_gui|s_log_err, "could not create GLFW window");
    return 1;
  }

  qvk.glfw_extensions = glfwGetRequiredInstanceExtensions(&qvk.num_glfw_extensions);
  if(!qvk.glfw_extensions)
  {
    dt_log(s_log_gui|s_log_err, "couldn't get GLFW instance extensions");
    return 1;
  }

  dt_log(s_log_gui, "vk extension required by GLFW:");
  for(int i = 0; i < qvk.num_glfw_extensions; i++)
    dt_log(s_log_gui, "  %s", qvk.glfw_extensions[i]);

  if(qvk_init())
  {
    dt_log(s_log_err|s_log_gui, "init vulkan failed");
    return 1;
  }

  /* create surface */
  if(glfwCreateWindowSurface(qvk.instance, qvk.window, NULL, &qvk.surface))
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

  QVKR(dt_gui_recreate_swapchain());

  // create command pool and fences and semaphores.
  // assume image_count does not change, so we only need to do this once.
  for(int i = 0; i < vkdt.image_count; i++)
  {
    VkCommandPoolCreateInfo cmd_pool_create_info = {
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = qvk.queue_idx_graphics,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    QVKR(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, vkdt.command_pool+i));

    VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = vkdt.command_pool[i],
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    QVKR(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, vkdt.command_buffer+i));

    VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    QVKR(vkCreateSemaphore(qvk.device, &semaphore_info, NULL, vkdt.sem_image_acquired + i));
    QVKR(vkCreateSemaphore(qvk.device, &semaphore_info, NULL, vkdt.sem_render_complete + i));

    VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT, /* fence's initial state set to be signaled to make program not hang */
    };
    QVKR(vkCreateFence(qvk.device, &fence_info, NULL, vkdt.fence + i));
  }
  vkdt.frame_index = 0;
  vkdt.sem_index = 0;
  // XXX intel says 0,0,0,1 is fastest:
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
  QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &vkdt.descriptor_pool));

  dt_gui_init_imgui();

  return 0;
}

// the following needs to be rerun on resize:
VkResult
dt_gui_recreate_swapchain()
{
  QVKR(vkDeviceWaitIdle(qvk.device));
  for(int i = 0; i < vkdt.image_count; i++)
    vkDestroyFramebuffer(qvk.device, vkdt.framebuffer[i], 0);
  if(vkdt.render_pass)
    vkDestroyRenderPass(qvk.device, vkdt.render_pass, 0);
  glfwGetWindowSize(qvk.window, &qvk.win_width, &qvk.win_height);
  style_to_state();
  QVKR(qvk_create_swapchain());

  // create the render pass
  VkAttachmentDescription attachment_desc = {
    .format = qvk.surf_format.format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    // clear enable?
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
  QVKR(vkCreateRenderPass(qvk.device, &info, 0, &vkdt.render_pass));

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
    QVKR(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, vkdt.framebuffer + i));
  }
  return VK_SUCCESS;
}

void dt_gui_cleanup()
{
  vkDestroyDescriptorPool(qvk.device, vkdt.descriptor_pool, 0);
  glfwDestroyWindow(qvk.window);
  glfwTerminate();
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
  threads_mutex_lock(&qvk.queue_mutex);
  QVK(vkQueueSubmit(qvk.queue_graphics, 1, &sub_info, vkdt.fence[i]));
  threads_mutex_unlock(&qvk.queue_mutex);
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

void
dt_gui_add_fav(
    dt_token_t module,
    dt_token_t inst,
    dt_token_t param)
{
  if(vkdt.fav_cnt >= sizeof(vkdt.fav_modid)/sizeof(vkdt.fav_modid[0]))
    return;

  int modid = dt_module_get(&vkdt.graph_dev, module, inst);
  if(modid < 0) return;
  int parid = dt_module_get_param(vkdt.graph_dev.module[modid].so, param);
  if(parid < 0) return;

  int i = vkdt.fav_cnt++;
  vkdt.fav_modid[i] = modid;
  vkdt.fav_parid[i] = parid;
}

int
dt_gui_read_favs(
    const char *filename)
{
  vkdt.fav_cnt = 0;
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;
  char buf[2048];
  while(!feof(f))
  {
    char *line = buf;
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    dt_token_t mod  = dt_read_token(line, &line);
    dt_token_t inst = dt_read_token(line, &line);
    dt_token_t parm = dt_read_token(line, &line);
    dt_gui_add_fav(mod, inst, parm);
  }
  fclose(f);
  return 0;
}
