#pragma once
#include "pipe/graph.h"
#include "db/thumbnails.h"
#include "db/db.h"
#include "db/rc.h"
#include "snd/snd.h"
#include "widget_image_util.h"
#include "nk.h"

#include <vulkan/vulkan.h>

// max images in flight in vulkan pipeline/swap chain
#define DT_GUI_MAX_IMAGES 8
#define NK_UPDATE_ACTIVE do {if(vkdt.ctx.current && (vkdt.ctx.current->property.active || vkdt.ctx.current->edit.active)) vkdt.wstate.nk_active_next = 1;} while(0)

// view modes, lighttable, darkroom, ..
typedef enum dt_gui_view_t
{
  s_view_files      = 0,
  s_view_lighttable = 1,
  s_view_darkroom   = 2,
  s_view_nodes      = 3,
  s_view_cnt,
}
dt_gui_view_t;

// some global gui config options. this is a minimal
// set of static user input
typedef struct dt_gui_style_t
{
#define NK_COLOR_DT_ACCENT         NK_COLOR_COUNT
#define NK_COLOR_DT_ACCENT_HOVER  (NK_COLOR_COUNT+1)
#define NK_COLOR_DT_ACCENT_ACTIVE (NK_COLOR_COUNT+2)
#define NK_COLOR_DT_BACKGROUND    (NK_COLOR_COUNT+3)
  float panel_width_frac;   // width of the side panel as fraction of the total window width
  float border_frac;        // width of border between image and panel
  struct nk_color colour[NK_COLOR_COUNT+4];
}
dt_gui_style_t;

