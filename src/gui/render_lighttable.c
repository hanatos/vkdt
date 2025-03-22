// the imgui render functions for the lighttable view
#include "core/version.h"
#include "gui/view.h"
#include "gui/gui.h"
#include "db/thumbnails.h"
#include "db/rc.h"
#include "db/hash.h"
#include "core/fs.h"
#include "pipe/graph-defaults.h"
#include "gui/render_view.h"
#include "gui/hotkey.h"
#include "gui/widget_thumbnail.h"
#include "gui/widget_recentcollect.h"
#include "gui/widget_export.h"
#include "gui/api_gui.h"
#include "gui/render.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdatomic.h>


static hk_t hk_lighttable[] = {
  {"tag",           "assign a tag to selected images",  {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_T}},
  {"select all",    "toggle select all/none",           {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_A}},
  {"export",        "export selected images",           {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_S}},
  {"copy",          "copy from selected image",         {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_C}},
  {"paste history", "paste history to selected images", {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_V}},
  {"append preset", "append preset to selected images", {GLFW_KEY_LEFT_CONTROL,  GLFW_KEY_P}},
  {"scroll cur",    "scroll to current image",          {GLFW_KEY_LEFT_SHIFT,    GLFW_KEY_C}},
  {"scroll end",    "scroll to end of collection",      {GLFW_KEY_LEFT_SHIFT,    GLFW_KEY_G}},
  {"scroll top",    "scroll to top of collection",      {GLFW_KEY_G}},
  {"duplicate",     "duplicate selected images",        {GLFW_KEY_LEFT_SHIFT,    GLFW_KEY_D}},
  {"rate 0",        "assign zero stars",                {GLFW_KEY_0}},
  {"rate 1",        "assign one star",                  {GLFW_KEY_1}},
  {"rate 2",        "assign two stars",                 {GLFW_KEY_2}},
  {"rate 3",        "assign three stars",               {GLFW_KEY_3}},
  {"rate 4",        "assign four stars",                {GLFW_KEY_4}},
  {"rate 5",        "assign five stars",                {GLFW_KEY_5}},
  {"label red",     "toggle red label",                 {GLFW_KEY_F1}},
  {"label green",   "toggle green label",               {GLFW_KEY_F2}},
  {"label blue",    "toggle blue label",                {GLFW_KEY_F3}},
  {"label yellow",  "toggle yellow label",              {GLFW_KEY_F4}},
  {"label purple",  "toggle purple label",              {GLFW_KEY_F5}},
  {"zoom in",       "decrease images per row",          {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_UP}},
  {"zoom out",      "increase images per row",          {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_DOWN}},
};
typedef enum hotkey_names_t
{
  s_hotkey_assign_tag    = 0,
  s_hotkey_select_all    = 1,
  s_hotkey_export        = 2,
  s_hotkey_copy_hist     = 3,
  s_hotkey_paste_hist    = 4,
  s_hotkey_append_preset = 5,
  s_hotkey_scroll_cur    = 6,
  s_hotkey_scroll_end    = 7,
  s_hotkey_scroll_top    = 8,
  s_hotkey_duplicate     = 9,
  s_hotkey_rate_0        = 10,
  s_hotkey_rate_1        = 11,
  s_hotkey_rate_2        = 12,
  s_hotkey_rate_3        = 13,
  s_hotkey_rate_4        = 14,
  s_hotkey_rate_5        = 15,
  s_hotkey_label_1       = 16,
  s_hotkey_label_2       = 17,
  s_hotkey_label_3       = 18,
  s_hotkey_label_4       = 19,
  s_hotkey_label_5       = 20,
  s_hotkey_zoom_in       = 21,
  s_hotkey_zoom_out      = 22,
} hotkey_names_t;
static int g_hotkey = -1; // to pass hotkey from handler to rendering. necessary for scrolling/export
static int g_scroll_colid = -1; // to scroll to certain file name
static int g_scroll_offset = 0; // remember last offset for leave/reenter
static int g_image_cursor = -1; // gui keyboard navigation in the center view, cursor

void
lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(vkdt.wstate.popup == s_popup_edit_hotkeys) // this window needs special
    return hk_keyboard(hk_lighttable, w, key, scancode, action, mods);
  if(dt_gui_input_blocked()) return; // includes other popups

  const int hotkey = action == GLFW_PRESS ? hk_get_hotkey(hk_lighttable, NK_LEN(hk_lighttable), key) : -1;
  switch(hotkey)
  {
    case s_hotkey_duplicate:
      dt_gui_lt_duplicate();
      return;
    case s_hotkey_rate_0: dt_gui_rate_0(); return;
    case s_hotkey_rate_1: dt_gui_rate_1(); return;
    case s_hotkey_rate_2: dt_gui_rate_2(); return;
    case s_hotkey_rate_3: dt_gui_rate_3(); return;
    case s_hotkey_rate_4: dt_gui_rate_4(); return;
    case s_hotkey_rate_5: dt_gui_rate_5(); return;
    case s_hotkey_label_1: dt_gui_label_1(); return;
    case s_hotkey_label_2: dt_gui_label_2(); return;
    case s_hotkey_label_3: dt_gui_label_3(); return;
    case s_hotkey_label_4: dt_gui_label_4(); return;
    case s_hotkey_label_5: dt_gui_label_5(); return;
    case s_hotkey_assign_tag:
      dt_gui_lt_assign_tag();
      return;
    case s_hotkey_select_all:
      dt_gui_lt_toggle_select_all();
      return;
    case s_hotkey_export: // handled during rendering
    case s_hotkey_scroll_cur:
    case s_hotkey_scroll_end:
    case s_hotkey_scroll_top:
      g_image_cursor = -1;
      g_hotkey = hotkey;
      return;
    case s_hotkey_copy_hist:
      dt_gui_copy_history();
      return;
    case s_hotkey_paste_hist:
      dt_gui_paste_history();
      return;
    case s_hotkey_append_preset:
      dt_gui_preset_apply();
      return;
    case s_hotkey_zoom_in:
      dt_gui_lt_zoom_in();
      return;
    case s_hotkey_zoom_out:
      dt_gui_lt_zoom_out();
      return;
    default: break;
  }

  if(action != GLFW_PRESS) return; // only handle key down events
  if(key == GLFW_KEY_ESCAPE)
  {
    dt_view_switch(s_view_files);
  }
  else if(key == GLFW_KEY_ENTER)
  {
    if(g_image_cursor >= 0)
    {
      dt_db_selection_clear(&vkdt.db);
      dt_db_selection_add(&vkdt.db, g_image_cursor);
      dt_view_switch(s_view_darkroom);
    }
    else if(dt_db_current_imgid(&vkdt.db) != -1u)
      dt_view_switch(s_view_darkroom);
  }
  else if(key == GLFW_KEY_UP)
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else if(g_image_cursor >= vkdt.wstate.lighttable_images_per_row) g_image_cursor -= vkdt.wstate.lighttable_images_per_row;
  }
  else if(key == GLFW_KEY_DOWN)
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else if(g_image_cursor < vkdt.db.collection_cnt - vkdt.wstate.lighttable_images_per_row) g_image_cursor += vkdt.wstate.lighttable_images_per_row;
  }
  else if(key == GLFW_KEY_LEFT)
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else g_image_cursor = MAX(0, g_image_cursor-1);
  }
  else if(key == GLFW_KEY_RIGHT)
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else g_image_cursor = MIN(vkdt.db.collection_cnt-1, g_image_cursor+1);
  }
  else if(key == GLFW_KEY_SPACE && g_image_cursor >= 0)
  {
    int shift = mods & GLFW_MOD_SHIFT;
    int ctrl  = mods & GLFW_MOD_CONTROL;
    vkdt.wstate.busy += 2;
    if(ctrl)
    {
      if(vkdt.db.image[vkdt.db.collection[g_image_cursor]].labels & s_image_label_selected)
        dt_db_selection_remove(&vkdt.db, g_image_cursor);
      else
        dt_db_selection_add(&vkdt.db, g_image_cursor);
    }
    else if(shift)
    { // shift selects ranges
      uint32_t colid = dt_db_current_colid(&vkdt.db);
      if(colid != -1u)
      {
        int a = MIN(colid, (uint32_t)g_image_cursor);
        int b = MAX(colid, (uint32_t)g_image_cursor);
        dt_db_selection_clear(&vkdt.db);
        for(int i=a;i<=b;i++)
          dt_db_selection_add(&vkdt.db, i);
      }
    }
    else
    { // no modifier, select exactly this image:
      if(dt_db_selection_contains(&vkdt.db, g_image_cursor))
      {
        dt_db_selection_clear(&vkdt.db);
      }
      else
      {
        dt_db_selection_clear(&vkdt.db);
        dt_db_selection_add(&vkdt.db, g_image_cursor);
      }
    }
  }
}


