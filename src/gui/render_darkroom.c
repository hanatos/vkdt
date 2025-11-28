#include "core/log.h"
#include "core/fs.h"
#include "core/core.h"
#include "qvk/qvk.h"
#include "gui/api.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/render.h"
#include "gui/api_gui.h"
#include "gui/darkroom.h"
#include "gui/view.h"
#include "pipe/draw.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-defaults.h"
#include "pipe/graph-history.h"
#include "pipe/modules/api.h"
#include "pipe/graph-history.h"
#include "pipe/graph-defaults.h"
#include "gui/render_view.h"
#include "gui/hotkey.h"
#include "gui/keyaccel.h"
#include "gui/api_gui.h"
#include "gui/widget_dopesheet.h"
#include "gui/widget_draw.h"
#include "gui/widget_thumbnail.h"
#include "gui/widget_image.h"
#include "gui/widget_resize_panel.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <libgen.h>
#include <limits.h>


static dt_keyaccel_t keyaccel;

enum hotkey_names_t
{ // for sane access in code
  s_hotkey_create_preset   = 0,
  s_hotkey_apply_preset    = 1,
  s_hotkey_show_history    = 2,
  s_hotkey_redo            = 3,
  s_hotkey_undo            = 4,
  s_hotkey_assign_tag      = 5,
  s_hotkey_insert_keyframe = 6,
  s_hotkey_nodes_enter     = 7,
  s_hotkey_next_image      = 8,
  s_hotkey_prev_image      = 9,
  s_hotkey_fullscreen      = 10,
  s_hotkey_dopesheet       = 11,
  s_hotkey_keyframe_dup    = 12,
  s_hotkey_rate_0          = 13,
  s_hotkey_rate_1          = 14,
  s_hotkey_rate_2          = 15,
  s_hotkey_rate_3          = 16,
  s_hotkey_rate_4          = 17,
  s_hotkey_rate_5          = 18,
  s_hotkey_label_1         = 19,
  s_hotkey_label_2         = 20,
  s_hotkey_label_3         = 21,
  s_hotkey_label_4         = 22,
  s_hotkey_label_5         = 23,
  s_hotkey_upvote          = 24,
  s_hotkey_downvote        = 25,
  s_hotkey_reload_shaders  = 26,
  s_hotkey_copy_hist       = 27,
  s_hotkey_paste_hist      = 28,
  s_hotkey_count           = 29,
};

static const int hk_darkroom_size = 128;
static int hk_darkroom_cnt = s_hotkey_count;
static hk_t hk_darkroom[128] = {
  {"create preset",   "create new preset from image",               {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_O}},
  {"apply preset",    "choose preset to apply",                     {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_P}},
  {"show history",    "toggle visibility of left panel",            {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_H}},
  {"redo",            "go up in history stack one item",            {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_Z}},
  {"undo",            "go down in history stack one item",          {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_Z}},
  {"assign tag",      "assign a tag to the current image",          {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_T}},
  {"insert keyframe", "insert a keyframe for the active widget",    {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_K}},
  {"node editor",     "show node editor for the current image",     {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N}},
  {"next image",      "switch to next image in folder",             {GLFW_KEY_SPACE}},
  {"prev image",      "switch to previous image in folder",         {GLFW_KEY_BACKSPACE}},
  {"fullscreen",      "show/hide side panels for fullscreen",       {GLFW_KEY_TAB}},
  {"dopesheet",       "show/hide keyframe overview",                {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_D}},
  {"dup keyframe",    "duplicate hovered keyframe in dopesheet",    {GLFW_KEY_LEFT_SHIFT,   GLFW_KEY_D}},
  {"rate 0",          "assign zero stars",                          {GLFW_KEY_0}},
  {"rate 1",          "assign one star",                            {GLFW_KEY_1}},
  {"rate 2",          "assign two stars",                           {GLFW_KEY_2}},
  {"rate 3",          "assign three stars",                         {GLFW_KEY_3}},
  {"rate 4",          "assign four stars",                          {GLFW_KEY_4}},
  {"rate 5",          "assign five stars",                          {GLFW_KEY_5}},
  {"label red",       "toggle red label",                           {GLFW_KEY_F1}},
  {"label green",     "toggle green label",                         {GLFW_KEY_F2}},
  {"label blue",      "toggle blue label",                          {GLFW_KEY_F3}},
  {"label yellow",    "toggle yellow label",                        {GLFW_KEY_F4}},
  {"label purple",    "toggle purple label",                        {GLFW_KEY_F5}},
  {"upvote",          "increase star rating and go to next image",  {GLFW_KEY_F12}},
  {"downvote",        "decrease star rating and go to next image",  {GLFW_KEY_F9}},
  {"reload shaders",  "debug: reload shader code while running",    {}},
  {"copy history",    "copy history from curren image",             {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_C}},
  {"paste history",   "paste history to selected images",           {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_V}},
};

// used to communictate between the gui helper functions
static struct gui_state_data_t
{
  int hotkey;
  dt_token_t active_module;   // remember last active module with instance name
  dt_token_t active_instance; // so we can expand it again when the next image loads
  int pgupdn;                 // collect pg up and down key presses for rotary encoder knobs
} gui;

#define ROTARY_ENCODER
// goes here because the keyframe code depends on the above defines/hotkeys
// could probably pass a function pointer instead.
#include "gui/render_darkroom.h"
#include "gui/widget_radial_menu_dr.h"

