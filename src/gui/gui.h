#pragma once
#include "pipe/graph.h"

#include <vulkan/vulkan.h>

#define DT_GUI_MAX_IMAGES 8

typedef struct dt_graph_t dt_graph_t;
typedef struct dt_gui_t
{
  VkRenderPass     render_pass;
  VkPipelineCache  pipeline_cache;   // XXX
  VkDescriptorPool descriptor_pool;   // XXX
  uint32_t         min_image_count;
  uint32_t         image_count;

  VkClearValue     clear_value;   // TODO: more colours

  uint32_t         frame_index;
  VkFence          fence         [DT_GUI_MAX_IMAGES];
  VkCommandPool    command_pool  [DT_GUI_MAX_IMAGES];
  VkCommandBuffer  command_buffer[DT_GUI_MAX_IMAGES];
  VkFramebuffer    framebuffer   [DT_GUI_MAX_IMAGES];

  uint32_t         sem_index;
  VkSemaphore      sem_image_acquired [DT_GUI_MAX_IMAGES];
  VkSemaphore      sem_render_complete[DT_GUI_MAX_IMAGES];

  dt_graph_t       graph_dev;
}
dt_gui_t;

extern dt_gui_t vkdt;

int  dt_gui_init();
void dt_gui_cleanup();

// draws imgui things
void dt_gui_render_frame();

// records and submits command buffer
void dt_gui_render();

// waits for semaphore and presents framebuffer
void dt_gui_present();