void render_lighttable_center()
{ // center image view
#if 0 // TODO bring back
  { // assign star rating/colour labels via gamepad:
    if(ImGui::IsKeyDown(ImGuiKey_GamepadFaceUp))
    {
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      int rtdir = 0, lbdir = 0;
      rtdir -= ImGui::IsKeyPressed(ImGuiKey_GamepadL1);
      rtdir += ImGui::IsKeyPressed(ImGuiKey_GamepadR1);
      lbdir -= ImGui::IsKeyPressed(ImGuiKey_GamepadL2);
      lbdir += ImGui::IsKeyPressed(ImGuiKey_GamepadR2);
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      {
        vkdt.db.image[sel[i]].rating = CLAMP(vkdt.db.image[sel[i]].rating + rtdir, 0, 5);
        if(lbdir)
        vkdt.db.image[sel[i]].labels =
          vkdt.db.image[sel[i]].labels ?
          CLAMP(lbdir > 0 ? (vkdt.db.image[sel[i]].labels << 1) :
                            (vkdt.db.image[sel[i]].labels >> 1), 0, 8) : 1;
      }
    }
  }
#endif
  struct nk_rect bounds = {vkdt.state.center_x, vkdt.state.center_y, vkdt.state.center_wd, vkdt.state.center_ht};
  const int disabled = vkdt.wstate.popup;
  nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
  if(!nk_begin(&vkdt.ctx, "lighttable center", bounds, disabled ? NK_WINDOW_NO_INPUT : 0))
  {
    NK_UPDATE_ACTIVE;
    nk_end(&vkdt.ctx);
    nk_style_pop_style_item(&vkdt.ctx);
    return;
  }

  if(vkdt.db.collection_cnt == 0)
  {
    // nk_style_push_font(&vkdt.ctx, &dt_gui_get_font(1)->handle);
    nk_layout_row_dynamic(&vkdt.ctx, vkdt.state.center_ht/5, 1);
    nk_label(&vkdt.ctx, "", 0);
    nk_layout_row_dynamic(&vkdt.ctx, vkdt.state.center_ht/3, 3);
    nk_label(&vkdt.ctx, "", 0);
    nk_labelf_wrap(&vkdt.ctx,
        "this collection is empty. the current directory (%s) contains %d files, "
        "try to relax the filter in the right panel in the `collect' expander. "
        "to open another directory press escape to go to the file browser.", vkdt.db.dirname, vkdt.db.image_cnt);
    nk_label(&vkdt.ctx, "", 0);
    // nk_style_pop_font(&vkdt.ctx);
    NK_UPDATE_ACTIVE;
    nk_end(&vkdt.ctx);
    nk_style_pop_style_item(&vkdt.ctx);
    return;
  }

  const int ipl = vkdt.wstate.lighttable_images_per_row;
  const int border = 0.004 * vkdt.win.width;
  const int spacing = border/2;
  const int wd = vkdt.state.center_wd / ipl - border*2;
  const int ht = wd;
  nk_layout_row_dynamic(&vkdt.ctx, ht + 2*border, ipl);

  struct nk_rect content = nk_window_get_content_region(&vkdt.ctx);
  int scroll_to = -1;
  if(g_scroll_offset > 0) 
    vkdt.ctx.current->scrollbar.y = g_scroll_offset;

  if(g_image_cursor == -2)
  { // set cursor to current image if any:
    g_image_cursor = dt_db_current_colid(&vkdt.db);
    if(g_image_cursor == -1) g_image_cursor = -2; // no current image
  }

  nk_style_push_vec2(&vkdt.ctx, &vkdt.ctx.style.window.spacing, nk_vec2(spacing, spacing));
  for(int i=0;i<vkdt.db.collection_cnt;i++)
  {
    struct nk_rect row = nk_widget_bounds(&vkdt.ctx);
    if(g_scroll_colid == i)
    {
      g_scroll_colid = -1;
      if(row.y < content.y || row.y + row.h > content.y + content.h)
        scroll_to = 1+(row.h + spacing) * (i/ipl); // row.y unreliable/negative in case of out of frustum
    }
    if(g_hotkey == s_hotkey_scroll_cur && vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
      scroll_to = row.y - content.y;
    if(row.y < content.y || row.y + row.h > content.y + content.h)
    { // only half visible
      if(g_image_cursor == i)
      { // i do not understand the nuklear way to compute borders, but this works:
        scroll_to = 1+(row.h + spacing) * (i/ipl); // scroll up
        if(row.y + row.h > content.y + content.h)  // scroll down
          scroll_to = vkdt.ctx.current->scrollbar.y + (row.y + row.h - content.y - content.h) + 1;
        g_scroll_colid = -1;
      }
    }
    if(row.y + row.h <= content.y ||
       row.y > content.y + content.h)
    { // add dummy for invisible thumbnails
      nk_label(&vkdt.ctx, "", 0);
      continue;
    }
    if(row.y - vkdt.ctx.current->scrollbar.y > content.y + 10*content.h) break; // okay this list is really long

    // precache
    if((i % ipl) == 0)
      dt_thumbnails_load_list(
          &vkdt.thumbnails,
          &vkdt.db,
          vkdt.db.collection,
          i, MIN(vkdt.db.collection_cnt, i+ipl));

    // set cursor to first visible image if it wasn't set before:
    if(g_image_cursor == -2) g_image_cursor = i;

    uint32_t tid = vkdt.db.image[vkdt.db.collection[i]].thumbnail;
    if(tid == -1u) tid = 0; // busybee
    struct nk_color col = vkdt.style.colour[NK_COLOR_BUTTON];
    struct nk_color hov = vkdt.style.colour[NK_COLOR_BUTTON_HOVER];
    const int selected = vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected;
    if(selected) col = vkdt.style.colour[NK_COLOR_BUTTON_ACTIVE];
    // const int current  = vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db);
    // if(current) 
    // { // can't miss this due to all the decoration shown
    //   hov = vkdt.style.colour[NK_COLOR_BUTTON];
    //   col = vkdt.style.colour[NK_COLOR_BUTTON_HOVER];
    // }
    float scale = MIN(
        wd/(float)vkdt.thumbnails.thumb[tid].wd,
        ht/(float)vkdt.thumbnails.thumb[tid].ht);
    float w = vkdt.thumbnails.thumb[tid].wd * scale;
    float h = vkdt.thumbnails.thumb[tid].ht * scale;
    uint32_t ret = dt_thumbnail_image(
        &vkdt.ctx,
        vkdt.thumbnails.thumb[tid].dset,
        (struct nk_vec2){w, h},
        hov, col,
        vkdt.db.image[vkdt.db.collection[i]].rating,
        vkdt.db.image[vkdt.db.collection[i]].labels,
        (vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db)) ?
        vkdt.db.image[vkdt.db.collection[i]].filename : 0);

    if(g_image_cursor == i)
      nk_stroke_rect(nk_window_get_canvas(&vkdt.ctx), row, 0, 0.004*vkdt.state.center_ht, vkdt.style.colour[NK_COLOR_DT_ACCENT]);

    if(!vkdt.wstate.popup && ret)
    {
      int shift = glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_SHIFT)   == GLFW_PRESS || glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_SHIFT)   == GLFW_PRESS;
      int ctrl  = glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
      vkdt.wstate.busy += 2;
      if(ctrl)
      {
        if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
          dt_db_selection_remove(&vkdt.db, i);
        else
          dt_db_selection_add(&vkdt.db, i);
      }
      else if(shift)
      { // shift selects ranges
        uint32_t colid = dt_db_current_colid(&vkdt.db);
        if(colid != -1u)
        {
          int a = MIN(colid, (uint32_t)i);
          int b = MAX(colid, (uint32_t)i);
          dt_db_selection_clear(&vkdt.db);
          for(int i=a;i<=b;i++)
            dt_db_selection_add(&vkdt.db, i);
        }
      }
      else
      { // no modifier, select exactly this image:
        if(dt_db_selection_contains(&vkdt.db, i) &&
           nk_input_is_mouse_click_in_rect(&vkdt.ctx.input, NK_BUTTON_DOUBLE, row))
        {
          dt_view_switch(s_view_darkroom);
        }
        else
        {
          dt_db_selection_clear(&vkdt.db);
          dt_db_selection_add(&vkdt.db, i);
        }
      }
    }
  }
  nk_style_pop_vec2(&vkdt.ctx);

  // lt hotkeys in same scope as center window (scroll)
  switch(g_hotkey)
  {
    case s_hotkey_scroll_cur:
      g_hotkey = -1;
      break;
    case s_hotkey_scroll_end:
      vkdt.ctx.current->scrollbar.y = 0x7fffffffu;
      g_hotkey = -1;
      break;
    case s_hotkey_scroll_top:
      vkdt.ctx.current->scrollbar.y = 0u;
      g_hotkey = -1;
      break;
    default: break;
  }
  if(scroll_to >= 0) 
    vkdt.ctx.current->scrollbar.y = scroll_to;
  g_scroll_offset = - vkdt.ctx.current->scrollbar.y; // remember

  // draw context sensitive help overlay
  if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();

  NK_UPDATE_ACTIVE;
  nk_end(&vkdt.ctx); // lt center window
  nk_style_pop_style_item(&vkdt.ctx);
}