void
darkroom_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type     = 4,
      .key      = key,
      .scancode = scancode,
      .action   = action,
      .mods     = mods,
      .grabbed  = vkdt.wstate.grabbed,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_CAPS_LOCK))
    {
      dt_gui_ungrab_mouse();
      p.type = -1; // disconnect event
      dt_gui_unset_fullscreen_view();
      // if this came from a camera button, we want to record the new camera in history:
      if(vkdt.wstate.active_widget_modid >= 0)
        dt_graph_history_append(&vkdt.graph_dev, vkdt.wstate.active_widget_modid, vkdt.wstate.active_widget_parid, 2.0);
      dt_gui_dr_anim_stop();
    }
    if(vkdt.wstate.active_widget_modid >= 0)
    {
      if(mod->so->input) mod->so->input(mod, &p);
      if(p.type == -1) vkdt.wstate.active_widget_modid = -1;
    }
    return; // grabbed, don't execute hotkeys
  }
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
    return hk_keyboard(hk_darkroom, window, key, scancode, action, mods);

  if(dt_radial_menu_dr_active(&vkdt.wstate.radial_menu_dr))
  { // radial menus block input, so check this before checking block
    if(action == GLFW_PRESS && (
          key == GLFW_KEY_ESCAPE ||
          key == GLFW_KEY_CAPS_LOCK ||
          key == GLFW_KEY_ENTER))
      dt_radial_menu_dr_close(&vkdt.wstate.radial_menu_dr);
    return;
  }

  if(dt_gui_input_blocked()) return;

  if(vkdt.wstate.active_widget_modid >= 0)
  { // active widget grabs controls
    if(action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_CAPS_LOCK))
    { // abort all widget interaction
      widget_end();
    }
    return;
  }

  gui.hotkey = action == GLFW_PRESS ? hk_get_hotkey(hk_darkroom, hk_darkroom_cnt, key) : -1;
  if(action != GLFW_PRESS) return; // only handle key down events
  switch(gui.hotkey)
  { // these are "destructive" hotkeys, they change the image and invalidate the dset.
    // this has to happen this frame *before* the dset is sent to imgui for display.
    case s_hotkey_next_image:
      dt_gui_dr_next_or_play();
      break;
    case s_hotkey_prev_image:
      dt_gui_dr_prev_or_rewind();
      break;
    case s_hotkey_upvote:
      dt_gui_dr_advance_upvote();
      break;
    case s_hotkey_downvote:
      dt_gui_dr_advance_downvote();
      break;
    case s_hotkey_create_preset:
      dt_gui_dr_preset_create();
      break;
    case s_hotkey_apply_preset:
      dt_gui_preset_apply();
      break;
    case s_hotkey_show_history:
      dt_gui_dr_toggle_history();
      break;
    case s_hotkey_redo:
      dt_gui_dr_show_history();
      dt_gui_dr_history_redo();
      break;
    case s_hotkey_undo:
      dt_gui_dr_show_history();
      dt_gui_dr_history_undo();
      break;
    case s_hotkey_assign_tag:
      dt_gui_dr_assign_tag();
      break;
    case s_hotkey_nodes_enter:
      dt_view_switch(s_view_nodes);
      break;
    case s_hotkey_fullscreen:
      dt_gui_toggle_fullscreen_view();
      break;
    case s_hotkey_dopesheet:
      dt_gui_dr_toggle_dopesheet();
      break;
    case s_hotkey_rate_0: dt_gui_rate_0(); break;
    case s_hotkey_rate_1: dt_gui_rate_1(); break;
    case s_hotkey_rate_2: dt_gui_rate_2(); break;
    case s_hotkey_rate_3: dt_gui_rate_3(); break;
    case s_hotkey_rate_4: dt_gui_rate_4(); break;
    case s_hotkey_rate_5: dt_gui_rate_5(); break;
    case s_hotkey_label_1: dt_gui_label_1(); break;
    case s_hotkey_label_2: dt_gui_label_2(); break;
    case s_hotkey_label_3: dt_gui_label_3(); break;
    case s_hotkey_label_4: dt_gui_label_4(); break;
    case s_hotkey_label_5: dt_gui_label_5(); break;
    case s_hotkey_reload_shaders: dt_gui_dr_reload_shaders(); break;
    case s_hotkey_copy_hist:
      dt_gui_copy_history();
      break;
    case s_hotkey_paste_hist:
      dt_gui_paste_history();
      break;
    default:
     if(gui.hotkey >= s_hotkey_count && gui.hotkey < hk_darkroom_cnt) dt_keyaccel_exec(hk_darkroom[gui.hotkey].name);
     break;
  }

  if(!dt_gui_input_blocked())
  {
    if(key == GLFW_KEY_ESCAPE)
      dt_view_switch(s_view_lighttable);
    if(key == GLFW_KEY_PAGE_UP)   gui.pgupdn++;
    if(key == GLFW_KEY_PAGE_DOWN) gui.pgupdn--;
  }
}

void render_darkroom_favourite()
{ // streamlined "favourite" ui
  dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  int32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < (int32_t)(sizeof(modid)/sizeof(modid[0])));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  int is_pst = 0;
  const float pwd = vkdt.state.panel_wd - vkdt.ctx.style.window.scrollbar_size.x - 2*vkdt.ctx.style.window.padding.x;
  const float w4[] = {
    0.7f/3.0f*pwd-vkdt.ctx.style.window.spacing.x,
    0.7f/3.0f*pwd-vkdt.ctx.style.window.spacing.x,
    0.7f/3.0f*pwd-vkdt.ctx.style.window.spacing.x,
    0.3f*pwd};
  for(int i=0;i<vkdt.fav_cnt;i++)
  {
    if(vkdt.fav_modid[i] == -1)
    {
      const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
      int pst = vkdt.fav_parid[i];
      if(pst >= sizeof(vkdt.fav_preset_name) / sizeof(vkdt.fav_preset_name[0])) continue;
      char *preset = vkdt.fav_preset_name[pst];
      if(!is_pst++) nk_layout_row(&vkdt.ctx, NK_STATIC, row_height, 4, w4);
      dt_tooltip("apply preset %s", vkdt.fav_preset_name[pst]);
      if(nk_button_label(&vkdt.ctx, vkdt.fav_preset_desc[pst]))
      {
        char pst[PATH_MAX];
        snprintf(pst, sizeof(pst), "%s.pst", preset);
        uint32_t err_lno = render_darkroom_apply_preset(pst);
        if(err_lno)
          dt_gui_notification("failed to read preset %s line %u", preset, err_lno);
      }
    }
    else for(int32_t m=0;m<cnt;m++)
    { // arg. can we please do that without n^2 every redraw?
      is_pst = 0;
      if(modid[m] == vkdt.fav_modid[i])
      {
        render_darkroom_widget(vkdt.fav_modid[i], vkdt.fav_parid[i], 1);
        break;
      }
    }
  }
}

void render_darkroom_full(char *filter_name, char *filter_inst)
{
  const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
  if(vkdt.graph_dev.active_module == -1)
  {
    nk_layout_row_dynamic(&vkdt.ctx, row_height, 2);
    dt_tooltip("filter by module name");
    nk_tab_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, filter_name, sizeof(filter_name), nk_filter_default);
    dt_tooltip("filter by module instance");
    nk_tab_edit_string_zero_terminated(&vkdt.ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, filter_inst, sizeof(filter_inst), nk_filter_default);
  }
  dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(int m=cnt-1;m>=0;m--)
  {
    if(filter_name[0])
    {
      char name[10] = {0};
      memcpy(name, dt_token_str(vkdt.graph_dev.module[modid[m]].name), 8);
      if(!strstr(name, filter_name)) continue;
    }
    if(filter_inst[0])
    {
      char inst[10] = {0};
      memcpy(inst, dt_token_str(vkdt.graph_dev.module[modid[m]].inst), 8);
      if(!strstr(inst, filter_inst)) continue;
    }
    render_darkroom_widgets(&vkdt.graph_dev, modid[m]);
  }
}

