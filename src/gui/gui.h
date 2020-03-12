#pragma once
#include "pipe/graph.h"
#include "db/thumbnails.h"
#include "db/db.h"

#include <vulkan/vulkan.h>

// max images in flight in vulkan pipeline/swap chain
#define DT_GUI_MAX_IMAGES 8

// view modes, lighttable, darkroom, ..
typedef enum dt_gui_view_t
{
  s_view_lighttable = 0,
  s_view_darkroom   = 1,
  s_view_cnt,
}
dt_gui_view_t;

// some global gui config options. this is a minimal
// set of static user input
typedef struct dt_gui_style_t
{
  float panel_width_frac;   // width of the side panel as fraction of the total window width
  float border_frac;        // width of border between image and panel
}
dt_gui_style_t;

// this is also gui style related, but has to be recomputed every time the
// window is resized and may contain user input.
typedef struct dt_gui_state_t
{
  // center window configuration
  float look_at_x;
  float look_at_y;
  float scale;
  int   center_x, center_y;
  int   center_wd, center_ht;
  int   panel_wd;
  int   panel_ht;

  // animation state
  int   anim_playing;
  int   anim_max_frame;
  int   anim_frame;
}
dt_gui_state_t;

// a few local things to exchange data between core and ui and c and c++
typedef struct dt_gui_wstate_t
{
  int    m_x, m_y;
  float  old_look_x, old_look_y;
  float  wd, ht;
  int    active_widget_modid;
  int    active_widget_parid;
  int    selected;
  float  state[2100];
  float *mapped;
  int    lod;
  float  connector[100][30][2];
}
dt_gui_wstate_t;

typedef struct dt_graph_t dt_graph_t;
typedef struct dt_gui_t
{
  VkRenderPass     render_pass;
  VkPipelineCache  pipeline_cache;
  VkDescriptorPool descriptor_pool;
  uint32_t         min_image_count;
  uint32_t         image_count;

  VkClearValue     clear_value;   // TODO: more colours
  dt_gui_style_t   style;
  dt_gui_state_t   state;
  dt_gui_wstate_t  wstate;

  uint32_t         frame_index;
  VkFence          fence         [DT_GUI_MAX_IMAGES];
  VkCommandPool    command_pool  [DT_GUI_MAX_IMAGES];
  VkCommandBuffer  command_buffer[DT_GUI_MAX_IMAGES];
  VkFramebuffer    framebuffer   [DT_GUI_MAX_IMAGES];

  uint32_t         sem_index;
  VkSemaphore      sem_image_acquired [DT_GUI_MAX_IMAGES];
  VkSemaphore      sem_render_complete[DT_GUI_MAX_IMAGES];

  dt_graph_t       graph_dev;

  dt_db_t          db;          // image list and current query
  dt_thumbnails_t  thumbnails;  // for light table mode
  dt_gui_view_t    view_mode;   // current view mode

  // favourite gui module/parameter list
  int fav_cnt;
  int fav_modid[20];
  int fav_parid[20];
}
dt_gui_t;

extern dt_gui_t vkdt;

int  dt_gui_init();
void dt_gui_cleanup();

// create or resize swapchain
VkResult dt_gui_recreate_swapchain();

// draws imgui things, implemented in render.cc
void dt_gui_render_frame_imgui();

// records and submits command buffer
void dt_gui_render();

// waits for semaphore and presents framebuffer
void dt_gui_present();

// add a widget for the given module instance and parameter name
void dt_gui_add_widget(
    dt_token_t module, dt_token_t inst, dt_token_t param, dt_token_t type,
    float min, float max);

// read a gui configuration from ascii config file
int dt_gui_read_favs(const char *filename);
