#include "gui.h"
#include "qvk/qvk.h"
#include "core/fs.h"
#include "core/log.h"
#include "core/threads.h"
#include "render.h"
#include "pipe/asciiio.h"
#include "pipe/modules/api.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <inttypes.h>

static void
window_size_callback(GLFWwindow* w, int width, int height)
{ // window resized, need to rebuild our swapchain:
  dt_gui_win_t *win = &vkdt.win;
  if(w == vkdt.win1.window) win = &vkdt.win1;
  dt_gui_recreate_swapchain(win);
  nk_glfw3_resize(w, win->width, win->height);
  if(w == vkdt.win.window) dt_gui_init_fonts();
}

static void
style_to_state()
{
  const float pwd = vkdt.style.panel_width_frac * (16.0/9.0) * vkdt.win.height;
  vkdt.state = (dt_gui_state_t) {
    .center_x   = vkdt.style.border_frac * vkdt.win.height,
    .center_y   = vkdt.style.border_frac * vkdt.win.height,
    .panel_wd   = pwd,
    .center_wd  = vkdt.win.width  - 2.0f*vkdt.style.border_frac * vkdt.win.height - pwd,
    .center_ht  = vkdt.win.height * (1.0f-2.0f*vkdt.style.border_frac),
    .panel_ht   = vkdt.win.height,
    .anim_frame = vkdt.state.anim_frame,
    .anim_max_frame = vkdt.state.anim_max_frame,
  };
}

static inline void
dt_gui_win_init(dt_gui_win_t *win)
{
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  win->width  = 3*mode->width/4;
  win->height = 3*mode->height/4;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  glfwWindowHintString(GLFW_X11_CLASS_NAME, "vkdt");
  win->window = glfwCreateWindow(win->width, win->height, "vkdt", NULL, NULL);
  glfwSetWindowPos(win->window, win->width/8, win->height/8);
  glfwSetWindowSizeCallback(win->window, window_size_callback);
}

static inline int
dt_gui_win_init_vk(dt_gui_win_t *win)
{
  /* create surface */
  if(glfwCreateWindowSurface(qvk.instance, win->window, NULL, &win->surface))
  {
    dt_log(s_log_qvk|s_log_err, "could not create surface!");
    return 1;
  }

  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(qvk.physical_device,
      qvk.queue[qvk.qid[s_queue_graphics]].family, win->surface, &res);
  if (res != VK_TRUE)
  {
    dt_log(s_log_qvk|s_log_err, "no WSI support on physical device!");
    dt_log(s_log_qvk|s_log_err, "if you're on intel/amd, check if the selected device is the one with the display cable to the screen.");
    dt_log(s_log_qvk|s_log_err, "if you're on nvidia and wayland, this might be a driver issue (can you run `vulkaninfo` successfully?)");
    return 1;
  }

  QVKR(dt_gui_recreate_swapchain(win));

  // create command pool and fences and semaphores.
  // assume num_swap_chain_images does not change, so we only need to do this once.
  for(int i = 0; i < win->num_swap_chain_images; i++)
  {
    VkCommandPoolCreateInfo cmd_pool_create_info = {
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = qvk.queue[qvk.qid[s_queue_graphics]].family,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    QVKR(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, win->command_pool+i));

    VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = win->command_pool[i],
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    QVKR(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, win->command_buffer+i));
  }

  win->pipeline_cache = VK_NULL_HANDLE;
  VkDescriptorPoolSize pool_sizes[] =
  {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 }, // Roboto Regular and MaterialIcons so far.
  };
  VkDescriptorPoolCreateInfo pool_info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets       = 2,
    .poolSizeCount = LENGTH(pool_sizes),
    .pPoolSizes    = pool_sizes,
  };
  QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &win->descriptor_pool));
  return 0;
}
  