void render_darkroom()
{
  int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
  int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
  struct nk_rect bounds = {win_x, win_y, win_w, win_h};
  const int display_frame = vkdt.graph_dev.double_buffer % 2;
  if(!dt_gui_input_blocked() && nk_input_is_mouse_click_in_rect(&vkdt.ctx.input, NK_BUTTON_DOUBLE, bounds))
  {
    dt_view_switch(s_view_lighttable);
    gui.pgupdn = 0;
    return;
  }

  dt_radial_menu_dr(&vkdt.wstate.radial_menu_dr, bounds);

  static int resize_panel = 0;
  resize_panel = dt_resize_panel(resize_panel);

  const int disabled = vkdt.wstate.popup || dt_radial_menu_dr_active(&vkdt.wstate.radial_menu_dr);
  nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
  if(nk_begin(&vkdt.ctx, "darkroom center", bounds, NK_WINDOW_NO_SCROLLBAR | (disabled ? NK_WINDOW_NO_INPUT : 0)))
  { // draw center view image:
    dt_node_t *out_main = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(out_main)
    {
      if(vkdt.graph_res[display_frame] == VK_SUCCESS)
      {
        int events = !vkdt.wstate.grabbed && !disabled;
        // center view has on-canvas widgets (but only if there *is* an image):
        nk_layout_row_dynamic(&vkdt.ctx, win_h, 1);
        dt_image(&vkdt.ctx, &vkdt.wstate.img_widget, out_main, events, out_main != 0);
      }
    }
    float wd = 0.8*win_y;
    const uint32_t ci = dt_db_current_imgid(&vkdt.db);
    if(ci != -1u)
    { // this should *always* be the case
      const uint16_t labels = vkdt.db.image[ci].labels;
      const uint16_t rating = vkdt.db.image[ci].rating;
      dt_draw_rating(win_x+wd,   win_y+wd, wd, rating);
      dt_draw_labels(win_x+6*wd, win_y+wd, wd, labels);
    }

    // draw context sensitive help overlay
    if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();
    if(vkdt.wstate.show_perf_overlay) render_perf_overlay();
  } // end center view
  NK_UPDATE_ACTIVE;
  nk_end(&vkdt.ctx);
  nk_style_pop_style_item(&vkdt.ctx);

  static char filter_name[10] = {0};
  static char filter_inst[10] = {0};
  if(vkdt.wstate.dopesheet_view > 0.0f)
  { // draw dopesheet
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y + vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
    int win_w = vkdt.state.center_wd, win_h = vkdt.wstate.dopesheet_view;
    struct nk_rect bounds = { .x = win_x, .y = win_y, .w = win_w, .h = win_h };
    const int disabled = vkdt.wstate.popup;
    nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
    if(nk_begin(&vkdt.ctx, "dopesheet", bounds, (disabled ? NK_WINDOW_NO_INPUT : 0)))
      dt_dopesheet(filter_name, filter_inst, gui.hotkey == s_hotkey_keyframe_dup);

    NK_UPDATE_ACTIVE;
    nk_end(&vkdt.ctx);
    nk_style_pop_style_item(&vkdt.ctx);
  }

  if(!vkdt.wstate.fullscreen_view && vkdt.wstate.history_view)
  { // left panel: history view
    struct nk_rect bounds = { .x = 0, .y = 0, .w = vkdt.state.panel_wd, .h = vkdt.state.panel_ht };
    const int disabled = vkdt.wstate.popup;
    if(nk_begin(&vkdt.ctx, "history panel", bounds, disabled ? NK_WINDOW_NO_INPUT : 0))
    {
      int action = 0;
      const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
      nk_layout_row_dynamic(&vkdt.ctx, row_height, 3);
      dt_tooltip("rewrite compactified history");
      if(nk_button_label(&vkdt.ctx, "compress")) action = 1; // compress
      dt_tooltip("roll back to the state when entered darkroom mode");
      if(nk_button_label(&vkdt.ctx, "roll back")) action = 2; // load previously stored cfg from disk
      dt_tooltip("reset everything to factory defaults");
      if(nk_button_label(&vkdt.ctx, "reset")) action = 3; // load factory defaults
      if(action)
      {
        uint32_t imgid = dt_db_current_imgid(&vkdt.db);
        char graph_cfg[PATH_MAX+100];
        char realimg[PATH_MAX];
        dt_token_t input_module = dt_token("i-raw");

        if(action >= 2) dt_db_image_path(&vkdt.db, imgid, graph_cfg, sizeof(graph_cfg));
        if(action == 2)
        {
          struct stat statbuf;
          if(stat(graph_cfg, &statbuf)) action = 3;
        }
        if(action == 3)
        {
          fs_realpath(graph_cfg, realimg);
          int len = strlen(realimg);
          assert(len > 4);
          realimg[len-4] = 0; // cut away ".cfg"
          input_module = dt_graph_default_input_module(realimg);
          snprintf(graph_cfg, sizeof(graph_cfg), "default-darkroom.%" PRItkn, dt_token_str(input_module));
        }

        if(action >= 2)
        { // read previous cfg or factory default cfg, first init modules to their default state:
          for(uint32_t m=0;m<vkdt.graph_dev.num_modules;m++) dt_module_reset_params(vkdt.graph_dev.module+m);
          dt_graph_read_config_ascii(&vkdt.graph_dev, graph_cfg);
        }

        if(action == 3)
        { // default needs to update input filename and search path
          dt_graph_set_searchpath(&vkdt.graph_dev, realimg);
          char *basen = fs_basename(realimg); // cut away path so we can relocate more easily
          int modid = dt_module_get(&vkdt.graph_dev, input_module, dt_token("main"));
          if(modid >= 0)
            dt_module_set_param_string(vkdt.graph_dev.module + modid, dt_token("filename"),
                basen);
        }

        dt_graph_history_reset(&vkdt.graph_dev);
        vkdt.graph_dev.runflags = s_graph_run_all;
      }

      nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
      nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
      for(int i=vkdt.graph_dev.history_item_end-1;i>=0;i--)
      {
        int pop = 0;
        if(i >= (int)vkdt.graph_dev.history_item_cur)
        { // inactive
          pop = 2;
          nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.normal, nk_style_item_color(nk_rgb(0,0,0)));
          nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.hover,  nk_style_item_color(vkdt.style.colour[NK_COLOR_BORDER]));
        }
        else if(i+1 == (int)vkdt.graph_dev.history_item_cur)
        { // last active item
          pop = 2;
          nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
          nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.hover,  nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]));
        }
        if(nk_button_label(&vkdt.ctx, vkdt.graph_dev.history_item[i]))
        {
          dt_graph_history_set(&vkdt.graph_dev, i);
          vkdt.graph_dev.runflags = s_graph_run_all;
        }
        if(pop)
        {
          nk_style_pop_style_item(&vkdt.ctx);
          nk_style_pop_style_item(&vkdt.ctx);
        }
      }
      nk_style_pop_flags(&vkdt.ctx);
    }
    NK_UPDATE_ACTIVE;
    nk_end(&vkdt.ctx);
  } // end history panel on the left

  struct nk_context *ctx = &vkdt.ctx;
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  bounds = (struct nk_rect){ vkdt.win.width - vkdt.state.panel_wd, 0, vkdt.state.panel_wd, vkdt.state.panel_ht };
  if(!vkdt.wstate.fullscreen_view && nk_begin(ctx, "darkroom panel right", bounds, (disabled ? NK_WINDOW_NO_INPUT : 0)))
  { // right panel
    // draw histogram image:
    dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
    if(out_hist && vkdt.graph_res[display_frame] == VK_SUCCESS && out_hist->dset[display_frame])
    {
      int wd = vkdt.state.panel_wd;
      int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
      nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
      struct nk_image img = nk_image_ptr(out_hist->dset[display_frame]);
      nk_image(ctx, img);
    }

    dt_node_t *out_view0 = dt_graph_get_display(&vkdt.graph_dev, dt_token("view0"));
    if(out_view0 && vkdt.graph_res[display_frame] == VK_SUCCESS && out_view0->dset[display_frame])
    {
      float iwd = out_view0->connector[0].roi.wd;
      float iht = out_view0->connector[0].roi.ht;
      float scale = MIN(vkdt.state.panel_wd / iwd, 2.0f/3.0f*vkdt.state.panel_wd / iht);
      int ht = scale * iht; // wd = scale * iwd;
      nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
      struct nk_image img = nk_image_ptr(out_view0->dset[display_frame]);
      nk_image(ctx, img);
    }

    { // print some basic exif if we have
      const dt_image_params_t *ip = &vkdt.graph_dev.module[0].img_param;
      if(ip->exposure != 0.0f)
      {
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
        if(ip->exposure >= 1.0f)
          if(nearbyintf(ip->exposure) == ip->exposure)
            nk_labelf(ctx, NK_TEXT_LEFT, "%s %s %.0f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
                (int)ip->focal_length, (int)ip->iso);
          else
            nk_labelf(ctx, NK_TEXT_LEFT, "%s %s %.1f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
                (int)ip->focal_length, (int)ip->iso);
        /* want to catch everything below 0.3 seconds */
        else if(ip->exposure < 0.29f)
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s 1/%.0f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        /* catch 1/2, 1/3 */
        else if(nearbyintf(1.0f / ip->exposure) == 1.0f / ip->exposure)
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s 1/%.0f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        /* catch 1/1.3, 1/1.6, etc. */
        else if(10 * nearbyintf(10.0f / ip->exposure) == nearbyintf(100.0f / ip->exposure))
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s 1/%.1f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        else
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s %.1f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
      }
    }

    if(vkdt.graph_dev.frame_cnt != 1 && vkdt.wstate.dopesheet_view == 0)
    { // print timeline/navigation only if not a still
      const float ratio[] = {0.1f, 0.9f};
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      dt_tooltip("play/pause the animation");
      if(vkdt.state.anim_playing)
      {
        if(nk_button_label(ctx, "\ue047"))
          dt_gui_dr_anim_stop();
      }
      else if(nk_button_label(ctx, "\ue037"))
        dt_gui_dr_anim_start();
      if(nk_widget_is_hovered(ctx))
      {
        char hk[64];
        hk_get_hotkey_lib(hk_darkroom + s_hotkey_insert_keyframe, hk, sizeof(hk));
        dt_tooltip("timeline navigation: set current frame.\n"
            "press space to play/pause and\n"
            "backspace to reset to beginning.\n"
            "hint: you can add keyframes for many controls "
            "in the context menu or hover and press the hotkey\n%s", hk);
      }
      nk_size anim_frame = vkdt.state.anim_frame;
      struct nk_rect bb = nk_widget_bounds(ctx);
      if(nk_progress(ctx, &anim_frame, vkdt.state.anim_max_frame, nk_true))
      {
        vkdt.state.anim_frame = anim_frame;
        vkdt.graph_dev.frame = vkdt.state.anim_frame;
        vkdt.state.anim_no_keyframes = 0;  // (re-)enable keyframes
        dt_graph_apply_keyframes(&vkdt.graph_dev); // rerun once
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
      }
      char text[50];
      bb.x += ctx->style.progress.padding.x;
      bb.y += ctx->style.progress.padding.y;
      snprintf(text, sizeof(text), "frame %d/%d", vkdt.state.anim_frame, vkdt.state.anim_max_frame);
      nk_draw_text(nk_window_get_canvas(ctx), bb, text, strlen(text), nk_glfw3_font(0), nk_rgba(0,0,0,0), nk_rgba(255,255,255,255));
    }

    // tabs for module/params controls:
    nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 0));
    const float pad = row_height * 0.2;
    nk_style_push_vec2(ctx, &ctx->style.button.padding, nk_vec2(pad, pad));
    nk_style_push_float(ctx, &ctx->style.button.rounding, 0);
    nk_layout_row_begin(ctx, NK_STATIC, row_height, 3);
    const char *names[] = {"favourites", "tweak all", "esoteric"};
    static int current_tab = 0;
    for(int i = 0; i < 3; i++)
    {
      const struct nk_user_font *f = ctx->style.font;
      float text_width = f->width(f->userdata, f->height, names[i], nk_strlen(names[i]));
      float widget_width = text_width + 3 * ctx->style.button.padding.x;
      nk_layout_row_push(ctx, widget_width+5);
      if (current_tab == i) 
      {
        struct nk_style_item button_color = ctx->style.button.normal;
        ctx->style.button.normal = ctx->style.button.active;
        current_tab = nk_button_label(ctx, names[i]) ? i: current_tab;
        ctx->style.button.normal = button_color;
      } else current_tab = nk_button_label(ctx, names[i]) ? i: current_tab;
    }
    nk_style_pop_float(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_vec2(ctx);
    nk_layout_row_dynamic(ctx, 2, 1);
    nk_rule_horizontal(ctx, vkdt.style.colour[NK_COLOR_BUTTON_ACTIVE], nk_true);

    const float ratio[] = {0.7, 0.3};
    const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;

    if(current_tab == 0)
    {
      vkdt.graph_dev.active_module = -1;
      render_darkroom_favourite();
    }
    else if(current_tab == 1)
    {
      nk_layout_row_dynamic(ctx, row_height, 2);
      if(nk_widget_is_hovered(ctx))
      {
        char hk[64];
        hk_get_hotkey_lib(hk_darkroom + s_hotkey_nodes_enter, hk, sizeof(hk));
        dt_tooltip("show node graph editor\n%s", hk);
      }
      if(nk_button_label(ctx, "node editor"))
        dt_view_switch(s_view_nodes);
      if(nk_widget_is_hovered(ctx))
      {
        char hk0[64], hk1[64], hk2[64];
        hk_get_hotkey_lib(hk_darkroom + s_hotkey_show_history, hk0, sizeof(hk0));
        hk_get_hotkey_lib(hk_darkroom + s_hotkey_undo, hk1, sizeof(hk1));
        hk_get_hotkey_lib(hk_darkroom + s_hotkey_redo, hk2, sizeof(hk2));
        dt_tooltip("show edit history in left panel\n%s\n%s\n%s", hk0, hk1, hk2);
      }
      const int history_on = vkdt.wstate.history_view;
      if(history_on)
      {
        nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
        nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.button.hover,  nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]));
      }
      if(nk_button_label(ctx, "history"))
        dt_gui_dr_toggle_history();
      if(history_on)
      {
        nk_style_pop_style_item(&vkdt.ctx);
        nk_style_pop_style_item(&vkdt.ctx);
      }
      render_darkroom_full(filter_name, filter_inst);
    }
    else if(current_tab == 2)
    {
      vkdt.graph_dev.active_module = -1;
      if(nk_tree_push(ctx, NK_TREE_TAB, "settings", NK_MINIMIZED))
      {
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
        if(nk_button_label(ctx, "hotkeys"))
          dt_gui_edit_hotkeys();
        if(nk_button_label(ctx, "toggle perf overlay"))
          vkdt.wstate.show_perf_overlay ^= 1;
        nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);

        dt_tooltip("level of detail: 1 means full res, any higher number will render on reduced resolution.");
        nk_tab_property(int, ctx, "#", 1, &vkdt.wstate.lod_fine, 16, 1, 1);
        dt_gui_set_lod(vkdt.wstate.lod_fine);
        nk_label(ctx, "level of detail", NK_TEXT_LEFT);

        dt_tooltip("level of detail while dragging a slider, for instance. you can set this to higher/coarser than the lod above.");
        nk_tab_property(int, ctx, "#", 1, &vkdt.wstate.lod_interact, 16, 1, 1);
        nk_label(ctx, "dynamic LOD", NK_TEXT_LEFT);
        vkdt.wstate.lod_interact = MAX(vkdt.wstate.lod_fine, vkdt.wstate.lod_interact); // enfore dynamic is coarser/equal
        nk_tree_pop(ctx);
      }

      if(nk_tree_push(ctx, NK_TREE_TAB, "animation", NK_MINIMIZED))
      { // animation controls
        nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
        int resi = vkdt.state.anim_max_frame;
        nk_tab_property(int, ctx, "#", 0, &resi, 10000, 1, 1);
        if(resi != vkdt.state.anim_max_frame) 
        {
          vkdt.state.anim_max_frame = resi;
          vkdt.graph_dev.frame_cnt = vkdt.state.anim_max_frame+1;
          dt_graph_history_global(&vkdt.graph_dev);
        }
        nk_label(ctx, "last frame", NK_TEXT_LEFT);
        float resf = vkdt.graph_dev.frame_rate;
        nk_tab_property(float, ctx, "#", 0, &resf, 200, 1, 1);
        if(resf != vkdt.graph_dev.frame_rate)
        {
          vkdt.graph_dev.frame_rate = resf; // conv to double
          dt_graph_history_global(&vkdt.graph_dev);
        }
        nk_label(ctx, "frame rate", NK_TEXT_LEFT);
        if(vkdt.graph_dev.frame_cnt != 1)
        {
          if(nk_widget_is_hovered(ctx))
          {
            char hk[64];
            hk_get_hotkey_lib(hk_darkroom + s_hotkey_dopesheet, hk, sizeof(hk));
            dt_tooltip("toggle keyframe editor\n%s", hk);
          }
          if(vkdt.wstate.dopesheet_view <= 0.0f && nk_button_label(ctx, "show dopesheet"))
          {
            dt_gui_dr_show_dopesheet();
          }
          else if(vkdt.wstate.dopesheet_view > 0.0f && nk_button_label(ctx, "hide dopesheet"))
          {
            dt_gui_dr_hide_dopesheet();
          }
          nk_label(ctx, "", 0);
          dt_tooltip("this is useful if an animated graph has output modules\n"
              "attached to it. for instance this allows you to trigger\n"
              "writing of intermediate results of an optimisation from the gui.\n"
              "only works when the animation is stopped.");
          if(nk_button_label(ctx, "force downloading all outputs"))
          {
            vkdt.graph_dev.runflags = s_graph_run_download_sink;
          }
          nk_label(ctx, "", 0);
        }
        nk_tree_pop(ctx);
      }

      if(nk_tree_push(ctx, NK_TREE_TAB, "presets", NK_MINIMIZED))
      {
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
        if(nk_widget_is_hovered(ctx))
        {
          char hk[64];
          hk_get_hotkey_lib(hk_darkroom + s_hotkey_create_preset, hk, sizeof(hk));
          dt_tooltip("form a new preset from items in the current history\n%s", hk);
        }
        if(nk_button_label(ctx, "create preset"))
          dt_gui_dr_preset_create();
        if(nk_widget_is_hovered(ctx))
        {
          char hk[64];
          hk_get_hotkey_lib(hk_darkroom + s_hotkey_apply_preset, hk, sizeof(hk));
          dt_tooltip("select preset from the list\n%s", hk);
        }
        if(nk_button_label(ctx, "apply preset"))
          dt_gui_preset_apply();
        nk_tree_pop(ctx);
      }
    }
    NK_UPDATE_ACTIVE;
    nk_end(ctx);
  } // end right panel

  // popup windows
  render_darkroom_modals(); // this is shared with nodes view, but this last one is distinct:
  bounds = nk_rect(vkdt.state.center_x+0.2*vkdt.state.center_wd, vkdt.state.center_y+0.2*vkdt.state.center_ht,
    0.6*vkdt.state.center_wd, 0.6*vkdt.state.center_ht);
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
  {
    if(nk_begin(&vkdt.ctx, "edit darkroom hotkeys", bounds, NK_WINDOW_NO_SCROLLBAR))
    {
      int ok = hk_edit(hk_darkroom, hk_darkroom_cnt);
      if(ok) vkdt.wstate.popup = 0;
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
  gui.pgupdn = 0;  // reset rotary encoder knob counter
  gui.hotkey = -1; // reset hotkey, we worked on all we could
}

void render_darkroom_init()
{
  gui.active_module = 0;
  gui.active_instance = 0;
  hk_darkroom_cnt = dt_keyaccel_init(&keyaccel, hk_darkroom, hk_darkroom_cnt, hk_darkroom_size);
  hk_deserialise("darkroom", hk_darkroom, hk_darkroom_cnt);
}

void render_darkroom_cleanup()
{
  hk_serialise("darkroom", hk_darkroom, hk_darkroom_cnt);
  dt_keyaccel_cleanup(&keyaccel);
  widget_end(); // commit params if still ongoing
}

void
darkroom_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  double x, y;
  dt_view_get_cursor_pos(vkdt.win.window, &x, &y);
  if(!vkdt.wstate.grabbed && (vkdt.graph_dev.active_module >= 0))
  { // pass on mouse events on dspy window
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.graph_dev.active_module;
    struct nk_rect b = vkdt.wstate.active_dspy_bound;
    if(action == 0 || NK_INBOX(x,y,b.x,b.y,b.w,b.h))
    {
      static double last_button_click = 0.0;
      double dt = glfwGetTime() - last_button_click;
      if(action)
      { // double click?
        last_button_click = glfwGetTime();
        if(dt > 0.02 && dt < 0.25) button = 3;
      }
      dt_module_input_event_t p = {
        .type = 1,
        .x = (x-b.x)/b.w,
        .y = 1.0-(y-b.y)/b.h,
        .mbutton = button,
        .action  = action,
        .mods    = mods,
        .grabbed = vkdt.wstate.grabbed,
      };
      if(mod->so->input)
        vkdt.graph_dev.runflags |= mod->so->input(mod, &p);
    }
  }

  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type = 1,
      .x = x,
      .y = y,
      .mbutton = button,
      .action  = action,
      .mods    = mods,
      .grabbed = vkdt.wstate.grabbed,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(vkdt.wstate.active_widget_modid >= 0)
      if(mod->so->input) mod->so->input(mod, &p);
    return;
  }
}

