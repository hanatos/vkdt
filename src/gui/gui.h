#pragma once
#include "pipe/graph.h"
#include "db/thumbnails.h"
#include "db/db.h"
#include "db/rc.h"
#include "snd/snd.h"

#include <vulkan/vulkan.h>

// max images in flight in vulkan pipeline/swap chain
#define DT_GUI_MAX_IMAGES 8

// view modes, lighttable, darkroom, ..
typedef enum dt_gui_view_t
{
  s_view_files      = 0,
  s_view_lighttable = 1,
  s_view_darkroom   = 2,
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
  int   anim_playing;        // playing yes/no
  int   anim_max_frame;      // last frame in animation
  int   anim_frame;          // current frame in animation
  int   anim_no_keyframes;   // used to temporarily switch off keyframes (during grabbed input for instance)
}
dt_gui_state_t;

// a few local things to exchange data between core and ui and c and c++
typedef struct dt_gui_wstate_t
{
  int      m_x, m_y;
  float    old_look_x, old_look_y;
  float    wd, ht;
  int      active_widget_modid; // module id
  int      active_widget_parid; // parameter id
  int      active_widget_parnm; // number of parameter (if multi count)
  int      active_widget_parsz; // size of parameter to copy
  int      selected;
  float    aspect;
  float    state[2100];
  size_t   mapped_size;
  float   *mapped;
  int      grabbed;
  int      lod;
  uint32_t copied_imgid;       // imgid copied for copy/paste
  float    connector[100][30][2];
  char    *module_names_buf;
  const char **module_names;

  double notification_time;     // time the message appeared
  char   notification_msg[256]; // message to display

  int fullscreen_view;          // darkroom mode without panels
  int history_view;             // darkroom mode with left panel shown (history view)

  int have_joystick;            // found and enabled a joystick (disable via gui/disable_joystick in config)
  int pentablet_enabled;        // 1 if the stylus is in proxmity of the pen tablet

  int set_nav_focus;            // gamepad navigation delay to communicate between lighttable and darkroom
  int busy;                     // still busy for how many frames before stopping redraw?
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

  VkResult         graph_res;
  dt_graph_t       graph_dev;

  dt_db_t          db;            // image list and current query
  dt_thumbnails_t  thumbnails;    // for light table mode
  dt_thumbnails_t  thumbnail_gen; // to generate thumbnails asynchronously
  dt_gui_view_t    view_mode;     // current view mode

  dt_snd_t         snd;           // connection to audio device
  dt_rc_t          rc;            // config file

  // favourite gui module/parameter list
  int fav_cnt;
  int fav_modid[20];
  int fav_parid[20];

  // list of recently used tags
  int  tag_cnt;
  char tag[10][30];
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
VkResult dt_gui_render();

// waits for semaphore and presents framebuffer
VkResult dt_gui_present();

// add a widget for the given module instance and parameter name
void dt_gui_add_widget(
    dt_token_t module, dt_token_t inst, dt_token_t param, dt_token_t type,
    float min, float max);

// read a gui configuration from ascii config file
int dt_gui_read_favs(const char *filename);

// read list of tags (i.e. the directories in ~/.config/vkdt/tags/)
void dt_gui_read_tags();

// close current db, load given folder instead
void dt_gui_switch_collection(const char *dir);

// display a notification message overlay in the gui for some seconds
void dt_gui_notification(const char *msg, ...);

// lets the current input module grab the mouse, i.e. hide it from the rest of the gui
void dt_gui_grab_mouse();

// ungrab the mouse, pass it on to imgui again
void dt_gui_ungrab_mouse();
