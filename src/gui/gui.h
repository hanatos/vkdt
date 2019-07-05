#pragma once
#include "pipe/graph.h"

#include <vulkan/vulkan.h>

#define DT_GUI_MAX_IMAGES 8

typedef struct dt_widget_t
{
  int        modid;
  int        parid;
  dt_token_t type;
  float      min;      // min slider value
  float      max;
}
dt_widget_t;

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

  char             graph_cfg[2048];
  dt_graph_t       graph_dev;

  // center window configuration
  // TODO: put on display node, too?
  float view_look_at_x;
  float view_look_at_y;
  float view_scale;
  int   view_x, view_y;
  int   view_width;
  int   view_height;

  // current gui module/parameter configuration
  int num_widgets;
  dt_widget_t widget[20];
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

// add a widget for the given module instance and parameter name
void dt_gui_add_widget(
    dt_token_t module, dt_token_t inst, dt_token_t param, dt_token_t type,
    float min, float max);

// read a gui configuration from ascii config file
int dt_gui_read_ui_ascii(const char *filename);