void
darkroom_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  double x, y;
  dt_view_get_cursor_pos(vkdt.win.window, &x, &y);

  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type = 3,
      .dx = xoff,
      .dy = yoff,
      .grabbed = vkdt.wstate.grabbed,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(vkdt.wstate.active_widget_modid >= 0)
      if(mod->so->input) mod->so->input(mod, &p);
    return;
  }
}

void
darkroom_mouse_position(GLFWwindow* window, double x, double y)
{
  if(!vkdt.wstate.grabbed && (vkdt.graph_dev.active_module >= 0))
  { // pass on mouse events on dspy window
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.graph_dev.active_module;
    struct nk_rect b = vkdt.wstate.active_dspy_bound;
    dt_module_input_event_t p = {
      .type = 2,
      .x = CLAMP((x-b.x)/b.w, 0, 1),
      .y = CLAMP(1.0-(y-b.y)/b.h, 0, 1),
      .grabbed = vkdt.wstate.grabbed,
    };
    if(mod->so->input) // always send to active module, just clamp coordinates
      vkdt.graph_dev.runflags |= mod->so->input(mod, &p);
  }
  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type = 2,
      .x = x,
      .y = y,
      .grabbed = vkdt.wstate.grabbed,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(vkdt.wstate.active_widget_modid >= 0)
      if(mod->so->input) mod->so->input(mod, &p);
  }
}