static inline void
render_lighttable_header()
{
  nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_hide());
  if(nk_begin(&vkdt.ctx, "lighttable header",
        nk_rect(0, 0, vkdt.state.center_wd, vkdt.state.center_y),
        NK_WINDOW_NO_SCROLLBAR))
  { // draw current collection description
    nk_layout_row_dynamic(&vkdt.ctx, 0.93*vkdt.state.center_y, 1);
    struct nk_rect bounds = nk_widget_bounds(&vkdt.ctx);
    nk_label(&vkdt.ctx, "", 0);
    nk_style_push_font(&vkdt.ctx, &dt_gui_get_font(2)->handle);
    recently_used_collections_draw(bounds, vkdt.db.dirname, &vkdt.db.collection_filter);
    nk_style_pop_font(&vkdt.ctx);
  }
  nk_end(&vkdt.ctx);
  nk_style_pop_style_item(&vkdt.ctx);
}



// export bg job stuff. put into api.hh?
typedef struct export_job_t
{ // this memory belongs to the export thread and will not change behind its back.
  uint32_t *sel;
  dt_token_t output_module;
  int wd, ht;
  float quality;
  int colour_prim, colour_trc;
  uint32_t cnt;
  uint32_t overwrite;
  uint32_t last_frame_only;
  char basename[1000];
  uint8_t *pdata;
  atomic_uint abort;
  atomic_uint state; // 0 idle, 1 started, 2 cleaned
  int taskid;
  dt_graph_t graph;
} export_job_t;
void export_job_cleanup(void *arg)
{ // task is done, every thread will call this (but we put only one)
  export_job_t *j = (export_job_t *)arg;
  free(j->sel);
  free(j->pdata);
  dt_graph_cleanup(&j->graph);
  j->state = 2;
}
void export_job_work(uint32_t item, void *arg)
{
  char filename[PATH_MAX], infilename[PATH_MAX], filedir[PATH_MAX];
  char dir[512];
  dt_graph_export_t param = {0};
  export_job_t *j = (export_job_t *)arg;
  if(j->abort) goto out;

  dt_db_image_path(&vkdt.db, j->sel[item], filedir, sizeof(filedir));
  fs_expand_export_filename(j->basename, sizeof(j->basename), filename, sizeof(filename), filedir, item);
  if(snprintf(dir, sizeof(dir), "%s", filename) >= (int)sizeof(dir))
  {
    dt_gui_notification("expanded filename too long!");
    goto out;
  }
  if(fs_dirname(dir)) fs_mkdir_p(dir, 0755);

  dt_gui_notification("exporting to %s", filename);

  dt_db_image_path(&vkdt.db, j->sel[item], infilename, sizeof(infilename));
  param.output_cnt = 1;
  param.output[0].p_filename = filename;
  param.output[0].max_width  = j->wd;
  param.output[0].max_height = j->ht;
  param.output[0].quality    = j->quality;
  param.output[0].mod        = j->output_module;
  param.output[0].p_pdata    = (char *)j->pdata;
  param.output[0].colour_primaries = j->colour_prim;
  param.output[0].colour_trc       = j->colour_trc;
  param.last_frame_only      = j->last_frame_only;
  param.p_cfgfile            = infilename;
  if(j->output_module == dt_token("o-web"))
  { // if module is o-web, also generate thumbnails at reduced size.
    param.output_cnt = 2;
    param.last_frame_only = 1; // avoid small thumbnail per frame in videos
    snprintf(filedir, sizeof(filedir), "%s-small", filename);
    param.output[1].p_filename = filedir;
    param.output[1].max_width  = 1024;
    param.output[1].max_height = 1024;
    param.output[1].quality    = j->quality;
    param.output[1].inst_out   = dt_token("small");
    param.output[1].mod        = dt_token("o-jpg");
    param.output[0].p_pdata    =
    param.output[1].p_pdata    = 0; // dangerous because the module doesn't match the ui. this will be mostly uninited memory.
    param.output[1].colour_primaries = j->colour_prim;
    param.output[1].colour_trc       = j->colour_trc;
  }
  if(dt_graph_export(&j->graph, &param))
    dt_gui_notification("export %s failed!\n", infilename);
out:
  dt_graph_reset(&j->graph);
  glfwPostEmptyEvent(); // redraw status bar
}
int export_job(
    export_job_t *j,
    dt_export_widget_t *w)
{
  j->abort = 0;
  j->state = 1;
  j->overwrite = w->overwrite;
  if(vkdt.db.selection_cnt <= 0)
  {
    dt_gui_notification("need to select at least one image to export!");
    return -1;
  }
  j->cnt = vkdt.db.selection_cnt;
  j->sel = (uint32_t *)malloc(sizeof(uint32_t)*j->cnt);
  memcpy(j->sel, dt_db_selection_get(&vkdt.db), sizeof(uint32_t)*j->cnt);
  snprintf(j->basename, sizeof(j->basename), "%.*s", (int)sizeof(j->basename)-1, w->basename);
  j->wd = w->wd;
  j->ht = w->ht;
  j->output_module = dt_pipe.module[w->modid[w->format]].name;
  j->quality = w->quality;
  j->colour_prim = w->colour_prim;
  j->colour_trc  = w->colour_trc;
  j->last_frame_only = w->last_frame_only;
  size_t psize = dt_module_total_param_size(w->modid[w->format]);
  j->pdata = (uint8_t *)malloc(sizeof(uint8_t)*psize);
  memcpy(j->pdata, w->pdata[w->format], psize);
  dt_graph_init(&j->graph, s_queue_compute);
  j->taskid = threads_task("export", j->cnt, -1, j, export_job_work, export_job_cleanup);
  return j->taskid;
}
// end export bg job stuff