int dt_gui_init()
{
  memset(&vkdt, 0, sizeof(vkdt));
  vkdt.graph_res = -1;
  if(!glfwInit())
  {
    const char* description;
    glfwGetError(&description);
    dt_log(s_log_gui|s_log_err, "glfwInit() failed: %s", description);
    return 1;
  }
  dt_log(s_log_gui, "glfwGetVersionString() : %s", glfwGetVersionString() );
  if(!glfwVulkanSupported())
  {
    const char* description;
    glfwGetError(&description);
    dt_log(s_log_gui|s_log_err, "glfwVulkanSupported(): no vulkan support found: %s", description);
    glfwTerminate(); // for correctness, if in future we don't just exit(1)
    return 1;
  }

  dt_rc_init(&vkdt.rc);
  char configfile[512];
  if(snprintf(configfile, sizeof(configfile), "%s/config.rc", dt_pipe.homedir) < 512)
    dt_rc_read(&vkdt.rc, configfile);

  vkdt.wstate.copied_imgid = -1u; // none copied at startup
  threads_mutex_init(&vkdt.wstate.notification_mutex, 0);

  const float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
  float rel_fontsize = 2.0f / 55.0f * dpi_scale;
  vkdt.style.panel_width_frac = 0.2f;
  vkdt.style.border_frac = rel_fontsize * 1.05f; // large enough to fit 2x fontsize heading
  dt_gui_win_init(&vkdt.win);

  // be verbose about monitor names so we can colour manage them:
  int monitors_cnt;
  GLFWmonitor** monitors = glfwGetMonitors(&monitors_cnt);
  for(int i=0;i<monitors_cnt;i++)
  {
    const char *name = glfwGetMonitorName(monitors[i]);
    int xpos, ypos;
    glfwGetMonitorPos(monitors[i], &xpos, &ypos);
    dt_log(s_log_gui, "monitor [%d] %s at %d %d", i, name, xpos, ypos);
    // the corresponding display profile gamma+matrix will be in basedir/profile.<name>
  }

  if(!vkdt.win.window)
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

  const char *gpu_name = dt_rc_get(&vkdt.rc, "qvk/device_name", "null");
  if(!strcmp(gpu_name, "null")) gpu_name = 0;
  int gpu_id = dt_rc_get_int(&vkdt.rc, "qvk/device_id", -1);
  if(qvk_init(gpu_name, gpu_id, 1))
  {
    dt_log(s_log_err|s_log_gui, "init vulkan failed");
    return 1;
  }

  dt_gui_win_init_vk(&vkdt.win);

  // joystick detection:
  vkdt.wstate.have_joystick = glfwJoystickPresent(GLFW_JOYSTICK_1);
  if(vkdt.wstate.have_joystick && glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
  {
    const char *name = glfwGetJoystickName(GLFW_JOYSTICK_1);
    dt_log(s_log_gui, "found gamepad %s", name);
    const int disable = dt_rc_get_int(&vkdt.rc, "gui/disable_joystick", 1);
    if(disable)
    {
      vkdt.wstate.have_joystick = 0;
      dt_log(s_log_gui, "disabling joystick due to explicit config request. enable by");
      dt_log(s_log_gui, "setting 'intgui/disable_joystick:0' in ~/.config/vkdt/config.rc");
    }
    else
    {
      vkdt.wstate.have_joystick = 1;
    }
  }
  else dt_log(s_log_gui, "no joysticks found");

  dt_gui_init_nk();

  return 0;
}

static inline VkResult
dt_gui_destroy_swapchain(dt_gui_win_t *win)
{
  for(int i = 0; i < win->num_swap_chain_images; i++)
  {
    if(win->swap_chain_image_views[i]) vkDestroyImageView  (qvk.device, win->swap_chain_image_views[i], 0);
    if(win->framebuffer[i])            vkDestroyFramebuffer(qvk.device, win->framebuffer[i], 0);
    if(win->render_pass)               vkDestroyRenderPass (qvk.device, win->render_pass, 0);
    win->swap_chain_image_views[i] = 0;
    win->framebuffer[i] = 0;
    win->render_pass = 0;
  }

  vkDestroySwapchainKHR(qvk.device, win->swap_chain, NULL);
  win->swap_chain = 0;
  return VK_SUCCESS;
}

VkResult
dt_gui_create_swapchain(dt_gui_win_t *win)
{
  VkSwapchainKHR old_swap_chain = win->swap_chain;
  QVKL(&qvk.queue[s_queue_graphics].mutex, vkQueueWaitIdle(qvk.queue[s_queue_graphics].queue));

  if(old_swap_chain)
    for(int i = 0; i < win->num_swap_chain_images; i++)
      vkDestroyImageView(qvk.device, win->swap_chain_image_views[i], 0);

  /* create swapchain */
  VkSurfaceCapabilitiesKHR surf_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(qvk.physical_device, win->surface, &surf_capabilities);

  uint32_t num_formats = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(qvk.physical_device, win->surface, &num_formats, NULL);
  VkSurfaceFormatKHR *avail_surface_formats = alloca(sizeof(VkSurfaceFormatKHR) * num_formats);
  vkGetPhysicalDeviceSurfaceFormatsKHR(qvk.physical_device, win->surface, &num_formats, avail_surface_formats);

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
        win->surf_format = avail_surface_formats[j];
        // please don't mess with our colour management.
        // see https://github.com/KhronosGroup/Vulkan-Docs/issues/2307
        // but apparently they will figure it out eventually.
        // it appears until then the default behaviour might be already what we need.
        // in fact it seems the situation is even worse. if i try to do this:
        // qvk.surf_format.colorSpace = VK_COLOR_SPACE_PASS_THROUGH_EXT;
        // the validation layers shout at me. this means there seems to be *no* way
        // to ask for pass through behaviour (and if there was it would be ignored downstream, see above)
        // fts
        dt_log(s_log_qvk, "using %s and colour space %d",
            qvk_format_to_string(win->surf_format.format), win->surf_format.colorSpace);
        goto out;
      }
  }
