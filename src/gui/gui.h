#pragma once
#include "core/colour.h"
#include "pipe/graph.h"
#include "db/thumbnails.h"
#include "db/db.h"
#include "db/rc.h"
#include "snd/snd.h"
#include "widget_image_util.h"
#include "widget_radial_menu_dr-fwd.h"
#include "nk.h"

#include <vulkan/vulkan.h>
#include <math.h>
#include <GLFW/glfw3.h>

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
#define NK_COLOR_DT_ACCENT_TEXT         NK_COLOR_COUNT
#define NK_COLOR_DT_ACCENT_TEXT_HOVER  (NK_COLOR_COUNT+1)
#define NK_COLOR_DT_ACCENT_TEXT_ACTIVE (NK_COLOR_COUNT+2)
#define NK_COLOR_DT_ACCENT             (NK_COLOR_COUNT+3)
#define NK_COLOR_DT_ACCENT_HOVER       (NK_COLOR_COUNT+4)
#define NK_COLOR_DT_ACCENT_ACTIVE      (NK_COLOR_COUNT+5)
#define NK_COLOR_DT_BACKGROUND         (NK_COLOR_COUNT+6)
  float panel_width_frac;   // width of the side panel as fraction of the total window width
  float border_frac;        // width of border between image and panel
  float fontsize;           // font height in pixels
  struct nk_color colour[NK_COLOR_COUNT+7];
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
  struct nk_rect active_dspy_bound;
  int      selected;
  float    aspect;
  float    state[2100];
  size_t   mapped_size;
  float   *mapped;
  int      interact_begin;      // marks the begin of a gui interaction (that might make lod changes necessary)
  int      interact_end;        // end of interaction
  int      grabbed;             // module grabbed input (quake for instance)
  int      nk_active;           // nuklear wants input (typing into an edit field)
  int      nk_active_next;
  int      lod, lod_fine, lod_interact;
  uint32_t copied_imgid;        // imgid copied for copy/paste
  char    *module_names_buf;
  const char **module_names;

  dt_radial_menu_dr_t radial_menu_dr;

  threads_mutex_t notification_mutex;
  double notification_time;     // time the message appeared
  char   notification_msg[256]; // message to display

  int fullscreen_view;          // darkroom mode without panels
  int history_view;             // darkroom mode with left panel shown (history view)
  float dopesheet_view;         // darkroom mode dopesheet, stores adaptive size (0 means collapsed)

  int have_joystick;            // found and enabled a joystick (disable via gui/disable_joystick in config)
  int joystick_id;              // like GLFW_JOYSTICK_1
  int pentablet_enabled;        // 1 if the stylus is in proxmity of the pen tablet

  int busy;                     // still busy for how many frames before stopping redraw?

  float fontsize;               // pixel size of currently loaded (regular) font

  int show_gamepadhelp;         // show context sensitive gamepad help
  int show_perf_overlay;        // show a frame time graph as overlay

  int popup;                    // currently open popup, see dt_gui_popup_t
  int popup_appearing;          // set on the first frame a popup is shown

  int tab_state;                // state required to tab between tabbable widgets

  int lighttable_images_per_row;// how many images per row in lighttable mode
}
dt_gui_wstate_t;

typedef struct dt_gui_win_t
{ // stuff required to open a window
  GLFWwindow        *window;
  int                width;
  int                height;
  int                width_restore;
  int                height_restore;
  int                xpos_restore;
  int                ypos_restore;
  float              content_scale[2]; // cached content scale factor

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

  uint32_t           fullscreen;
}
dt_gui_win_t;