void render_lighttable_right_panel()
{ // right panel
  struct nk_context *ctx = &vkdt.ctx;
  struct nk_rect bounds = {vkdt.win.width - vkdt.state.panel_wd, 0, vkdt.state.panel_wd, vkdt.win.height};
  const float ratio[] = {vkdt.state.panel_wd*0.6, vkdt.state.panel_wd*0.3};
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  const struct nk_vec2 size = {vkdt.state.panel_wd*0.95, vkdt.state.panel_wd*0.95};

  if(!nk_begin(ctx, "lighttable panel right", bounds, 0))
  {
    NK_UPDATE_ACTIVE;
    nk_end(ctx);
    return;
  }

  if(nk_tree_push(ctx, NK_TREE_TAB, "settings", NK_MINIMIZED))
  {
    nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
    nk_label(&vkdt.ctx, "vkdt version "VKDT_VERSION, NK_TEXT_LEFT);

    const float ratio[] = {0.7f, 0.3f};
    nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);

    if(nk_button_label(ctx, "hotkeys"))
      dt_gui_edit_hotkeys();
    nk_label(ctx, "", 0);

    float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
    float old_dpi_scale = dpi_scale;
    nk_tab_property(float, ctx, "#", 0.1, &dpi_scale, 10.0, 0.1, 0.05);
    dt_tooltip("scale the font size, and with it the whole gui");
    nk_label(ctx, "gui scale", NK_TEXT_LEFT);
    if(old_dpi_scale != dpi_scale)
    {
      dt_rc_set_float(&vkdt.rc, "gui/dpiscale", dpi_scale);
      dt_gui_recreate_swapchain(&vkdt.win);
      dt_gui_init_fonts();
    }

    nk_tab_property(int, ctx, "#", 2, &vkdt.wstate.lighttable_images_per_row, 16, 1, 1);
    dt_tooltip("how many images per row to display");
    nk_label(ctx, "images/row", NK_TEXT_LEFT);

    nk_tree_pop(ctx);
  }

  int update_collection = 0;
  if(nk_tree_push(ctx, NK_TREE_TAB, "collect", NK_MINIMIZED))
  {
    dt_db_filter_t *ft = &vkdt.db.collection_filter;
    int32_t sort_prop  =  vkdt.db.collection_sort;

    nk_layout_row(ctx, NK_STATIC, row_height, 2, ratio);

    int res = nk_combo_string(ctx, dt_db_property_text, sort_prop, 0xffff, row_height, size);
    if(res != sort_prop)
    {
      vkdt.db.collection_sort = sort_prop = res;
      update_collection = 1;
    }
    nk_label(&vkdt.ctx, "sort", NK_TEXT_LEFT);

    float ratio3[] = {ratio[0]*0.2, ratio[0]*0.8-ctx->style.combo.spacing.x, ratio[1]};
    nk_layout_row(ctx, NK_STATIC, row_height, 3, ratio3);
    int resi = ft->active & (1<<s_prop_rating) ? CLAMP(ft->rating_cmp, 0, 2) : 0;
    enum nk_symbol_type old0 = ctx->style.combo.sym_normal;
    enum nk_symbol_type old1 = ctx->style.combo.sym_hover;
    enum nk_symbol_type old2 = ctx->style.combo.sym_active;
    ctx->style.combo.sym_normal = ctx->style.combo.sym_hover = ctx->style.combo.sym_active = NK_SYMBOL_NONE;
    resi = nk_combo_string(ctx, ">=\0==\0<\0\0", resi, 0xffff, row_height, size);
    if(resi != ft->rating_cmp)
    {
      ft->rating_cmp = resi;
      update_collection = 1;
    }
    ctx->style.combo.sym_normal = old0;
    ctx->style.combo.sym_hover  = old1;
    ctx->style.combo.sym_active = old2;
    resi = ft->active & (1<<s_prop_rating) ? CLAMP(ft->rating, 0, 5) : 0;
    nk_style_push_font(ctx, &dt_gui_get_font(3)->handle);
    resi = nk_combo_string(ctx, "\ue836\0\ue838\0\ue838\ue838\0\ue838\ue838\ue838\0\ue838\ue838\ue838\ue838\0\ue838\ue838\ue838\ue838\ue838\0\0", resi, 0xffff, row_height, size);

    nk_style_pop_font(ctx);
    if(resi != ft->rating) 
    {
      if(resi == 0) ft->active &= ~(1<<s_prop_rating);
      else          ft->active |=   1<<s_prop_rating ;
      ft->rating = resi;
      update_collection = 1;
    }
    dt_tooltip("select the minimum star rating of images in the collection");
    nk_label(ctx, "rating", NK_TEXT_LEFT);

    const struct nk_color col[] = {
      {0xee, 0x11, 0x11, 0xff},
      {0x11, 0xee, 0x11, 0xff},
      {0x11, 0x11, 0xee, 0xff},
      {0xee, 0xee, 0x11, 0xff},
      {0xee, 0x11, 0xee, 0xff},
      {0x07, 0x07, 0x07, 0xff},
      {0x07, 0x07, 0x07, 0xff}};
    nk_layout_row_dynamic(ctx, row_height, 7);
    if(!(ft->active & (1<<s_prop_labels))) ft->labels = 0;
    for(int k=0;k<7;k++)
    {
      nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(col[k]));
      nk_style_push_float(ctx, &ctx->style.button.border, 1);

      int sel = ft->labels & (1<<k);
      if(sel) nk_style_push_float(ctx, &ctx->style.button.border, vkdt.wstate.fontsize*0.2);
      if(sel) nk_style_push_float(ctx, &ctx->style.button.rounding, row_height/2);
      else    nk_style_push_float(ctx, &ctx->style.button.rounding, 0);
      dt_tooltip(k==0?"red":k==1?"green":k==2?"blue":k==3?"yellow":k==4?"purple":k==5?"video":"bracket");
      if(nk_button_label(ctx, k==5 ? "m" : k==6 ? "[ ]" : " "))
      {
        ft->labels ^= (1<<k);
        update_collection = 1;
      }
      nk_style_pop_float(ctx);
      if(sel) nk_style_pop_float(ctx);
      nk_style_pop_float(ctx);
      nk_style_pop_style_item(ctx);
    }
    if(!ft->labels) ft->active &= ~(1<<s_prop_labels);
    else            ft->active |=   1<<s_prop_labels;


    nk_layout_row(ctx, NK_STATIC, row_height, 2, ratio);
    static int32_t filter_module_idx = 0;
    static const char *input_modules_str =
        "any\0"
        "raw : raw files\0"
        "jpg : jpg images\0"
        "exr : high dynamic range scene linear exr\0"
        "vid : compressed video\0"
        "mlv : magic lantern raw video\0"
        "mcraw: motioncam raw video\0\0";
    dt_token_t input_modules[] = {
      0,
      dt_token("i-raw"),
      dt_token("i-jpg"),
      dt_token("i-exr"),
      dt_token("i-vid"),
      dt_token("i-mlv"),
      dt_token("i-mcraw"),
    };
    dt_token_t filter_val = ft->filetype;
    if(!(ft->active & (1<<s_prop_filetype))) { filter_module_idx = 0; ft->filetype = 0; }
    else for(int k=0;k<NK_LEN(input_modules);k++) if(filter_val == input_modules[k]) { filter_module_idx = k; break; }
    res = nk_combo_string(ctx, input_modules_str, filter_module_idx, 0xffff, row_height, size);
    if(res != filter_module_idx)
    {
      filter_module_idx = res;
      ft->filetype = input_modules[res];
      update_collection = 1;
    }
    nk_label(ctx, "filetype", NK_TEXT_LEFT);
    if(!ft->filetype) ft->active &= ~(1<<s_prop_filetype);
    else              ft->active |=   1<<s_prop_filetype;