// this is also gui style related, but has to be recomputed every time the
// window is resized and may contain user input.
typedef struct dt_gui_state_t
{
  // center window configuration
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

typedef enum dt_gui_popup_t
{ // constants identifying currently opened popup
  s_popup_none = 0,
  s_popup_assign_tag,
  s_popup_create_preset,
  s_popup_apply_preset,
  s_popup_add_module,
  s_popup_edit_hotkeys,
} dt_gui_popup_t;

// a few local things to exchange data between core and ui
typedef struct dt_gui_wstate_t
{
  dt_image_widget_t img_widget;
  int      active_widget_modid; // module id
  int      active_widget_parid; // parameter id
  int      active_widget_parnm; // number of parameter (if multi count)
  int      active_widget_parsz; // size of parameter to copy
  int      selected;
  float    aspect;
  float    state[2100];
  size_t   mapped_size;
  float   *mapped;
  int      grabbed;             // module grabbed input (quake for instance)
  int      nk_active;           // nuklear wants input (typing into an edit field)
  int      nk_active_next;
  int      lod;
  uint32_t copied_imgid;        // imgid copied for copy/paste
  char    *module_names_buf;
  const char **module_names;

  threads_mutex_t notification_mutex;
  double notification_time;     // time the message appeared
  char   notification_msg[256]; // message to display

  int fullscreen_view;          // darkroom mode without panels
  int history_view;             // darkroom mode with left panel shown (history view)
  float dopesheet_view;         // darkroom mode dopesheet, stores adaptive size (0 means collapsed)

  int have_joystick;            // found and enabled a joystick (disable via gui/disable_joystick in config)
  GLFWgamepadstate gamepad_curr;// current state of gamepad buttons.
  GLFWgamepadstate gamepad_prev;
  int pentablet_enabled;        // 1 if the stylus is in proxmity of the pen tablet

  int busy;                     // still busy for how many frames before stopping redraw?

  float fontsize;               // pixel size of currently loaded (regular) font

  int show_gamepadhelp;         // show context sensitive gamepad help
  int show_perf_overlay;        // show a frame time graph as overlay

  int popup;                    // currently open popup, see dt_gui_popup_t
  int popup_appearing;          // set on the first frame a popup is shown
}
dt_gui_wstate_t;

typedef struct dt_gui_win_t
{ // stuff required to open a window
  GLFWwindow        *window;
  int                width;
  int                height;

  VkSurfaceKHR       surface;
  VkSwapchainKHR     swap_chain;
  VkSurfaceFormatKHR surf_format;
  VkPresentModeKHR   present_mode;
  uint32_t           num_swap_chain_images;
  VkImage            swap_chain_images[QVK_MAX_SWAPCHAIN_IMAGES];
  VkImageView        swap_chain_image_views[QVK_MAX_SWAPCHAIN_IMAGES];

  VkRenderPass       render_pass;
  VkPipelineCache    pipeline_cache;
  VkDescriptorPool   descriptor_pool;

  uint32_t           frame_index;
  VkFence            fence         [DT_GUI_MAX_IMAGES];
  VkCommandPool      command_pool  [DT_GUI_MAX_IMAGES];
  VkCommandBuffer    command_buffer[DT_GUI_MAX_IMAGES];
  VkFramebuffer      framebuffer   [DT_GUI_MAX_IMAGES];

  uint32_t           sem_index;
  VkSemaphore        sem_image_acquired [DT_GUI_MAX_IMAGES];
  VkSemaphore        sem_render_complete[DT_GUI_MAX_IMAGES];
  uint32_t           sem_fence[DT_GUI_MAX_IMAGES];
}
dt_gui_win_t;

typedef struct dt_graph_t dt_graph_t;
typedef struct dt_gui_t
{
  dt_gui_win_t     win;
  dt_gui_win_t     win1;

  struct nk_context ctx;          // nuklear gui context, main screen
  struct nk_context ctx1;         // nuklear gui context, secondary viewport

  dt_gui_style_t   style;
  dt_gui_state_t   state;
  dt_gui_wstate_t  wstate;

  VkResult         graph_res;     // result of last run
  dt_graph_t       graph_dev;     // processing graph

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
  // same in global file, possibly not all present all the time:
  int fav_file_cnt;
  dt_token_t fav_file_modid[20];
  dt_token_t fav_file_insid[20];
  dt_token_t fav_file_parid[20];

  // list of recently used tags
  int  tag_cnt;
  char tag[10][30];
}
dt_gui_t;

extern dt_gui_t vkdt;

int  dt_gui_init();
void dt_gui_cleanup();

// create or resize swapchain
VkResult dt_gui_recreate_swapchain(dt_gui_win_t *win);

// draws nuklear things, implemented in render.c
void dt_gui_render_frame_nk();

// records and submits command buffer
VkResult dt_gui_render();

// waits for semaphore and presents framebuffer
VkResult dt_gui_present();

// add a widget for the given module instance and parameter name
void dt_gui_add_widget(
    dt_token_t module, dt_token_t inst, dt_token_t param, dt_token_t type,
    float min, float max);

int  dt_gui_read_favs (const char *filename); // read a gui configuration from ascii config file
int  dt_gui_write_favs(const char *filename); // serialise the file with potential changes to home directory
void dt_gui_add_fav   (dt_token_t modid, dt_token_t insid, dt_token_t parid);
void dt_gui_move_fav  (dt_token_t modid, dt_token_t insid, dt_token_t parid, int up);
void dt_gui_remove_fav(dt_token_t modid, dt_token_t insid, dt_token_t parid);

// read list of tags (i.e. the directories in ~/.config/vkdt/tags/)
void dt_gui_read_tags();

// update recently used collections list, pushing the current collection on top
void dt_gui_update_recently_used_collections();

// close current db, load given folder instead
void dt_gui_switch_collection(const char *dir);

// display a notification message overlay in the gui for some seconds
// unlike most other functions, this one is thread-safe.
void dt_gui_notification(const char *msg, ...);

// lets the current input module grab the mouse, i.e. hide it from the rest of the gui
void dt_gui_grab_mouse();

// ungrab the mouse, pass it on to nuklear again
void dt_gui_ungrab_mouse();

// open secondary window for dual screen use
void dt_gui_win1_open();

// close secondary window
void dt_gui_win1_close();