out:;

  uint32_t num_present_modes = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(qvk.physical_device, win->surface, &num_present_modes, NULL);
  VkPresentModeKHR *avail_present_modes = alloca(sizeof(VkPresentModeKHR) * num_present_modes);
  vkGetPhysicalDeviceSurfacePresentModesKHR(qvk.physical_device, win->surface, &num_present_modes, avail_present_modes);
  win->present_mode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be there, but has vsync frame time jitter

  VkExtent2D extent;
  if(surf_capabilities.currentExtent.width != ~0u)
  {
    extent = surf_capabilities.minImageExtent;
  }
  else
  {
    extent.width  = MIN(surf_capabilities.maxImageExtent.width,  win->width);
    extent.height = MIN(surf_capabilities.maxImageExtent.height, win->height);

    extent.width  = MAX(surf_capabilities.minImageExtent.width,  extent.width);
    extent.height = MAX(surf_capabilities.minImageExtent.height, extent.height);
  }

  // this is stupid, but it seems if the window manager does not allow going fullscreen
  // it crashes otherwise. sometimes you need to first make the window floating in dwm
  // before going F11 -> fullscreen works. sigh.
  win->width  = extent.width;
  win->height = extent.height;

  uint32_t num_images = surf_capabilities.minImageCount;
  if(surf_capabilities.maxImageCount > 0)
    num_images = MIN(num_images, surf_capabilities.maxImageCount);

  VkSwapchainCreateInfoKHR swpch_create_info = {
    .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface               = win->surface,
    .minImageCount         = num_images,
    .imageFormat           = win->surf_format.format,
    .imageColorSpace       = win->surf_format.colorSpace,
    .imageExtent           = extent,
    .imageArrayLayers      = 1, /* only needs to be changed for stereoscopic rendering */ 
    .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                           | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE, /* VK_SHARING_MODE_CONCURRENT if not using same queue */
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = NULL,
    .preTransform          = surf_capabilities.currentTransform,
    .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, /* no alpha for window transparency */
    .presentMode           = win->present_mode,
    .clipped               = VK_FALSE, /* do not render pixels that are occluded by other windows */
    .oldSwapchain          = old_swap_chain, /* need to provide previous swapchain in case of window resize */
  };

  QVKR(vkCreateSwapchainKHR(qvk.device, &swpch_create_info, NULL, &win->swap_chain));

  vkGetSwapchainImagesKHR(qvk.device, win->swap_chain, &win->num_swap_chain_images, NULL);
  assert(vkdt.win.num_swap_chain_images <= QVK_MAX_SWAPCHAIN_IMAGES);
  vkGetSwapchainImagesKHR(qvk.device, win->swap_chain, &win->num_swap_chain_images, win->swap_chain_images);

  for(int i = 0; i < win->num_swap_chain_images; i++)
  {
    VkImageViewCreateInfo img_create_info = {
      .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image      = win->swap_chain_images[i],
      .viewType   = VK_IMAGE_VIEW_TYPE_2D,
      .format     = win->surf_format.format,
      .subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
      }
    };

    QVKR(vkCreateImageView(qvk.device, &img_create_info, NULL, win->swap_chain_image_views + i));
  }

  if(old_swap_chain)
    vkDestroySwapchainKHR(qvk.device, old_swap_chain, 0);

  return VK_SUCCESS;
}