void
darkroom_process()
{
  static int    start_frame = 0;           // frame we were when play was issued
  static struct timespec start_time = {0}; // start time of the same event

  struct timespec beg;
  clock_gettime(CLOCK_REALTIME, &beg);

  int advance = 0;
  if(vkdt.state.anim_playing)
  {
    if(vkdt.graph_dev.frame_rate == 0.0)
    {
      advance = 1; // no frame rate set, run as fast as we can
      vkdt.state.anim_frame = CLAMP(vkdt.graph_dev.frame + 1, 0, (uint32_t)vkdt.graph_dev.frame_cnt-1);
    }
    else
    { // just started replay, record timestamp:
      if(start_time.tv_nsec == 0)
      {
        start_time  = beg;
        start_frame = vkdt.state.anim_frame;
      }
      // compute current animation frame by time:
      double dt = (double)(beg.tv_sec - start_time.tv_sec) + 1e-9*(beg.tv_nsec - start_time.tv_nsec);
      vkdt.state.anim_frame = CLAMP(
          start_frame + MAX(0, vkdt.graph_dev.frame_rate * dt),
          0, (uint32_t)vkdt.graph_dev.frame_cnt-1);
      if(vkdt.graph_dev.frame > start_frame &&
         vkdt.graph_dev.frame == vkdt.state.anim_frame)
        vkdt.graph_dev.runflags = 0; // no need to re-render
      else advance = 1;
    }
    if(advance)
    {
      if(vkdt.state.anim_frame > vkdt.graph_dev.frame + 1)
        dt_log(s_log_snd, "frame drop warning, audio may stutter!");
      vkdt.graph_dev.frame = vkdt.state.anim_frame;
      if(!vkdt.state.anim_no_keyframes)
        dt_graph_apply_keyframes(&vkdt.graph_dev);
      if(vkdt.graph_dev.frame_cnt == 0 || vkdt.state.anim_frame < vkdt.state.anim_max_frame+1)
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    }
    if(vkdt.state.anim_frame == vkdt.graph_dev.frame_cnt - 1)
      dt_gui_dr_anim_stop(); // reached the end, stop.
  }
  else
  { // if no animation, reset time stamp
    start_time = (struct timespec){0};
  }

  int reset_view = 0;
  dt_roi_t old_roi;
  if(vkdt.graph_dev.runflags & s_graph_run_roi)
  {
    reset_view = 1;
    dt_node_t *md = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(md) old_roi = md->connector[0].roi;
  }

  const int grabbed = vkdt.wstate.grabbed;
  if(!grabbed)
  { // async graph compute vs. sync cpu + gui rendering
    // graph->double_buffer points to the buffer currently locked for render/display
    // set graph_res = -1 initially (when entering dr mode) so we won't draw before it finished processing
    static int running = 0; // 1-double buffer is still running on gpu, has not been swapped in yet
    if(vkdt.graph_res[vkdt.graph_dev.double_buffer^1] == -1)
      running = 1; // when entering dr mode graph_res is -1 and it will kick off 0 as running

    if((!running && vkdt.graph_dev.runflags) || // stills and stopped animations
       (!running && vkdt.graph_dev.runflags && vkdt.state.anim_playing && advance)) // running animations only if frame advances
    { // double buffered async compute
      vkdt.graph_dev.double_buffer ^= 1; // work on the one that's not currently locked
      vkdt.graph_res[vkdt.graph_dev.double_buffer] =
        dt_graph_run(&vkdt.graph_dev, (vkdt.graph_dev.runflags & ~s_graph_run_wait_done));
      if(vkdt.graph_res[vkdt.graph_dev.double_buffer] != VK_SUCCESS)
        dt_gui_notification("failed to run the graph %s!",
            qvk_result_to_string(vkdt.graph_res[vkdt.graph_dev.double_buffer]));
      vkdt.graph_dev.runflags = 0; // clear this here, running graph has a copy.
      vkdt.graph_dev.double_buffer ^= 1; // reset to the locked/already finished one
      running = 1;
    }
    if(running)
    {
      uint64_t value;
      VkResult res = vkGetSemaphoreCounterValue(qvk.device, vkdt.graph_dev.semaphore_process, &value);
      if(res == VK_SUCCESS && value >= vkdt.graph_dev.process_dbuffer[vkdt.graph_dev.double_buffer^1])
      {
        if(vkdt.graph_res[vkdt.graph_dev.double_buffer^1] == -1)
          vkdt.graph_res[vkdt.graph_dev.double_buffer^1] = VK_SUCCESS; // let display now it's now good to show
        running = 0;
        vkdt.graph_dev.double_buffer ^= 1; // flip double buffer frame
      }
    }
  }
  else // if grabbed
  { // swap double buffers and run for grabbed animations
    if(vkdt.graph_dev.runflags) // stills and stopped animations
    { // double buffered compute
      // this will wait for other run
      // process double_buffer, wait for double_buffer^1
      vkdt.graph_res[vkdt.graph_dev.double_buffer] =
        dt_graph_run(&vkdt.graph_dev, (vkdt.graph_dev.runflags & ~s_graph_run_wait_done));
      if(vkdt.graph_res[vkdt.graph_dev.double_buffer] != VK_SUCCESS)
        dt_gui_notification("failed to run the graph %s!",
            qvk_result_to_string(vkdt.graph_res[vkdt.graph_dev.double_buffer]));
      vkdt.graph_dev.runflags = 0; // we started this
      VkSemaphoreWaitInfo wait_info = {
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores    = &vkdt.graph_dev.semaphore_process,
        .pValues        = &vkdt.graph_dev.process_dbuffer[vkdt.graph_dev.double_buffer^1],
      };
      VkResult res = vkWaitSemaphores(qvk.device, &wait_info, ((uint64_t)1)<<30);
      if(res == VK_SUCCESS)
        vkdt.graph_dev.double_buffer ^= 1; // lock ^1 as display buffer, we waited for it to complete
    }
  }
  if(reset_view)
  {
    dt_node_t *md = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(md && memcmp(&old_roi, &md->connector[0].roi, sizeof(dt_roi_t))) // did the output roi change?
      dt_image_reset_zoom(&vkdt.wstate.img_widget);
  }

  if(vkdt.state.anim_playing && advance)
  { // new frame for animations need new audio, too
    dt_graph_t *g = &vkdt.graph_dev;
    for(int i=0;i<g->num_modules;i++)
    { // find first audio module, if any
      if(g->module[i].name == 0) continue;
      if(g->module[i].so->audio)
      {
        uint16_t *samples;
        if(g->frame_rate > 0)
        { // fixed frame rate, maybe video
          int samples_per_frame = g->module[i].img_param.snd_samplerate / g->frame_rate;
          uint64_t pos = g->frame * samples_per_frame;
          uint32_t left = samples_per_frame;
          while(left)
          {
            int cnt = g->module[i].so->audio(g->module+i, pos, left, &samples);
            left -= cnt; pos += cnt;
            if(cnt > 0) dt_snd_play(&vkdt.snd, samples, cnt);
            else break;
          }
        }
        else
        { // go as fast as we can, probably a game engine. ask only once
          int cnt = g->module[i].so->audio(g->module+i, 0, 0, &samples);
          if(cnt > 0) dt_snd_play(&vkdt.snd, samples, cnt);
        }
        break;
      }
    }
  }

  // struct timespec end;
  // clock_gettime(CLOCK_REALTIME, &end);
  // double dt = (double)(end.tv_sec - beg.tv_sec) + 1e-9*(end.tv_nsec - beg.tv_nsec);
  // dt_log(s_log_perf, "frame time %2.3fs", dt);
}