#if 0
    nk_layout_row(ctx, NK_STATIC, row_height, 2, ratio);
    int filename = (ft->active >> s_prop_filename)&1;
    res = nk_combo_string(ctx, "any\0filter by\0\0", filename, 0xffff, row_height, size);
    if(res != filename)
    {
      ft->active ^= 1<<s_prop_filename;
      update_collection = 1;
    }
    nk_label(ctx, "filename", NK_TEXT_LEFT);
    if(ft->active & (1<<s_prop_filename))
    {
      // TODO
    }
#endif
    nk_layout_row(ctx, NK_STATIC, row_height, 2, ratio);
    int createdate = (ft->active >> s_prop_createdate)&1;
    res = nk_combo_string(ctx, "any\0filter by\0\0", createdate, 0xffff, row_height, size);
    if(res != createdate)
    {
      ft->active ^= 1<<s_prop_createdate;
      update_collection = 1;
    }
    nk_label(ctx, "createdate", NK_TEXT_LEFT);
    if(ft->active & (1<<s_prop_createdate))
    {
      static char typed_filter_val[20] = "uninited";
      if(!strcmp(typed_filter_val, "uninited")) snprintf(typed_filter_val, sizeof(typed_filter_val), "%s", ft->createdate);
      dt_tooltip("substring to match in the createdate\nin YYYY:MM:DD HH:MM:SS form");
      nk_flags ret = nk_tab_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, typed_filter_val, sizeof(typed_filter_val), nk_filter_default);
      if(ret & NK_EDIT_COMMITED)
      {
        snprintf(ft->createdate, sizeof(ft->createdate), "%s", typed_filter_val);
        update_collection = 1;
      }
      static int cached_hash = 0;
      static int day_cnt = 0;
      static char day[31][11]; // okay if you have more than a month you should split it some other way first
      if(sort_prop == s_prop_createdate)
      {
        int hash = nk_murmur_hash(vkdt.db.dirname, (int)nk_strlen(vkdt.db.dirname), 0);
        if(hash != cached_hash)
        {
          dt_tooltip("create list of quick buttons for every day in current collection");
          if(nk_button_label(ctx, "by day"))
          {
            // reset collection filter first, so we can query all images in this folder.
            // we don't simply go through imageid because these aren't sorted by date
            typed_filter_val[0] = ft->createdate[0] = 0;
            uint64_t old_active = ft->active; // save
            ft->active = 0;
            dt_db_update_collection(&vkdt.db);
            day_cnt = 0;
            for(int i=0;i<vkdt.db.collection_cnt;i++)
            {
              if(day_cnt >= NK_LEN(day)) break;
              char createdate[20];
              dt_db_read_createdate(&vkdt.db, vkdt.db.collection[i], createdate);
              if(!day_cnt || strncmp(createdate, day[day_cnt-1], 10))
              {
                day[day_cnt][10] = 0;
                strncpy(day[day_cnt], createdate, 10);
                dt_sanitize_user_string(day[day_cnt]);
                day_cnt++;
              }
            }
            cached_hash = hash;
            ft->active = old_active; // restore
            dt_db_update_collection(&vkdt.db);
          }
        }
        else
        {
          nk_label(ctx, "", 0);
          nk_layout_row_dynamic(ctx, row_height, 7);
          for(int i=0;i<day_cnt;i++)
          {
            dt_tooltip(day[i]);
            if(nk_button_text(ctx, day[i]+8, 2))
            {
              snprintf(typed_filter_val, sizeof(typed_filter_val), "%s", day[i]);
              snprintf(ft->createdate, sizeof(ft->createdate), "%s", typed_filter_val);
              update_collection = 1;
            }
          }
        }
      }
    }

    nk_layout_row_dynamic(ctx, row_height, 1);
    dt_tooltip("open a different folder on your computer");
    if(nk_button_label(ctx, "open directory"))
      dt_view_switch(s_view_files);

    nk_tree_pop(ctx);
  }
  if(update_collection)
  {
    dt_db_update_collection(&vkdt.db);
    dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
    dt_gui_update_recently_used_collections();
  }

  if(nk_tree_push(ctx, NK_TREE_TAB, "tags", NK_MINIMIZED))
  {
    // ==============================================================
    // assign tag modal popup:
    if(vkdt.db.selection_cnt)
      if(nk_button_label(ctx, "assign tag.."))
        dt_gui_lt_assign_tag();

    // ==============================================================
    // recently used tags:
    nk_layout_row_dynamic(ctx, row_height, 3);
    char filename[PATH_MAX+100];
    for(int i=0;i<vkdt.tag_cnt;i++)
    {
      if(nk_button_label(ctx, vkdt.tag[i]))
      { // load tag collection:
        snprintf(filename, sizeof(filename), "%s/tags/%s", vkdt.db.basedir, vkdt.tag[i]);
        dt_gui_switch_collection(filename);
      }
    }
    // button to jump to original folder of selected image if it is a symlink
    uint32_t main_imgid = dt_db_current_imgid(&vkdt.db);
    if(main_imgid != -1u)
    {
      dt_db_image_path(&vkdt.db, main_imgid, filename, sizeof(filename));
      if(fs_islnk_file(filename) && nk_button_label(ctx, "jump to original collection"))
      {
        char *resolved = fs_realpath(filename, 0);
        if(resolved)
        {
          char *bn = fs_basename(resolved);
          size_t len = strlen(bn);
          if(len > 4) bn[len-4] = 0; // cut away .cfg
          fs_dirname(resolved);
          dt_gui_switch_collection(resolved);
          g_scroll_colid = dt_db_filename_colid(&vkdt.db, bn);
          free(resolved);
        }
      }
    }
    nk_tree_pop(ctx);
  } // end collapsing header "recent tags"

  if(nk_tree_push(ctx, NK_TREE_TAB, "recent collections", NK_MINIMIZED))
  { // recently used collections in ringbuffer:
    recently_used_collections();
    nk_tree_pop(ctx);
  } // end collapsing header "recent collections"

  if(vkdt.db.selection_cnt > 0 && nk_tree_push(ctx, NK_TREE_TAB, "selected images", NK_MINIMIZED))
  {
    // ==============================================================
    // copy/paste history stack
    nk_layout_row_dynamic(ctx, row_height, 2);

    if(nk_button_label(ctx, "copy history stack"))
      dt_gui_copy_history();

    if(vkdt.wstate.copied_imgid != -1u)
    {
      if(nk_button_label(ctx, "paste history stack"))
        dt_gui_paste_history();
    }
    else nk_label(ctx, "", 0);

    if(nk_button_label(ctx, "append preset"))
      dt_gui_preset_apply();
    nk_label(ctx, "", 0);

    // ==============================================================
    // delete images
    static int really_delete = 0;
    if(really_delete) { if(nk_button_label(ctx, "no, don't delete!")) really_delete = 0; }
    else
    {
      dt_tooltip("will ask you again");
      if(nk_button_label(ctx, "delete image[s]")) really_delete = 1;
    }

    if(really_delete)
    {
      dt_tooltip(
          "this button will physically delete the .cfg files of the selection.\n"
          "it will only delete the source image file if its filename is\n"
          "exacty the .cfg file name without the .cfg postfix.\n"
          "this means duplicates or tag collections will keep the source\n"
          "image file name on disk untouched, but only remove the duplicate\n"
          "or the tag from the image");
      if(nk_button_label(ctx, "*really* delete image[s]"))
      {
        dt_db_remove_selected_images(&vkdt.db, &vkdt.thumbnails, 1);
        really_delete = 0;
      }
    }
    else nk_label(ctx, "", 0);

    // ==============================================================
    // reset history stack
    if(nk_button_label(ctx, "reset history stack"))
    {
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      char filename[1024], realname[PATH_MAX];
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      {
        dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
        fs_realpath(filename, realname);
        unlink(realname);
        dt_thumbnails_invalidate(&vkdt.thumbnail_gen, filename);
      }
      dt_gui_label_unset(s_image_label_video);
      dt_gui_label_unset(s_image_label_bracket);
      dt_thumbnails_cache_list(
          &vkdt.thumbnail_gen,
          &vkdt.db,
          sel, vkdt.db.selection_cnt,
          &glfwPostEmptyEvent);
    }

    // ==============================================================
    // duplicate selected images
    if(nk_button_label(ctx, "duplicate"))
      dt_gui_lt_duplicate();

    if(vkdt.db.selection_cnt > 1)
    {
      // ==============================================================
      // create timelapse video
      dt_tooltip(
          "update the first image to become a timelapse video of the selected images.\n"
          "this assumes consecutive numbering of the image file names in the last four digits:\n"
          "for example IMG_0001.CR2..IMG_0020.CR2.\n"
          "does not work on tag collections or duplicates.");
      if(nk_button_label(ctx, "create video"))
      { // does not work on tag list images or duplicates.
        const uint32_t *sel = dt_db_selection_get(&vkdt.db);
        char filename[1024] = {0};
        dt_db_image_path(&vkdt.db, sel[0], filename, sizeof(filename));
        FILE *f = fopen(filename, "rb");
        int len = strlen(filename); // we'll work with the full file name, symlinks do not work
        if(len > 4) filename[len-4] = 0; // cut away ".cfg"
        dt_token_t input_module = dt_graph_default_input_module(filename);
        if(len > 4) filename[len-4] = '.'; // restore ".cfg"
        if(!f)
        {
          char defcfg[PATH_MAX+30];
          snprintf(defcfg, sizeof(defcfg), "%s/default-darkroom.%" PRItkn, dt_pipe.homedir, dt_token_str(input_module));
          if(fs_copy(filename, defcfg))
          { // no homedir defaults, go global:
            snprintf(defcfg, sizeof(defcfg), "%s/default-darkroom.%" PRItkn, dt_pipe.basedir, dt_token_str(input_module));
            fs_copy(filename, defcfg);
          }
          f = fopen(filename, "ab");
        }
        else
        {
          fclose(f);
          f = fopen(filename, "ab");
        }
        if(f && len > 11)
        { // cut away .cfg and directory part
          filename[len-4] = 0; len -=4;
          char *c = fs_basename(filename);
          int clen = strlen(c);
          for(int k=5;k>1;k--) if(c[clen-k] == '.') { clen -= k; break; }
          if(clen > 4)
          {
            int startid = atol(c+clen-4);
            memcpy(c+clen-4, "%04d", 4);
            fprintf(f, "param:%" PRItkn":main:filename:%s\n", dt_token_str(input_module), c);
            fprintf(f, "param:%" PRItkn":main:startid:%d\n", dt_token_str(input_module), startid);
            fprintf(f, "frames:%d\n", vkdt.db.selection_cnt);
            fprintf(f, "fps:24\n");
            fclose(f);
            filename[len-4] = '.';
            dt_thumbnails_invalidate(&vkdt.thumbnail_gen, filename);
            dt_thumbnails_cache_list(
                &vkdt.thumbnail_gen,
                &vkdt.db,
                sel, 1,
                &glfwPostEmptyEvent);
            // stupid, but can't add by imgid (or else would be able to select images that you can't see in the current collection)
            int colid = dt_db_filename_colid(&vkdt.db, vkdt.db.image[sel[0]].filename);
            dt_db_selection_clear(&vkdt.db);
            dt_db_selection_add(&vkdt.db, colid);
            dt_gui_label_set(s_image_label_video);
          }
        }
      }

      // ==============================================================
      // merge/align images
      dt_tooltip("align and stack selected images for low light photography. they have to be same exposure to work.");
      if(nk_button_label(ctx, "low light bracket"))
      { // overwrite .cfg for this image file:
        uint32_t main_imgid = dt_db_current_imgid(&vkdt.db);
        const uint32_t *sel = dt_db_selection_get(&vkdt.db);
        char filename[1024] = {0}, realname[PATH_MAX] = {0};
        dt_db_image_path(&vkdt.db, main_imgid, filename, sizeof(filename));
        FILE *f = fopen(filename, "wb");
        fprintf(f, "frames:1\n");
        fprintf(f, "module:i-raw:main\n");
        for(uint32_t i=1;i<vkdt.db.selection_cnt;i++)
        {
          fprintf(f, "module:i-raw:%02d\n", i);
          fprintf(f, "module:align:%02d\n", i);
          fprintf(f, "module:blend:%02d\n", i);
        }
        fprintf(f,
            "module:denoise:01\n"
            "module:hilite:01\n"
            "module:demosaic:01\n"
            "module:colour:01\n"
            "module:filmcurv:01\n"
            "module:hist:01\n"
            "module:display:hist\n"
            "module:display:main\n");
        fs_realpath(filename, realname);
        char *base = fs_basename(realname);
        base[strlen(base)-4] = 0;
        fprintf(f, "param:i-raw:main:filename:%s\n", base);
        int ii = 1;
        for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
        {
          if(sel[i] == main_imgid) continue;
          dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
          fs_realpath(filename, realname);
          char *base = fs_basename(realname);
          base[strlen(base)-4] = 0;
          fprintf(f, "param:i-raw:%02d:filename:%s\n", ii, base);
          fprintf(f,
              "connect:i-raw:%02d:output:align:%02d:alignsrc\n"
              "connect:i-raw:%02d:output:align:%02d:input\n"
              "connect:align:%02d:output:blend:%02d:input\n"
              "connect:align:%02d:mask:blend:%02d:mask\n",
              ii, ii, ii, ii, ii, ii, ii, ii);
          if(ii == 1) fprintf(f, "connect:i-raw:main:output:blend:%02d:back\n", ii);
          else        fprintf(f, "connect:blend:%02d:output:blend:%02d:back\n", ii-1, ii);
          fprintf(f, "connect:i-raw:main:output:align:%02d:aligndst\n", ii);
          fprintf(f,
              "param:blend:%02d:mode:0\n"
              "param:blend:%02d:mask:1\n"
              "param:blend:%02d:opacity:%g\n"
              "param:align:%02d:merge_n:0.05\n"
              "param:align:%02d:merge_k:30\n"
              "param:align:%02d:blur0:2\n"
              "param:align:%02d:blur1:16\n"
              "param:align:%02d:blur2:32\n"
              "param:align:%02d:blur3:64\n",
              ii, ii, ii, 1.0/(ii+1.0),
              ii, ii, ii, ii, ii, ii);
          ii++;
        }
        // TODO: grab from default darkroom cfg?
        fprintf(f,
            "connect:blend:%02d:output:denoise:01:input\n"
            "connect:denoise:01:output:hilite:01:input\n"
            "connect:hilite:01:output:demosaic:01:input\n"
            "connect:demosaic:01:output:colour:01:input\n"
            "connect:colour:01:output:filmcurv:01:input\n"
            "connect:filmcurv:01:output:display:main:input\n"
            "connect:filmcurv:01:output:hist:01:input\n"
            "connect:hist:01:output:display:hist:input\n", vkdt.db.selection_cnt-1);
        fclose(f);
        int colid = dt_db_filename_colid(&vkdt.db, vkdt.db.image[main_imgid].filename);
        dt_db_selection_clear(&vkdt.db);
        dt_db_selection_add(&vkdt.db, colid);
        dt_gui_label_set(s_image_label_bracket);
        // now redo/delete thumbnail of main_imgid
        dt_thumbnails_cache_list(
            &vkdt.thumbnail_gen,
            &vkdt.db,
            &main_imgid, 1,
            &glfwPostEmptyEvent);
      }
    } // end if multiple images are selected
    nk_tree_pop(ctx);
  } // end collapsing header "selected"

  if(vkdt.db.selection_cnt > 0 && nk_tree_push(ctx, NK_TREE_TAB, "metadata", NK_MINIMIZED))
  {
    nk_layout_row_static(ctx, row_height, vkdt.state.panel_wd, 1);
    static uint32_t imgid = -1u;
    static char text[2048], *text_end = text;
    if(imgid != vkdt.db.current_imgid)
    {
      text[0] = 0; text_end = text;
      const char *rccmd = dt_rc_get(&vkdt.rc, "gui/metadata/command", "/usr/bin/exiftool -l -createdate -aperture -shutterspeed -iso");
      dt_sanitize_user_string((char*)rccmd); // be sure nothing evil is in here. we won't change the length so we don't care about const.
      char cmd[PATH_MAX], imgpath[PATH_MAX];
      snprintf(cmd, sizeof(cmd), "%s '", rccmd);
      dt_db_image_path(&vkdt.db, vkdt.db.current_imgid, imgpath, sizeof(imgpath));
      fs_realpath(imgpath, cmd+strlen(cmd)); // use GNU extension: fill path even if it doesn't exist
      size_t len = strnlen(cmd, sizeof(cmd));
      if(len > 4)
      {
        cmd[len-4] = '\''; // cut away .cfg
        cmd[len-3] = 0;
        FILE *f = popen(cmd, "r");
        if(f)
        {
          len = fread(text, 1, sizeof(text), f);
          while(!feof(f) && !ferror(f)) fgetc(f); // drain empty
          text_end = text + len;
          text[len] = 0;
          imgid = vkdt.db.current_imgid;
          pclose(f);
        }
      }
    }
    dt_tooltip("customise what is shown here in config.rc");
    char *c = text;
    while(c < text_end)
    {
      char *cc = c;
      for(;*cc!='\n'&&*cc!=0&&cc<text_end;cc++) ;
      if(*cc=='\n') *cc = 0;
      nk_label(&vkdt.ctx, c, NK_TEXT_LEFT);
      c = cc+1;
    }
    nk_tree_pop(ctx);
  } // end collapsing header "metadata"

  // ==============================================================
  // export selection
  if(vkdt.db.selection_cnt > 0 && nk_tree_push(ctx, NK_TREE_TAB, "export selection", 
        g_hotkey == s_hotkey_export ? NK_FORCE_MAXIMIZED : NK_MINIMIZED))
  {
    static dt_export_widget_t w = {0};
    dt_export(&w);
    const float ratio[] = {0.7f, 0.3f};
    nk_layout_row(ctx, NK_DYNAMIC, 0, 2, ratio);
#define NUM_JOBS 4
    static export_job_t job[NUM_JOBS] = {{0}};
    int32_t num_idle = 0;
    for(int k=0;k<NUM_JOBS;k++)
    { // list of jobs to export stuff simultaneously
      if(job[k].state == 2) job[k].state = 0; // reset
      if(job[k].state == 0)
      { // idle job
        if(num_idle++)
        { // show at max one idle job
          break;
        }
        dt_tooltip("export current selection");
        if(g_hotkey == s_hotkey_export || nk_button_label(ctx, "export"))
        { // TODO: make sure we don't start a job that is already running in another job[.]
          export_job(job+k, &w);
          g_hotkey = -1;
        }
        nk_label(ctx, "", 0);
      }
      else if(job[k].cnt > 0 && threads_task_running(job[k].taskid))
      { // running
        struct nk_rect bb = nk_widget_bounds(ctx);
        float progress = threads_task_progress(job[k].taskid);
        nk_prog(ctx, 100*progress, 100, nk_false);
        char text[50];
        snprintf(text, sizeof(text), "%d%%", (int)(100.0*progress));
        nk_draw_text(nk_window_get_canvas(ctx), bb, text, strlen(text), &dt_gui_get_font(0)->handle, nk_rgba(0,0,0,0), nk_rgba(255,255,255,255));
        if(nk_button_label(ctx, "abort")) job[k].abort = 1;
        // technically a race condition on frame_cnt being inited by graph
        // loading during the async job. do we care?
        if(job[k].graph.frame_cnt > 1)
        {
          bb = nk_widget_bounds(ctx);
          nk_prog(ctx, job[k].graph.frame, job[k].graph.frame_cnt, nk_false);
          snprintf(text, sizeof(text), "frame %d/%d", job[k].graph.frame, job[k].graph.frame_cnt);
          nk_draw_text(nk_window_get_canvas(ctx), bb, text, strlen(text), &dt_gui_get_font(0)->handle, nk_rgba(0,0,0,0), nk_rgba(255,255,255,255));
          nk_label(ctx, "", 0);
        }
      }
    }
#undef NUM_JOBS
    nk_tree_pop(ctx);
  } // end collapsing header "export"
  NK_UPDATE_ACTIVE;
  nk_end(&vkdt.ctx); // lt right panel
}