// the following needs to be rerun on resize:
VkResult
dt_gui_recreate_swapchain(dt_gui_win_t *win)
{
  QVKLR(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[s_queue_graphics]].queue));
  for(int i = 0; i < win->num_swap_chain_images; i++)
    vkDestroyFramebuffer(qvk.device, win->framebuffer[i], 0);
  if(win->render_pass)
    vkDestroyRenderPass(qvk.device, win->render_pass, 0);
  glfwGetFramebufferSize(win->window, &win->width, &win->height);
  style_to_state();
  QVKR(dt_gui_create_swapchain(win));

  // create the render pass
  VkAttachmentDescription attachment_desc = {
    .format = win->surf_format.format,
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
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
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
  QVKR(vkCreateRenderPass(qvk.device, &info, 0, &win->render_pass));

  // XXX TODO need to pass new render pass to nk/glfw3?

  // create framebuffers
  VkImageView attachment[1] = {};
  VkFramebufferCreateInfo fb_create_info = {
    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass      = win->render_pass,
    .attachmentCount = 1,
    .pAttachments    = attachment,
    .width           = win->width,
    .height          = win->height,
    .layers          = 1,
  };
  for(int i = 0; i < win->num_swap_chain_images; i++)
  {
    attachment[0] = win->swap_chain_image_views[i];
    QVKR(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, win->framebuffer + i));

    if(win->sem_image_acquired[i])  vkDestroySemaphore(qvk.device, win->sem_image_acquired[i], 0);
    if(win->sem_render_complete[i]) vkDestroySemaphore(qvk.device, win->sem_render_complete[i], 0);
    if(win->fence[i])               vkDestroyFence(qvk.device, win->fence[i], 0);
    VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    QVKR(vkCreateSemaphore(qvk.device, &semaphore_info, NULL, win->sem_image_acquired + i));
    QVKR(vkCreateSemaphore(qvk.device, &semaphore_info, NULL, win->sem_render_complete + i));

    VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT, /* fence's initial state set to be signaled to make program not hang */
    };
    QVKR(vkCreateFence(qvk.device, &fence_info, NULL, win->fence + i));
  }
  win->frame_index = 0;
  win->sem_index = 0;
  return VK_SUCCESS;
}