int
darkroom_enter()
{
  vkdt.wstate.lod_fine     = dt_rc_get_int(&vkdt.rc, "gui/lod", 1); // set finest lod by default
  vkdt.wstate.lod_interact = dt_rc_get_int(&vkdt.rc, "gui/lod_interact", 1);
  vkdt.state.anim_frame = 0;
  dt_gui_dr_anim_stop();
  dt_image_reset_zoom(&vkdt.wstate.img_widget);
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.active_widget_parid = -1;
  dt_radial_menu_dr_close(&vkdt.wstate.radial_menu_dr);
  vkdt.wstate.mapped = 0;
  vkdt.wstate.selected = -1;
  uint32_t imgid = dt_db_current_imgid(&vkdt.db);
  if(imgid == -1u) return 1;
  char graph_cfg[PATH_MAX+100];
  dt_db_image_path(&vkdt.db, imgid, graph_cfg, sizeof(graph_cfg));

  // stat, if doesn't exist, load default
  // always set filename param? (definitely do that for default cfg)
  int load_default = 0;
  char realimg[PATH_MAX];
  dt_token_t input_module = dt_token("i-raw");
  struct stat statbuf;
  if(stat(graph_cfg, &statbuf))
  {
    fs_realpath(graph_cfg, realimg); // depend on GNU extension in case of ENOENT (to cut out /../ and so on)
    int len = strlen(realimg);
    assert(len > 4);
    realimg[len-4] = 0; // cut away ".cfg"
    input_module = dt_graph_default_input_module(realimg);
    snprintf(graph_cfg, sizeof(graph_cfg), "default-darkroom.%"PRItkn, dt_token_str(input_module));
    load_default = 1;
  }

  dt_graph_init(&vkdt.graph_dev, s_queue_compute);
  vkdt.graph_dev.gui_attached = 1;
  vkdt.graph_dev.gui_colour[0] = vkdt.style.colour[NK_COLOR_DT_ACCENT].r/255.0f;
  vkdt.graph_dev.gui_colour[1] = vkdt.style.colour[NK_COLOR_DT_ACCENT].g/255.0f;
  vkdt.graph_dev.gui_colour[2] = vkdt.style.colour[NK_COLOR_DT_ACCENT].b/255.0f;
  vkdt.graph_dev.gui_colour[3] = vkdt.style.colour[NK_COLOR_DT_ACCENT].a/255.0f;
  vkdt.graph_dev.gui_colour[4] = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER].r/255.0f;
  vkdt.graph_dev.gui_colour[5] = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER].g/255.0f;
  vkdt.graph_dev.gui_colour[6] = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER].b/255.0f;
  vkdt.graph_dev.gui_colour[7] = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER].a/255.0f;
  dt_graph_history_init(&vkdt.graph_dev);

  if(dt_graph_read_config_ascii(&vkdt.graph_dev, graph_cfg))
  {
    dt_log(s_log_err|s_log_gui, "could not load graph configuration from '%s'!", graph_cfg);
    dt_graph_cleanup(&vkdt.graph_dev);
    return 2;
  }

  if(load_default)
  {
    dt_graph_set_searchpath(&vkdt.graph_dev, realimg);
    char *basen = fs_basename(realimg); // cut away path so we can relocate more easily
    int modid = dt_module_get(&vkdt.graph_dev, input_module, dt_token("main"));
    if(modid < 0 ||
       dt_module_set_param_string(vkdt.graph_dev.module + modid, dt_token("filename"),
         basen))
    {
      dt_log(s_log_err|s_log_gui, "config '%s' has no valid input module!", graph_cfg);
      dt_graph_cleanup(&vkdt.graph_dev);
      return 3;
    }
  }
  dt_graph_history_reset(&vkdt.graph_dev);

  vkdt.graph_dev.active_module = dt_module_get(&vkdt.graph_dev, gui.active_module, gui.active_instance);
  if(vkdt.graph_dev.active_module >= 0)
  { // if we don't find it, this will be -1
    // make sure this thing is on the current graph
    int active_on_graph = 0;
    dt_graph_t *graph = &vkdt.graph_dev;
    dt_module_t *const arr = graph->module;
    const int arr_cnt = graph->num_modules;
#define TRAVERSE_POST if(curr == vkdt.graph_dev.active_module) active_on_graph=1;
#include "pipe/graph-traverse.inc"
#undef TRAVERSE_POST
    if(active_on_graph)
    { // only automatically connect dspy if we think the module is on the graph
      int cid = dt_module_get_connector(vkdt.graph_dev.module+vkdt.graph_dev.active_module, dt_token("dspy"));
      if(cid >= 0)
      { // connect dspy
        int mid = dt_module_add(&vkdt.graph_dev, dt_token("display"), dt_token("dspy"));
        if(mid >= 0)
        {
          dt_module_connect(&vkdt.graph_dev, vkdt.graph_dev.active_module, cid, mid, 0); // reconnect
          const float pwd = vkdt.style.panel_width_frac * vkdt.win.width;
          vkdt.graph_dev.module[mid].connector[0].max_wd = pwd;
          vkdt.graph_dev.module[mid].connector[0].max_ht = pwd;
          vkdt.graph_dev.runflags = s_graph_run_all;
        }
      }
    }
    else vkdt.graph_dev.active_module = -1;
  }

  dt_gui_set_lod(vkdt.wstate.lod_fine);
  vkdt.graph_res[0] = vkdt.graph_res[1] = VK_INCOMPLETE; // invalidate
  if((vkdt.graph_res[vkdt.graph_dev.double_buffer] =
        dt_graph_run(&vkdt.graph_dev, s_graph_run_all & ~s_graph_run_wait_done)) != VK_SUCCESS)
    dt_gui_notification("running the graph failed (%s)!",
        qvk_result_to_string(vkdt.graph_res[vkdt.graph_dev.double_buffer]));
  if(vkdt.graph_res[vkdt.graph_dev.double_buffer] == VK_SUCCESS)
    vkdt.graph_res[vkdt.graph_dev.double_buffer] = -1;
  vkdt.graph_dev.double_buffer = 1; // we are rendering to 0, make sure the display code uses this dset after swapping

  // nodes are only constructed after running once
  // (could run up to s_graph_run_create_nodes)
  if(!dt_graph_get_display(&vkdt.graph_dev, dt_token("main")))
    dt_gui_notification("graph does not contain a display:main node!");

  // do this after running the graph, it may only know
  // after initing say the output roi, after loading an input file
  vkdt.state.anim_max_frame = vkdt.graph_dev.frame_cnt-1;

  // rebuild gui specific to this image
  dt_gui_read_favs("darkroom.ui");
