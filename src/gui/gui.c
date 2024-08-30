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
style_to_state()
{
  const float pwd = vkdt.style.panel_width_frac * (16.0/9.0) * qvk.win_height;
  vkdt.state = (dt_gui_state_t) {
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

  vkdt.style.panel_width_frac = 0.2f;
  vkdt.style.border_frac = 0.02f;
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  // start "full screen"
  qvk.win_width  = 3*mode->width/4;
  qvk.win_height = 3*mode->height/4;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  glfwWindowHintString(GLFW_X11_CLASS_NAME, "vkdt");
  qvk.window = glfwCreateWindow(qvk.win_width, qvk.win_height, "vkdt", NULL, NULL);
  glfwSetWindowPos(qvk.window, qvk.win_width/8, qvk.win_height/8);

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

  const char *gpu_name = dt_rc_get(&vkdt.rc, "qvk/device_name", "null");
  if(!strcmp(gpu_name, "null")) gpu_name = 0;
  int gpu_id = dt_rc_get_int(&vkdt.rc, "qvk/device_id", -1);
  if(qvk_init(gpu_name, gpu_id))
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
  vkGetPhysicalDeviceSurfaceSupportKHR(qvk.physical_device, qvk.queue[qvk.qid[s_queue_graphics]].family, qvk.surface, &res);
  if (res != VK_TRUE)
  {
    dt_log(s_log_qvk|s_log_err, "no WSI support on physical device!");
    dt_log(s_log_qvk|s_log_err, "if you're on intel/amd, check if the selected device is the one with the display cable to the screen.");
    dt_log(s_log_qvk|s_log_err, "if you're on nvidia and wayland, this might be a driver issue (can you run `vulkaninfo` successfully?)");
    return 1;
  }

  QVKR(dt_gui_recreate_swapchain());

  // create command pool and fences and semaphores.
  // assume image_count does not change, so we only need to do this once.
  for(int i = 0; i < vkdt.image_count; i++)
  {
    VkCommandPoolCreateInfo cmd_pool_create_info = {
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = qvk.queue[qvk.qid[s_queue_graphics]].family,
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
  }
  vkdt.clear_value = (VkClearValue){{.float32={0.18f, 0.18f, 0.18f, 1.0f}}};

  vkdt.pipeline_cache = VK_NULL_HANDLE;
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
  QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &vkdt.descriptor_pool));

  // joystick detection:
  vkdt.wstate.have_joystick = glfwJoystickPresent(GLFW_JOYSTICK_1);
  if(vkdt.wstate.have_joystick)
  {
    const char *name = glfwGetJoystickName(GLFW_JOYSTICK_1);
    dt_log(s_log_gui, "found joystick %s", name);
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

// the following needs to be rerun on resize:
VkResult
dt_gui_recreate_swapchain()
{
  QVKLR(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[s_queue_graphics]].queue));
  for(int i = 0; i < vkdt.image_count; i++)
    vkDestroyFramebuffer(qvk.device, vkdt.framebuffer[i], 0);
  if(vkdt.render_pass)
    vkDestroyRenderPass(qvk.device, vkdt.render_pass, 0);
  glfwGetFramebufferSize(qvk.window, &qvk.win_width, &qvk.win_height);
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

    if(vkdt.sem_image_acquired[i])  vkDestroySemaphore(qvk.device, vkdt.sem_image_acquired[i], 0);
    if(vkdt.sem_render_complete[i]) vkDestroySemaphore(qvk.device, vkdt.sem_render_complete[i], 0);
    if(vkdt.fence[i])               vkDestroyFence(qvk.device, vkdt.fence[i], 0);
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
  return VK_SUCCESS;
}

void dt_gui_cleanup()
{
  dt_gui_cleanup_nk();
  char configfile[512];
  if(snprintf(configfile, sizeof(configfile), "%s/config.rc", dt_pipe.homedir) < 512)
    dt_rc_write(&vkdt.rc, configfile);
  dt_rc_cleanup(&vkdt.rc);
  dt_graph_cleanup(&vkdt.graph_dev);

  for(int i=0;i<vkdt.image_count;i++)
  {
    vkFreeCommandBuffers(qvk.device, vkdt.command_pool[i], 1, &vkdt.command_buffer[i]);
    vkDestroyCommandPool(qvk.device, vkdt.command_pool[i], 0);
    vkDestroySemaphore(qvk.device, vkdt.sem_image_acquired[i], 0);
    vkDestroySemaphore(qvk.device, vkdt.sem_render_complete[i], 0);
    vkDestroyFence(qvk.device, vkdt.fence[i], 0);
    vkDestroyFramebuffer(qvk.device, vkdt.framebuffer[i], 0);
  }
  if(vkdt.render_pass)
    vkDestroyRenderPass(qvk.device, vkdt.render_pass, 0);
  vkDestroyDescriptorPool(qvk.device, vkdt.descriptor_pool, 0);
  qvk_cleanup();
  glfwDestroyWindow(qvk.window);
  glfwTerminate();
  threads_mutex_destroy(&vkdt.wstate.notification_mutex);
}