static inline void
dt_gui_win_cleanup(dt_gui_win_t *win)
{
  for(int i=0;i<win->num_swap_chain_images;i++)
  {
    vkFreeCommandBuffers(qvk.device, win->command_pool[i], 1, &win->command_buffer[i]);
    vkDestroyCommandPool(qvk.device, win->command_pool[i], 0);
    vkDestroySemaphore(qvk.device, win->sem_image_acquired[i], 0);
    vkDestroySemaphore(qvk.device, win->sem_render_complete[i], 0);
    vkDestroyFence(qvk.device, win->fence[i], 0);
    vkDestroyFramebuffer(qvk.device, win->framebuffer[i], 0);
    win->command_buffer[i] = 0;
    win->command_pool[i] = 0;
    win->sem_image_acquired[i] = 0;
    win->sem_render_complete[i] = 0;
    win->fence[i] = 0;
    win->framebuffer[i] = 0;
  }
  if(win->render_pass)
    vkDestroyRenderPass(qvk.device, win->render_pass, 0);
  win->render_pass = 0;
  vkDestroyDescriptorPool(qvk.device, win->descriptor_pool, 0);
  win->descriptor_pool = 0;

  if(win->window)  dt_gui_destroy_swapchain(win);
  if(win->surface) vkDestroySurfaceKHR(qvk.instance, win->surface, NULL);
  glfwDestroyWindow(win->window);
  win->window = 0;
  win->surface = 0;
}

void dt_gui_cleanup()
{
  dt_gui_win1_close();
  dt_gui_cleanup_nk();
  char configfile[512];
  if(snprintf(configfile, sizeof(configfile), "%s/config.rc", dt_pipe.homedir) < 512)
    dt_rc_write(&vkdt.rc, configfile);
  dt_rc_cleanup(&vkdt.rc);
  dt_graph_cleanup(&vkdt.graph_dev);

  dt_gui_win_cleanup(&vkdt.win);

  qvk_cleanup();
  glfwTerminate();
  threads_mutex_destroy(&vkdt.wstate.notification_mutex);
}

static inline VkResult
dt_gui_win_render(struct nk_context *ctx, dt_gui_win_t *win)
{
  VkSemaphore render_complete_semaphore = win->sem_render_complete[win->sem_index];
  VkSemaphore image_acquired_semaphore  = win->sem_image_acquired [win->sem_index];
  QVKR(vkWaitForFences(qvk.device, 1, win->fence+win->sem_fence[win->sem_index], VK_TRUE, UINT64_MAX)); // make sure the semaphore is free
  // timeout is in nanoseconds (these are ~2sec)
  VkResult res = vkAcquireNextImageKHR(qvk.device, win->swap_chain, 2ul<<30, image_acquired_semaphore, VK_NULL_HANDLE, &win->frame_index);
  if(res != VK_SUCCESS)
    return res;
  
  const int i = win->frame_index;
  QVKR(vkWaitForFences(qvk.device, 1, win->fence+i, VK_TRUE, UINT64_MAX));    // wait indefinitely instead of periodically checking
  QVKR(vkResetFences(qvk.device, 1, win->fence+i));
  QVKR(vkResetCommandPool(qvk.device, win->command_pool[i], 0));
  VkCommandBufferBeginInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  QVKR(vkBeginCommandBuffer(win->command_buffer[i], &info));
  VkClearValue clear_value = {{.float32={0.18f, 0.18f, 0.18f, 1.0f}}};
  VkRenderPassBeginInfo rp_info = {
    .sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass               = win->render_pass,
    .framebuffer              = win->framebuffer[i],
    .renderArea.extent.width  = win->width,
    .renderArea.extent.height = win->height,
    .clearValueCount          = 1,
    .pClearValues             = &clear_value,
  };
  vkCmdBeginRenderPass(win->command_buffer[i], &rp_info, VK_SUBPASS_CONTENTS_INLINE);
  win->sem_fence[win->sem_index] = i; // remember which frame in flight uses the semaphores

  nk_glfw3_create_cmd(ctx, win->window, win->command_buffer[i], NK_ANTI_ALIASING_ON, i, win->num_swap_chain_images);

  // submit command buffer
  vkCmdEndRenderPass(win->command_buffer[i]);
  const int len = vkdt.graph_res == VK_SUCCESS ? 2 : 1;
  VkPipelineStageFlags wait_stage[] = { 
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, };
  if(len > 1) // we are adding one more command list reading the current double buf
    vkdt.graph_dev.display_dbuffer[vkdt.graph_dev.double_buffer] = MAX(vkdt.graph_dev.display_dbuffer[0], vkdt.graph_dev.display_dbuffer[1]) + 1;
  uint64_t value_wait  [] = { 0, vkdt.graph_dev.process_dbuffer[vkdt.graph_dev.double_buffer] };
  uint64_t value_signal[] = { 0, vkdt.graph_dev.display_dbuffer[vkdt.graph_dev.double_buffer] };
  VkTimelineSemaphoreSubmitInfo timeline_info = {
    .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount   = len,
    .pWaitSemaphoreValues      = value_wait,
    .signalSemaphoreValueCount = len,
    .pSignalSemaphoreValues    = value_signal,
  };
  VkSemaphore sem_wait  [] = { image_acquired_semaphore,  vkdt.graph_dev.semaphore_process };
  VkSemaphore sem_signal[] = { render_complete_semaphore, vkdt.graph_dev.semaphore_display };
  VkSubmitInfo submit = {
    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext                = &timeline_info,
    .waitSemaphoreCount   = len,
    .pWaitSemaphores      = sem_wait,
    .pWaitDstStageMask    = wait_stage,
    .commandBufferCount   = 1,
    .pCommandBuffers      = win->command_buffer+i,
    .signalSemaphoreCount = len,
    .pSignalSemaphores    = sem_signal,
  };

  QVKR(vkEndCommandBuffer(win->command_buffer[i]));
  QVKLR(&qvk.queue[qvk.qid[s_queue_graphics]].mutex,
      vkQueueSubmit(qvk.queue[qvk.qid[s_queue_graphics]].queue, 1, &submit, win->fence[i]));
  return VK_SUCCESS;
}