void render_lighttable()
{
  render_lighttable_right_panel();
  render_lighttable_center();
  render_lighttable_header();

  // popup windows
  struct nk_rect bounds = { vkdt.state.center_x+0.2*vkdt.state.center_wd, vkdt.state.center_y+0.2*vkdt.state.center_ht,
    0.6*vkdt.state.center_wd, 0.6*vkdt.state.center_ht };
  if(vkdt.wstate.popup == s_popup_assign_tag)
  {
    if(nk_begin(&vkdt.ctx, "assign tag", bounds, NK_WINDOW_NO_SCROLLBAR))
    {
      static char filter[256] = "all time best";
      static char name[PATH_MAX];
      int ok = filteredlist(0, "%s/tags", filter, name, sizeof(name), s_filteredlist_allow_new | s_filteredlist_return_short);
      if(ok) vkdt.wstate.popup = 0; // got some answer
      nk_popup_end(&vkdt.ctx);
      if(ok == 1)
      {
        const uint32_t *sel = dt_db_selection_get(&vkdt.db);
        for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
          dt_db_add_to_collection(&vkdt.db, sel[i], name);
        dt_gui_read_tags();
      }
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
  else if(vkdt.wstate.popup == s_popup_edit_hotkeys)
  {
    if(nk_begin(&vkdt.ctx, "edit lighttable hotkeys", bounds, NK_WINDOW_NO_SCROLLBAR))
    {
      int ok = hk_edit(hk_lighttable, NK_LEN(hk_lighttable));
      if(ok) vkdt.wstate.popup = 0;
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
  else if(vkdt.wstate.popup == s_popup_apply_preset)
  {
    if(nk_begin(&vkdt.ctx, "apply preset", bounds, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
      char filename[1024] = {0};
      static char filter[256];
      int ok = filteredlist("%s/data/presets", "%s/presets", filter, filename, sizeof(filename), s_filteredlist_default);
      if(ok) vkdt.wstate.popup = 0;
      if(ok == 1)
      {
        FILE *fin = fopen(filename, "rb");
        if(!fin)
        { // also fails if src is 0
          dt_gui_notification("could not open preset %s!", filename);
        }
        else
        {
          fseek(fin, 0, SEEK_END);
          size_t fsize = ftell(fin);
          fseek(fin, 0, SEEK_SET);
          uint8_t *buf = (uint8_t*)malloc(fsize);
          fread(buf, fsize, 1, fin);
          fclose(fin);
          const uint32_t *sel = dt_db_selection_get(&vkdt.db);
          for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
          {
            dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
            char dst[PATH_MAX];
            fs_realpath(filename, dst);
            FILE *fout = fopen(dst, "ab");
            if(fout)
            {
              size_t pos = ftell(fout);
              if(pos == 0)
              { // no pre-existing cfg, copy defaults
                dt_token_t input_module = dt_graph_default_input_module(filename);
                char graph_cfg[PATH_MAX+100];
                snprintf(graph_cfg, sizeof(graph_cfg), "%s/default-darkroom.%"PRItkn, dt_pipe.homedir, dt_token_str(input_module));
                FILE *f = fopen(graph_cfg, "rb");
                if(!f)
                {
                  snprintf(graph_cfg, sizeof(graph_cfg), "%s/default-darkroom.%"PRItkn, dt_pipe.basedir, dt_token_str(input_module));
                  f = fopen(graph_cfg, "rb");
                }
                if(!f)
                {
                  dt_gui_notification("could not open default graph %s!", graph_cfg);
                }
                else
                {
                  fseek(f, 0, SEEK_END);
                  size_t fsize2 = ftell(f);
                  fseek(f, 0, SEEK_SET);
                  uint8_t *buf2 = (uint8_t*)malloc(fsize2);
                  fread(buf2, fsize2, 1, f);
                  fclose(f);
                  fwrite(buf2, fsize2, 1, fout);
                  free(buf2);
                  filename[strlen(filename)-4] = 0; // cut off .cfg
                  fprintf(fout, "param:%"PRItkn":main:filename:%s\n", dt_token_str(input_module), filename);
                }
              }
              // now append preset
              fwrite(buf, fsize, 1, fout);
              fclose(fout);
            }
          }
          dt_thumbnails_cache_list(
              &vkdt.thumbnail_gen,
              &vkdt.db,
              sel, vkdt.db.selection_cnt,
              &glfwPostEmptyEvent);
          free(buf);
        } // if fin
      } // end if ok == 1
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
}

void render_lighttable_init()
{
  vkdt.wstate.copied_imgid = -1u; // reset to invalid
  hk_deserialise("lighttable", hk_lighttable, NK_LEN(hk_lighttable));
}

void render_lighttable_cleanup()
{
  hk_serialise("lighttable", hk_lighttable, NK_LEN(hk_lighttable));
}

int lighttable_leave()
{
  g_image_cursor = -1;
  dt_gamepadhelp_clear();
  dt_rc_set_int(&vkdt.rc, "gui/images_per_line", vkdt.wstate.lighttable_images_per_row);
  return 0;
}

int lighttable_enter()
{
  uint32_t colid = dt_db_current_colid(&vkdt.db);
  g_scroll_colid = colid;
  g_scroll_offset = abs(g_scroll_offset);
  g_image_cursor = -1;
  if(vkdt.wstate.history_view)    dt_gui_dr_toggle_history();
  if(vkdt.wstate.fullscreen_view) dt_gui_dr_toggle_fullscreen_view();
  dt_gamepadhelp_set(dt_gamepadhelp_ps,              "toggle this help");
  // dt_gamepadhelp_set(dt_gamepadhelp_button_circle,   "back to files");
  dt_gamepadhelp_set(dt_gamepadhelp_button_cross,    "enter darkroom");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_up,        "move up");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_down,      "move down");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_left,      "move left");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_right,     "move right");
  vkdt.wstate.copied_imgid = -1u; // reset to invalid
  vkdt.wstate.lighttable_images_per_row = dt_rc_get_int(&vkdt.rc, "gui/images_per_line", 6);
  return 0;
}

void lighttable_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  g_image_cursor = -1;
}

void lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  g_image_cursor = -1;
}

void lighttable_gamepad(GLFWwindow *window, GLFWgamepadstate *last, GLFWgamepadstate *curr)
{
#define PRESSED(A) curr->buttons[A] && !last->buttons[A]
  if(PRESSED(GLFW_GAMEPAD_BUTTON_A))
  {
    if(g_image_cursor >= 0)
    {
      dt_db_selection_clear(&vkdt.db);
      dt_db_selection_add(&vkdt.db, g_image_cursor);
      dt_view_switch(s_view_darkroom);
    }
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_UP))
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else if(g_image_cursor >= vkdt.wstate.lighttable_images_per_row) g_image_cursor -= vkdt.wstate.lighttable_images_per_row;
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_DOWN))
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else if(g_image_cursor < vkdt.db.collection_cnt - vkdt.wstate.lighttable_images_per_row) g_image_cursor += vkdt.wstate.lighttable_images_per_row;
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_LEFT))
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else g_image_cursor = MAX(0, g_image_cursor-1);
  }
  else if(PRESSED(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT))
  {
    if(g_image_cursor < 0) g_image_cursor = -2;
    else g_image_cursor = MIN(vkdt.db.collection_cnt-1, g_image_cursor+1);
  }
#undef PRESSED
}
