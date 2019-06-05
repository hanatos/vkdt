#pragma once

#include <vulkan/vulkan.h>

#define DT_GUI_MAX_IMAGES 8

typedef struct dt_gui_frame_t
{
  VkFence         fence;
  VkCommandPool   command_pool;
  VkCommandBuffer command_buffer;
  VkFramebuffer   frame_buffer;
}
dt_gui_frame_t;

typedef struct dt_gui_t
{
  VkRenderPass     render_pass;
  VkPipelineCache  pipeline_cache;
  VkDescriptorPool descriptor_pool;
  uint32_t         min_image_count;
  uint32_t         image_count;

  float            clear_value[3];   // TODO: more colours

  uint32_t         frame_index;
  dt_gui_frame_t   frame[DT_GUI_MAX_IMAGES];

  uint32_t         sem_index;
  VkSemaphore      sem_image_acquired [DT_GUI_MAX_IMAGES];
  VkSemaphore      sem_render_complete[DT_GUI_MAX_IMAGES];
}
dt_gui_t;

extern dt_gui_t vkdt;

int dt_gui_init(dt_gui_t *gui);
void dt_gui_cleanup(dt_gui_t *gui);