VkResult dt_gui_render()
{
  dt_gui_render_frame_nk(); // potentially set off commands for both ctx/win
  VkResult res = 0;
  if(vkdt.win1.window) res = dt_gui_win_render(&vkdt.ctx1, &vkdt.win1);
  return res | dt_gui_win_render(&vkdt.ctx, &vkdt.win);
}

static inline VkResult
dt_gui_present_win(dt_gui_win_t *win)
{
  VkSemaphore render_complete_semaphore = win->sem_render_complete[win->sem_index];
  VkPresentInfoKHR info = {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &render_complete_semaphore,
    .swapchainCount     = 1,
    .pSwapchains        = &win->swap_chain,
    .pImageIndices      = &win->frame_index,
  };
  win->sem_index = (win->sem_index + 1) % win->num_swap_chain_images;
  QVKLR(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueuePresentKHR(qvk.queue[qvk.qid[s_queue_graphics]].queue, &info));
  return VK_SUCCESS;
}

VkResult dt_gui_present()
{
  VkResult res = 0;
  if(vkdt.win1.window) res = dt_gui_present_win(&vkdt.win1);
  return res | dt_gui_present_win(&vkdt.win);
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
  FILE *f = 0;
  char tmp[PATH_MAX+100] = {0};
  if(filename[0] == '/')
    f = fopen(filename, "rb");
  else
  {
    snprintf(tmp, sizeof(tmp), "%s/%s", vkdt.db.basedir, filename); // try ~/.config/vkdt/ first
    f = fopen(tmp, "rb");
    if(!f)
    {
      snprintf(tmp, sizeof(tmp), "%s/%s", dt_pipe.basedir, filename);
      f = fopen(tmp, "rb");
    }
  }
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

void
dt_gui_read_tags()
{
  vkdt.tag_cnt = 0;
  uint64_t time[sizeof(vkdt.tag)/sizeof(vkdt.tag[0])];
  char filename[PATH_MAX+10];
  snprintf(filename, sizeof(filename), "%s/tags", vkdt.db.basedir);
  DIR *dir = opendir(filename);
  if(!dir) return; // could not open tags directory, possibly we have none.
  struct dirent *ep;
  while((ep = readdir(dir)))
  {
    if(fs_isdir(filename, ep))
    {
      if(!strcmp(ep->d_name, "." )) continue;
      if(!strcmp(ep->d_name, "..")) continue;
      snprintf(filename, sizeof(filename), "%s/tags/%s", vkdt.db.basedir, ep->d_name);
      uint64_t t = fs_createtime(filename);
      if(vkdt.tag_cnt < sizeof(vkdt.tag)/sizeof(vkdt.tag[0]))
      { // add
        int i = vkdt.tag_cnt++;
        memcpy(vkdt.tag[i], ep->d_name, sizeof(vkdt.tag[0]));
        time[i] = t;
      }
      else
      { // lru replace:
        int ii = 0;
        for(int i=1;i<vkdt.tag_cnt;i++)
          if(time[i] < time[ii]) ii = i;
        memcpy(vkdt.tag[ii], ep->d_name, sizeof(vkdt.tag[0]));
        time[ii] = t;
      }
    }
  }
  closedir(dir);
  // sort tags alphabetically, in ugly and slow:
  qsort(vkdt.tag, vkdt.tag_cnt, sizeof(vkdt.tag[0]), (int(*)(const void*,const void*))strcmp);
}

void dt_gui_update_recently_used_collections()
{
  int32_t num = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/ruc_num", 0), 0, 10);
  char entry[512], saved[2][1018], collstr[512];
  int32_t j=0;
  // instead of vkdt.db.dirname use a new string that has &filter:value appended, except if filter is 0
  char *c = collstr;
  size_t len = sizeof(collstr);
  int off = snprintf(c, len, "%s", vkdt.db.dirname);
  c += off; len -= off;
  dt_db_filter_t *ft = &vkdt.db.collection_filter;
  if(len > 0 && !ft->active)
  {
    off = snprintf(c, len, "&all");
    c += off; len -= off;
  }
  if(len > 0 && (ft->active & (1<<s_prop_filename)))
  {
    off = snprintf(c, len, "&filename:%s", ft->filename);
    c += off; len -= off;
  }
  if(len > 0 && (ft->active & (1<<s_prop_rating)))
  {
    off = snprintf(c, len, "&rating:%u", ft->rating);
    c += off; len -= off;
  }
  if(len > 0 && (ft->active & (1<<s_prop_labels)))
  {
    off = snprintf(c, len, "&labels:%u", ft->labels);
    c += off; len -= off;
  }
  if(len > 0 && (ft->active & (1<<s_prop_createdate)))
  {
    off = snprintf(c, len, "&createdate:%s", ft->createdate);
    c += off; len -= off;
  }
  if(len > 0 && (ft->active & (1<<s_prop_filetype)))
  {
    off = snprintf(c, len, "&filetype:%s", dt_token_str(ft->filetype));
    c += off; len -= off;
  }
  snprintf(saved[1], sizeof(saved[1]), "%s", collstr);
  for(int32_t i=0;i<=num;i++)
  { // compact the list by deleting all duplicate entries
    snprintf(entry, sizeof(entry), "gui/ruc_entry%02d", i);
    const char *read_dir = i == num ? 0 : dt_rc_get(&vkdt.rc, entry, "null");
    if(read_dir && strcmp(read_dir, collstr)) snprintf(saved[i&1], sizeof(saved[i&1]), "%s", read_dir); // take a copy
    else saved[i&1][0] = 0;
    snprintf(entry, sizeof(entry), "gui/ruc_entry%02d", j);
    if(saved[(i+1)&1][0]) { dt_rc_set(&vkdt.rc, entry, saved[(i+1)&1]); j++; }
    saved[(i+1)&1][0] = 0;
  }
  dt_rc_set_int(&vkdt.rc, "gui/ruc_num", MIN(j, 10));
}

void dt_gui_switch_collection(const char *dir)
{
  char *end = 0;
  if(dir)
  { // find '&' in dir and replace by '\0', remember the position
    const int len = strlen(dir);
    for(int i=0;i<len;i++) if(dir[i] == '&') { end = (char *)(dir + i); break; }
    if(end) *end = 0;
  }

  vkdt.wstate.copied_imgid = -1u; // invalidate
  dt_thumbnails_cache_abort(&vkdt.thumbnail_gen); // this is essential since threads depend on db
  dt_db_cleanup(&vkdt.db);
  dt_db_init(&vkdt.db);
  QVKL(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[s_queue_graphics]].queue));
  dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, dir);
  dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);

  if(end)
  { // now set db filter based on stuff we parse behind &
    *end = '&'; // restore & (because the string was const, right..?)
    dt_db_filter_t *ft = &vkdt.db.collection_filter;
    ft->active = 0;
    for(int i=0;i<s_prop_cnt;i++)
    {
      end++;
      char filter[20] = {0}, val[30] = {0};
      sscanf(end, "%[^:]:%[^&]", filter, val);
      if(!strcmp(filter, "all"))         ft->active = s_prop_none;
      if(!strcmp(filter, "filename"))    { ft->active |= 1<<s_prop_filename;   snprintf(ft->filename, sizeof(ft->filename), "%s", val); }
      if(!strcmp(filter, "rating"))      { ft->active |= 1<<s_prop_rating;     ft->rating = atol(val); }
      if(!strcmp(filter, "labels"))      { ft->active |= 1<<s_prop_labels;     ft->labels = atol(val); }
      if(!strcmp(filter, "createdate"))  { ft->active |= 1<<s_prop_createdate; snprintf(ft->createdate, sizeof(ft->createdate), "%s", val); }
      if(!strcmp(filter, "filetype"))    { ft->active |= 1<<s_prop_filetype;   snprintf(dt_token_str(ft->filetype), 8, "%.8s", val); }
      while(*end != '&' && *end != 0) end++;
      if(*end == 0) break;
    }
    dt_db_update_collection(&vkdt.db);
  }
  dt_gui_update_recently_used_collections();
}