#if 1//ndef QVK_ENABLE_VALIDATION // debug build does not reset zoom (reload shaders keeping focus is nice)
  dt_image_reset_zoom(&vkdt.wstate.img_widget);
#endif

  if(vkdt.graph_dev.frame_cnt == 1) dt_gui_dr_hide_dopesheet();

  dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "back to lighttable");
  dt_gamepadhelp_set(dt_gamepadhelp_ps, "toggle this help");
  dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_L, "pan around");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_up, "anim: play");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_down, "anim: rewind");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_right, "next image");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_left, "prev image");
  dt_gamepadhelp_set(dt_gamepadhelp_button_triangle, "upvote and next");
  dt_gamepadhelp_set(dt_gamepadhelp_button_square, "downvote and next");
  dt_gamepadhelp_set(dt_gamepadhelp_L1, "hold to show menu, use with right stick");
  dt_gamepadhelp_set(dt_gamepadhelp_L2, "zoom out");
  dt_gamepadhelp_set(dt_gamepadhelp_R1, "show/hide right panel");
  dt_gamepadhelp_set(dt_gamepadhelp_R2, "zoom in");
  dt_gamepadhelp_set(dt_gamepadhelp_L3, "reset zoom");
  // dt_gamepadhelp_set(dt_gamepadhelp_R3, "reset focussed control");
  return 0;
}