VkResult dt_gui_render()
{
  VkSemaphore image_acquired_semaphore  = vkdt.sem_image_acquired [vkdt.sem_index];
  VkSemaphore render_complete_semaphore = vkdt.sem_render_complete[vkdt.sem_index];
  QVKR(vkWaitForFences(qvk.device, 1, vkdt.fence+vkdt.sem_fence[vkdt.sem_index], VK_TRUE, UINT64_MAX)); // make sure the semaphore is free
  // timeout is in nanoseconds (these are ~2sec)
  VkResult res = vkAcquireNextImageKHR(qvk.device, qvk.swap_chain, 2ul<<30, image_acquired_semaphore, VK_NULL_HANDLE, &vkdt.frame_index);
  if(res != VK_SUCCESS)
    return res;
  
  const int i = vkdt.frame_index;
  QVKR(vkWaitForFences(qvk.device, 1, vkdt.fence+i, VK_TRUE, UINT64_MAX));    // wait indefinitely instead of periodically checking
  QVKR(vkResetFences(qvk.device, 1, vkdt.fence+i));
  QVKR(vkResetCommandPool(qvk.device, vkdt.command_pool[i], 0));
  VkCommandBufferBeginInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  QVKR(vkBeginCommandBuffer(vkdt.command_buffer[i], &info));
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
  vkdt.sem_fence[vkdt.sem_index] = i; // remember which frame in flight uses the semaphores

  dt_gui_render_frame_nk();
  nk_glfw3_create_cmd(&vkdt.ctx, vkdt.command_buffer[i], NK_ANTI_ALIASING_ON, i);

  // submit command buffer
  vkCmdEndRenderPass(vkdt.command_buffer[i]);
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
    .pCommandBuffers      = vkdt.command_buffer+i,
    .signalSemaphoreCount = len,
    .pSignalSemaphores    = sem_signal,
  };

  QVKR(vkEndCommandBuffer(vkdt.command_buffer[i]));
  QVKLR(&qvk.queue[qvk.qid[s_queue_graphics]].mutex,
      vkQueueSubmit(qvk.queue[qvk.qid[s_queue_graphics]].queue, 1, &submit, vkdt.fence[i]));
  return res;
}

VkResult dt_gui_present()
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
  vkdt.sem_index = (vkdt.sem_index + 1) % vkdt.image_count;
  QVKLR(&qvk.queue[qvk.qid[s_queue_graphics]].mutex,
      vkQueuePresentKHR(qvk.queue[qvk.qid[s_queue_graphics]].queue, &info));
  return VK_SUCCESS;
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
  const char *filter_name[] = {"none", "filename", "rating", "label", "create date", "file type"};
  if(vkdt.db.collection_filter == s_prop_none)
  {
    snprintf(collstr, sizeof(collstr), "%s&all", vkdt.db.dirname);
  }
  else if(vkdt.db.collection_filter == s_prop_rating ||
          vkdt.db.collection_filter == s_prop_labels)
  {
    snprintf(collstr, sizeof(collstr), "%s&%s:%"PRIu64, vkdt.db.dirname, filter_name[vkdt.db.collection_filter], vkdt.db.collection_filter_val);
  }
  else if(vkdt.db.collection_filter == s_prop_filename ||
          vkdt.db.collection_filter == s_prop_createdate ||
          vkdt.db.collection_filter == s_prop_filetype)
  {
    snprintf(collstr, sizeof(collstr), "%s&%s:%"PRItkn, vkdt.db.dirname, filter_name[vkdt.db.collection_filter], dt_token_str(vkdt.db.collection_filter_val));
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
    end++;
    char filter[20] = {0};
    sscanf(end, "%[^:]", filter);
    if(!strcmp(filter, "all"))         vkdt.db.collection_filter = s_prop_none;
    if(!strcmp(filter, "filename"))    vkdt.db.collection_filter = s_prop_filename;
    if(!strcmp(filter, "rating"))      vkdt.db.collection_filter = s_prop_rating;
    if(!strcmp(filter, "label"))       vkdt.db.collection_filter = s_prop_labels;
    if(!strcmp(filter, "create date")) vkdt.db.collection_filter = s_prop_createdate;
    if(!strcmp(filter, "file type"))   vkdt.db.collection_filter = s_prop_filetype;
    end += strlen(filter);
    if(end[0] == ':' && end[1] != 0)
    {
      uint64_t num = 0;
      end++;
      if(vkdt.db.collection_filter == s_prop_rating ||
         vkdt.db.collection_filter == s_prop_labels)
      {
        sscanf(end, "%"PRIu64, &num);
      }
      else if(vkdt.db.collection_filter == s_prop_filename ||
              vkdt.db.collection_filter == s_prop_createdate ||
              vkdt.db.collection_filter == s_prop_filetype)
      {
        char inp[10] = {0};
        sscanf(end, "%8s", inp);
        num = dt_token(inp);
      }
      vkdt.db.collection_filter_val = num;
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

