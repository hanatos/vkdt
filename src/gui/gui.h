#pragma once
#include "pipe/graph.h"
#include "db/thumbnails.h"
#include "db/db.h"
#include "db/rc.h"
#include "snd/snd.h"
#include "widget_image_util.h"
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
#define NK_COLOR_DT_ACCENT         NK_COLOR_COUNT
#define NK_COLOR_DT_ACCENT_HOVER  (NK_COLOR_COUNT+1)
#define NK_COLOR_DT_ACCENT_ACTIVE (NK_COLOR_COUNT+2)
#define NK_COLOR_DT_BACKGROUND    (NK_COLOR_COUNT+3)
  float panel_width_frac;   // width of the side panel as fraction of the total window width
  float border_frac;        // width of border between image and panel
  float fontsize;           // font height in pixels
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
  char fav_preset_name[4][64];
  char fav_preset_desc[4][64];

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

// some gui colour things. we want perceptually uniform colour picking.
// since hsv is a severely broken concept, we mean oklab LCh (or hCL really)
static inline void rec2020_to_oklab(const float rgb[], float oklab[])
{ // these matrices are the same as in glsl, i.e. transposed:
  const float M[] = { // M1 * rgb_to_xyz
      0.61668844, 0.2651402 , 0.10015065,
      0.36015907, 0.63585648, 0.20400432,
      0.02304329, 0.09903023, 0.69632468};
  float lms[3] = {0.0f};
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      lms[k] += M[3*i+k] * rgb[i];
  for(int k=0;k<3;k++)
    lms[k] = cbrtf(lms[k]);
  const float M2[] = {
      0.21045426,  1.9779985 ,  0.02590404,
      0.79361779, -2.42859221,  0.78277177,
     -0.00407205,  0.45059371, -0.80867577};
  oklab[0] = oklab[1] = oklab[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      oklab[k] += M2[3*i+k] * lms[i];
}

static inline void oklab_to_rec2020(const float oklab[], float rgb[])
{
  const float M2inv[] = {
      1.        ,  1.00000001,  1.00000005,
      0.39633779, -0.10556134, -0.08948418,
      0.21580376, -0.06385417, -1.29148554};
  float lms[3] = {0.0f};
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      lms[k] += M2inv[3*i+k] * oklab[i];
  for(int k=0;k<3;k++)
    lms[k] *= lms[k]*lms[k];
  float M[] = { // = xyz_to_rec2020 * M1inv
        2.14014041, -0.88483245, -0.04857906,
       -1.24635595,  2.16317272, -0.45449091,
        0.10643173, -0.27836159,  1.50235629};
  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      rgb[k] += M[3*i+k] * lms[i];
}
#if 1
/* The following is darktable Uniform Color Space 2022, MIT licence
 * https://eng.aurelienpierre.com/2022/02/color-saturation-control-for-the-21th-century/ */
static inline float
Y_to_dt_UCS_L_star(const float Y)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  const float Y_hat = powf(Y, 0.631651345306265f);
  return 2.098883786377f * Y_hat / (Y_hat + 1.12426773749357f);
}
static inline float
dt_UCS_L_star_to_Y(const float L_star)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  return powf((1.12426773749357f * L_star / (2.098883786377f - L_star)), 1.5831518565279648f);
}
static inline struct nk_colorf
xy_to_dt_UCS_UV(struct nk_colorf xy)
{
  const float M1[] = { // column vectors:
      -0.783941002840055,  0.745273540913283, 0.318707282433486,
       0.277512987809202, -0.205375866083878, 2.16743692732158,
       0.153836578598858, -0.165478376301988, 0.291320554395942};
  float uvd[3] = {0.0f};
  for(int k=0;k<3;k++)
  {
    uvd[k] += M1[3*0+k] * xy.r;
    uvd[k] += M1[3*1+k] * xy.g;
    uvd[k] += M1[3*2+k] * 1.0f;
  }
  uvd[0] /= uvd[2];
  uvd[1] /= uvd[2];
  const float factors[]     = {1.39656225667, 1.4513954287 };
  const float half_values[] = {1.49217352929, 1.52488637914};
  float UV_star[] = {
    factors[0] * uvd[0] / (fabsf(uvd[0]) + half_values[0]),
    factors[1] * uvd[1] / (fabsf(uvd[1]) + half_values[1])};
  const float M2[] = {-1.124983854323892, 1.86323315098672, -0.980483721769325, 1.971853092390862};
  return (struct nk_colorf){
    M2[0] * UV_star[0] + M2[2] * UV_star[1],
    M2[1] * UV_star[0] + M2[3] * UV_star[1], 0, 0};
}
//    * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
//    * L_white the lightness of white as dt UCS L* lightness
//  range : xy in [0; 1], Y normalized for perfect diffuse white = 1
static inline struct nk_colorf
xyY_to_dt_UCS_JCH(const struct nk_colorf xyY, const float L_white)
{
  struct nk_colorf UV_star_prime = xy_to_dt_UCS_UV(xyY);
  const float L_star = Y_to_dt_UCS_L_star(xyY.b);
  const float M2 = UV_star_prime.r*UV_star_prime.r + UV_star_prime.g*UV_star_prime.g; // square of colorfulness M
  return (struct nk_colorf){ // should be JCH[0] = powf(L_star / L_white), cz) but we treat only the case where cz = 1
      L_star / L_white,
      15.932993652962535f * powf(L_star, 0.6523997524738018f) * powf(M2, 0.6007557017508491f) / L_white,
      atan2f(UV_star_prime.g, UV_star_prime.r)};
}
static inline struct nk_colorf
dt_UCS_JCH_to_xyY(const struct nk_colorf JCH, const float L_white)
{
  const float L_star = JCH.r * L_white; // should be L_star = powf(JCH[0], 1.f / cz) * L_white but we treat only cz = 1
  float M = powf(JCH.g * L_white / (15.932993652962535f * powf(L_star, 0.6523997524738018f)), 0.8322850678616855f);
  // touchy bugger:
  M = CLAMP(M, 0.0, 0.05);
  float UV_starp[] = { M * cosf(JCH.b), M * sin(JCH.b) }; // uv*'
  const float M1[] = {-5.037522385190711, 4.760029407436461, -2.504856328185843, 2.874012963239247}; // col major
  float UV_star[] = {
    M1[0] * UV_starp[0] + M1[2] * UV_starp[1],
    M1[1] * UV_starp[0] + M1[3] * UV_starp[1]};
  const float factors[]     = {1.39656225667, 1.4513954287};
  const float half_values[] = {1.49217352929, 1.52488637914};
  float UV[] = {
    -half_values[0] * UV_star[0] / (fabsf(UV_star[0]) - factors[0]),
    -half_values[1] * UV_star[1] / (fabsf(UV_star[1]) - factors[1])};
  const float M2[] = { // given as column vectors
       0.167171472114775,   -0.150959086409163,    0.940254742367256,
       0.141299802443708,   -0.155185060382272,    1.000000000000000,
      -0.00801531300850582, -0.00843312433578007, -0.0256325967652889};
  float xyD[3] = {0.0f};
  for(int k=0;k<3;k++)
  {
    xyD[k] += M2[3*0+k] * UV[0];
    xyD[k] += M2[3*1+k] * UV[1];
    xyD[k] += M2[3*2+k] * 1.0f;
  }
  return (struct nk_colorf){xyD[0]/xyD[2], xyD[1]/xyD[2], dt_UCS_L_star_to_Y(L_star)};
}
static inline struct nk_colorf
dt_UCS_JCH_to_HSB(const struct nk_colorf JCH)
{
  struct nk_colorf HSB;
  HSB.b = JCH.r * (powf(JCH.g, 1.33654221029386f) + 1.f);
  return (struct nk_colorf){JCH.b, (HSB.b > 0.f) ? JCH.g / HSB.b : 0.f, HSB.b};
}
static inline struct nk_colorf
dt_UCS_HSB_to_JCH(const struct nk_colorf HSB)
{
  return (struct nk_colorf){HSB.b / (powf(HSB.g*HSB.b, 1.33654221029386f) + 1.f), HSB.g * HSB.b, HSB.r};
}
static inline struct nk_colorf
dt_UCS_JCH_to_HCB(const struct nk_colorf JCH)
{
  return (struct nk_colorf){JCH.b, JCH.g, JCH.r * (powf(JCH.g, 1.33654221029386f) + 1.f)};
}
static inline struct nk_colorf
dt_UCS_HCB_to_JCH(const struct nk_colorf HCB)
{
  return (struct nk_colorf){HCB.b / (powf(HCB.g, 1.33654221029386f) + 1.f), HCB.g, HCB.r};
}
static inline void
rec2020_to_dtucs(const float rgb[], float dtucs[])
{ // these matrices are the same as in glsl, i.e. transposed:
  const float rec2020_to_xyz[] = {
    6.36958048e-01, 2.62700212e-01, 4.20575872e-11,
    1.44616904e-01, 6.77998072e-01, 2.80726931e-02,
    1.68880975e-01, 5.93017165e-02, 1.06098506e+00};
  float xyz[3] = {0.0f};
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      xyz[k] += rec2020_to_xyz[3*i+k] * rgb[i];
  float s = 1.0f/(xyz[0]+xyz[1]+xyz[2]);
  struct nk_colorf xyY = { s*xyz[0], s*xyz[1], xyz[1] };
  struct nk_colorf jch = xyY_to_dt_UCS_JCH(xyY, 1.0f);
  struct nk_colorf hsb = dt_UCS_JCH_to_HSB(jch);
  dtucs[0] = hsb.r; dtucs[1] = hsb.g; dtucs[2] = hsb.b;
}
static inline void
dtucs_to_rec2020(const float dtucs[], float rgb[])
{
  struct nk_colorf hsb = {dtucs[0],dtucs[1],dtucs[2]};
  struct nk_colorf jch = dt_UCS_HSB_to_JCH(hsb);
  struct nk_colorf xyY = dt_UCS_JCH_to_xyY(jch, 1.0f);
  float s = xyY.b / xyY.g;
  float xyz[] = {s*xyY.r, s*xyY.g, s*(1.0-xyY.r-xyY.g)};
  const float xyz_to_rec2020[] = {
    1.71665119, -0.66668435,  0.01763986,
   -0.35567078,  1.61648124, -0.04277061,
   -0.25336628,  0.01576855,  0.94210312};
  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      rgb[k] += xyz_to_rec2020[3*i+k] * xyz[i];
}
#endif

static inline struct nk_colorf
rgb2hsv(float r, float g, float b)
{
  float ucs[3], rgb[] = {r, g, b, 0.0f};
  // rec2020_to_oklab(rgb, ucs);
  // return (struct nk_colorf){modff(1.0f + atan2f(ucs[2], ucs[1])/(2.0f*M_PI), rgb+3), sqrtf(ucs[1]*ucs[1]+ucs[2]*ucs[2]), ucs[0], 1.0};
  rec2020_to_dtucs(rgb, ucs);
  return (struct nk_colorf){ modff(1.0f + ucs[0]/(2.0f*M_PI), rgb+3), ucs[1], ucs[2], 1.0};
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
  float ucs[3] = {h*2.0f*M_PI, s, v}, rgb[3];
  dtucs_to_rec2020(ucs, rgb);
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