typedef struct dt_graph_t dt_graph_t;
typedef struct dt_gui_t
{
  dt_gui_win_t     win;
  dt_gui_win_t     win1;

  struct nk_context ctx;          // nuklear gui context, main screen
  struct nk_context ctx1;         // nuklear gui context, secondary viewport

  struct nk_buffer         global_buf; // buffer backend for global_cmd
  struct nk_command_buffer global_cmd; // extra ui elements drawn on top of everything else in window 0

  dt_gui_style_t   style;
  dt_gui_state_t   state;
  dt_gui_wstate_t  wstate;

  VkResult         graph_res[2];  // result of last run/double pumped
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
  char fav_preset_name[16][64];
  char fav_preset_desc[16][64];

  // list of recently used tags
  int  tag_cnt;
  char tag[10][30];

  int  session_type;     // 0: running on xorg, 1: wayland, maybe extend for macos and windows too, -1 unknown
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

// init style variables / padding based on fontsize / colours from text file
void dt_gui_style_to_state();

// this is only a shorthand for the direct access to vkdt.win*.content_scale
// selecting the right one based on the glfw window. this is otherwise
// exactly equivalent to direct variable access.
static inline void
dt_gui_content_scale(GLFWwindow *w, float *x, float *y)
{ // these factors are only inited to content scale if the compositor cares 
  // (i.e. wayland and macintosh, not x11 or windows)
  if(w == vkdt.win.window)
  {
    x[0] = vkdt.win.content_scale[0];
    y[0] = vkdt.win.content_scale[1];
  }
  else
  {
    x[0] = vkdt.win1.content_scale[0];
    y[0] = vkdt.win1.content_scale[1];
  }
}

static inline struct nk_colorf
rgb2hsv(float r, float g, float b)
{
  float ucs[3], rgb[] = {r, g, b, 0.0f};
  // rec2020_to_oklab(rgb, ucs);
  // return (struct nk_colorf){modff(1.0f + atan2f(ucs[2], ucs[1])/(2.0f*M_PI), rgb+3), sqrtf(ucs[1]*ucs[1]+ucs[2]*ucs[2]), ucs[0], 1.0};
  // rec2020_to_dtucs(rgb, ucs);
  // return (struct nk_colorf){ modff(1.0f + ucs[0]/(2.0f*M_PI), rgb+3), ucs[1], ucs[2], 1.0};
  rec2020_to_hsluv(rgb, ucs);
  return (struct nk_colorf){ modff(ucs[0], rgb+3), ucs[1], ucs[2], 1.0};
}

static inline struct nk_colorf
rgb2hsv_prior(float r, float g, float b, struct nk_colorf hsv_prior)
{
  if(r <= 0 && g <= 0 && b <= 0) return (struct nk_colorf){hsv_prior.r, hsv_prior.g, 0.0, 1.0};
  struct nk_colorf res = rgb2hsv(r, g, b);
  if(res.g <= 1e-5) res.r = hsv_prior.r;
  return res;
}

static inline struct nk_colorf
hsv2rgb(float h, float s, float v)
{
  if(v <= 0.0f) return (struct nk_colorf){0,0,0,1.0f};
  // float ucs[3] = {v, s * cosf(2.0f*M_PI*h), s * sinf(2.0f*M_PI*h)}, rgb[3];
  // oklab_to_rec2020(ucs, rgb);
  // float ucs[3] = {h*2.0f*M_PI, s, v}, rgb[3];
  // dtucs_to_rec2020(ucs, rgb);
  float ucs[3] = {h, s, v}, rgb[3];
  hsluv_to_rec2020(ucs, rgb);
  return (struct nk_colorf){rgb[0], rgb[1], rgb[2], 1.0f};
}

// define groups of widgets that can be switched by pressing "tab"
// render.c will initialise the tab state in case a key has been pressed.
#define nk_tab_property(TYPE, CTX, NAME, m, V, M, I1, I2)\
  do {\
    int adv = 0, act = (CTX)->current && !((CTX)->current->flags & NK_WINDOW_NOT_INTERACTIVE);\
    if(act && vkdt.wstate.tab_state == 2) { nk_property_focus(CTX); vkdt.wstate.tab_state = 0; }\
    nk_property_ ## TYPE(CTX, NAME, m, V, M, I1, I2);\
    if(act) adv = nk_property_## TYPE ##_unfocus(CTX, NAME, m, V, M, I1, vkdt.wstate.tab_state);\
    if(act && adv) { vkdt.wstate.tab_state = 2; adv = 0; }\
  } while(0)

#define nk_tab_edit_string_zero_terminated(CTX, FLAGS, STR, LEN, FILTER) ({\
    nk_flags ret = 0;\
    int act = (CTX)->current && !((CTX)->current->flags & NK_WINDOW_NOT_INTERACTIVE);\
    if(act && vkdt.wstate.tab_state == 2) { nk_edit_focus(CTX, 0); vkdt.wstate.tab_state = 0; }\
    ret = nk_edit_string_zero_terminated(CTX, FLAGS, STR, LEN, FILTER);\
    int has_focus = (CTX) && (CTX)->current->edit.seq-1 == (CTX)->current->edit.name && (CTX)->current->edit.active;\
    if(act && has_focus && vkdt.wstate.tab_state == 1) {\
      nk_edit_unfocus(CTX);\
      vkdt.wstate.tab_state = 2; }\
    ret; })