int
darkroom_leave()
{
  dt_rc_set_int(&vkdt.rc, "gui/lod",          vkdt.wstate.lod_fine);
  dt_rc_set_int(&vkdt.rc, "gui/lod_interact", vkdt.wstate.lod_interact);
  dt_gui_dr_anim_stop();
  char filename[1024];
  dt_db_image_path(&vkdt.db, dt_db_current_imgid(&vkdt.db), filename, sizeof(filename));
  if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
    dt_graph_write_config_ascii(&vkdt.graph_dev, filename);

  if(vkdt.graph_dev.frame_cnt != 1) dt_gui_label_set(s_image_label_video);
  else dt_gui_label_unset(s_image_label_video);

  if(vkdt.graph_dev.active_module >= 0)
  {
    gui.active_module   = vkdt.graph_dev.module[vkdt.graph_dev.active_module].name;
    gui.active_instance = vkdt.graph_dev.module[vkdt.graph_dev.active_module].inst;
  }

  // TODO: start from already loaded/inited graph instead of from scratch!
  const uint32_t imgid = dt_db_current_imgid(&vkdt.db);
  dt_thumbnails_cache_list(
      &vkdt.thumbnail_gen,
      &vkdt.db,
      &imgid, 1,
      &glfwPostEmptyEvent);

  // TODO: repurpose instead of cleanup!
  dt_graph_cleanup(&vkdt.graph_dev);
  dt_graph_history_cleanup(&vkdt.graph_dev);
  vkdt.graph_res[0] = vkdt.graph_res[1] = VK_INCOMPLETE; // invalidate
  dt_gamepadhelp_clear();
  dt_gui_write_favs("darkroom.ui");
  return 0;
}

void
darkroom_pentablet_data(double x, double y, double z, double pressure, double pitch, double yaw, double roll)
{
  dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  if(!out) return; // should never happen
  if(vkdt.wstate.active_widget_modid >= 0)
  {
    dt_token_t type = vkdt.graph_dev.module[
        vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(type == dt_token("draw"))
    {
      float v[] = {(float)x, (float)y}, n[2] = {0};
      dt_image_from_view(&vkdt.wstate.img_widget, v, n);
#ifdef __APPLE__
     if(glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_CONTROL)  != GLFW_PRESS &&
        glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_CONTROL) != GLFW_PRESS)
#else
      if(glfwGetMouseButton(vkdt.win.window, GLFW_MOUSE_BUTTON_MIDDLE) != GLFW_PRESS)
#endif
        dt_gui_dr_draw_position(n, pressure);
    }
  }
}

void
darkroom_gamepad(GLFWwindow *window, GLFWgamepadstate *last, GLFWgamepadstate *curr)
{
#define PRESSED(A) curr->buttons[A] && !last->buttons[A]
  if(PRESSED(GLFW_GAMEPAD_BUTTON_A))
  {
    if(dt_radial_menu_dr_active(&vkdt.wstate.radial_menu_dr))
      dt_radial_menu_dr_close(&vkdt.wstate.radial_menu_dr);
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_B))
  {
    if(dt_radial_menu_dr_active(&vkdt.wstate.radial_menu_dr))
      dt_radial_menu_dr_reset(&vkdt.wstate.radial_menu_dr);
    else
      dt_view_switch(s_view_lighttable);
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT))
  {
    dt_gui_dr_next();
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_LEFT))
  {
    dt_gui_dr_prev();
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_UP))
  {
    dt_gui_dr_play();
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_DOWN))
  {
    dt_gui_dr_rewind();
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_X))
  {
    dt_gui_dr_advance_downvote();
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_Y))
  {
    dt_gui_dr_advance_upvote();
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_LEFT_THUMB)) // left stick pressed
  {
    dt_image_reset_zoom(&vkdt.wstate.img_widget);
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER))
  {
    dt_gui_toggle_fullscreen_view();
  }

  dt_node_t *out_main = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  if(out_main)
  {
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
    float wd  = (float)out_main->connector[0].roi.wd;
    float ht  = (float)out_main->connector[0].roi.ht;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
    float imwd = win_w, imht = win_h;
    float scale = MIN(imwd/wd, imht/ht);
    if(vkdt.wstate.img_widget.scale > 0.0f) scale = vkdt.wstate.img_widget.scale;
    float ax = curr->axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    float ay = curr->axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    float zo = curr->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    float zi = curr->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
    if(zi > -1.0f) scale *= powf(2.0, -0.04*SMOOTH(zi+1.0f)); 
    if(zo > -1.0f) scale *= powf(2.0,  0.04*SMOOTH(zo+1.0f)); 
    // scale *= powf(2.0, -0.1*SMOOTH(axes[4])); 
    vkdt.wstate.img_widget.look_at_x += SMOOTH(ax) * wd * 0.01 / scale;
    vkdt.wstate.img_widget.look_at_y += SMOOTH(ay) * ht * 0.01 / scale;
    vkdt.wstate.img_widget.scale = scale;
#undef SMOOTH
  }
#undef PRESSED
}