void dt_gui_notification(const char *msg, ...)
{
  threads_mutex_lock(&vkdt.wstate.notification_mutex);
  va_list args;
  va_start(args, msg);
  vsnprintf(vkdt.wstate.notification_msg, sizeof(vkdt.wstate.notification_msg), msg, args);
  vkdt.wstate.notification_time = glfwGetTime();
  va_end(args);
  threads_mutex_unlock(&vkdt.wstate.notification_mutex);
}

void dt_gui_win1_close()
{
  if(!vkdt.win1.window) return;
  QVKL(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[s_queue_graphics]].queue));
  dt_gui_win_cleanup(&vkdt.win1);
  nk_glfw3_win1_close();
  nk_free(&vkdt.ctx1);
}

void dt_gui_win1_open()
{
  if(vkdt.win1.window) dt_gui_win1_close();
  dt_gui_win_init(&vkdt.win1);
  dt_gui_win_init_vk(&vkdt.win1);
  nk_init_default(&vkdt.ctx1, 0);
  nk_style_default(&vkdt.ctx1);
  nk_style_from_table(&vkdt.ctx1, vkdt.style.colour);
  nk_style_set_font(&vkdt.ctx1, &dt_gui_get_font(0)->handle);
  nk_glfw3_win1_open(&vkdt.ctx1, vkdt.win1.render_pass, vkdt.win1.window, 
      vkdt.win1.num_swap_chain_images * 2560*1024,
      vkdt.win1.num_swap_chain_images * 640*1024);
  // XXX TODO glfw callbacks for resize, close, maybe some buttons for fullscreen/close
}
