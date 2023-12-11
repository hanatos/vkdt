// the imgui render functions for the lighttable view
extern "C"
{
#include "gui/view.h"
#include "gui/gui.h"
#include "db/thumbnails.h"
#include "db/rc.h"
#include "db/hash.h"
#include "core/fs.h"
#include "pipe/graph-defaults.h"
}
#include "gui/render_view.hh"
#include "gui/hotkey.hh"
#include "gui/widget_thumbnail.hh"
#include "gui/widget_recentcollect.hh"
#include "gui/widget_export.hh"
#include "gui/api.hh"
#include "gui/render.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace { // anonymous namespace

static ImHotKey::HotKey hk_lighttable[] = {
  {"tag",           "assign a tag to selected images",  {ImGuiKey_LeftCtrl,  ImGuiKey_T}},
  {"select all",    "toggle select all/none",           {ImGuiKey_LeftCtrl,  ImGuiKey_A}},
  {"export",        "export selected images",           {ImGuiKey_LeftCtrl,  ImGuiKey_S}},
  {"copy",          "copy from selected image",         {ImGuiKey_LeftCtrl,  ImGuiKey_C}},
  {"paste history", "paste history to selected images", {ImGuiKey_LeftCtrl,  ImGuiKey_V}},
  {"scroll cur",    "scroll to current image",          {ImGuiKey_LeftShift, ImGuiKey_C}},
  {"scroll end",    "scroll to end of collection",      {ImGuiKey_LeftShift, ImGuiKey_G}},
  {"scroll top",    "scroll to top of collection",      {ImGuiKey_G}},
  {"duplicate",     "duplicate selected images",        {ImGuiKey_LeftShift, ImGuiKey_D}},
  {"rate 0",          "assign zero stars",                          {ImGuiKey_0}},
  {"rate 1",          "assign one star",                            {ImGuiKey_1}},
  {"rate 2",          "assign two stars",                           {ImGuiKey_2}},
  {"rate 3",          "assign three stars",                         {ImGuiKey_3}},
  {"rate 4",          "assign four stars",                          {ImGuiKey_4}},
  {"rate 5",          "assign five stars",                          {ImGuiKey_5}},
  {"label red",       "toggle red label",                           {ImGuiKey_F1}},
  {"label green",     "toggle green label",                         {ImGuiKey_F2}},
  {"label blue",      "toggle blue label",                          {ImGuiKey_F3}},
  {"label yellow",    "toggle yellow label",                        {ImGuiKey_F4}},
  {"label purple",    "toggle purple label",                        {ImGuiKey_F5}},
};
enum hotkey_names_t
{
  s_hotkey_assign_tag = 0,
  s_hotkey_select_all = 1,
  s_hotkey_export     = 2,
  s_hotkey_copy_hist  = 3,
  s_hotkey_paste_hist = 4,
  s_hotkey_scroll_cur = 5,
  s_hotkey_scroll_end = 6,
  s_hotkey_scroll_top = 7,
  s_hotkey_duplicate  = 8,
  s_hotkey_rate_0     = 9,
  s_hotkey_rate_1     = 10,
  s_hotkey_rate_2     = 11,
  s_hotkey_rate_3     = 12,
  s_hotkey_rate_4     = 13,
  s_hotkey_rate_5     = 14,
  s_hotkey_label_1    = 15,
  s_hotkey_label_2    = 16,
  s_hotkey_label_3    = 17,
  s_hotkey_label_4    = 18,
  s_hotkey_label_5    = 19,
};

void render_lighttable_center(int hotkey)
{ // center image view
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
  ImGuiStyle &style = ImGui::GetStyle();
  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_NoTitleBar;
  window_flags |= ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoResize;
  window_flags |= ImGuiWindowFlags_NoBackground;
  ImGui::SetNextWindowPos (ImVec2(
        ImGui::GetMainViewport()->Pos.x + vkdt.state.center_x,
        ImGui::GetMainViewport()->Pos.y + vkdt.state.center_y),  ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.center_wd, vkdt.state.center_ht), ImGuiCond_Always);
  ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
  ImGui::Begin("lighttable center", 0, window_flags);

  const int ipl = 6;
  const int border = 0.004 * qvk.win_width;
  const int wd = vkdt.state.center_wd / ipl - border*2 - style.ItemSpacing.x*2;
  const int ht = wd;
  const int cnt = vkdt.db.collection_cnt;
  const int lines = (cnt+ipl-1)/ipl;
  ImGuiListClipper clipper;
  clipper.Begin(lines, ht + 2*border + style.ItemSpacing.y);
  // a bit crude, but works: align thumbnail at pixel boundary:
  ImGui::GetCurrentWindow()->DC.CursorPos[0] = (int)ImGui::GetCurrentWindow()->DC.CursorPos[0];
  while(clipper.Step())
  {
    dt_thumbnails_load_list(
        &vkdt.thumbnails,
        &vkdt.db,
        vkdt.db.collection,
        MIN(clipper.DisplayStart * ipl, (int)vkdt.db.collection_cnt-1),
        MIN(clipper.DisplayEnd   * ipl, (int)vkdt.db.collection_cnt));
    for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;line++)
    {
      int i = line * ipl;
      for(int k=0;k<ipl;k++)
      {
        uint32_t tid = vkdt.db.image[vkdt.db.collection[i]].thumbnail;
        if(tid == -1u) tid = 0; // busybee
        if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
        {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0, 1.0, 1.0, 1.0));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
        }
        else if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
        {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0.6, 0.6, 1.0));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
        }
        float scale = MIN(
            wd/(float)vkdt.thumbnails.thumb[tid].wd,
            ht/(float)vkdt.thumbnails.thumb[tid].ht);
        float w = vkdt.thumbnails.thumb[tid].wd * scale;
        float h = vkdt.thumbnails.thumb[tid].ht * scale;
        uint32_t ret = ImGui::ThumbnailImage(
            vkdt.db.collection[i],
            vkdt.thumbnails.thumb[tid].dset,
            ImVec2(w, h),
            ImVec2(0,0), ImVec2(1,1),
            border,
            ImVec4(0.5f,0.5f,0.5f,1.0f),
            ImVec4(1.0f,1.0f,1.0f,1.0f),
            vkdt.db.image[vkdt.db.collection[i]].rating,
            vkdt.db.image[vkdt.db.collection[i]].labels,
            (vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db)) ?
            vkdt.db.image[vkdt.db.collection[i]].filename : 0,
            (vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db)) ?
            vkdt.wstate.set_nav_focus : 0);
        if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
          vkdt.wstate.set_nav_focus = MAX(0, vkdt.wstate.set_nav_focus-1);
        if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db) ||
          (vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected))
          ImGui::PopStyleColor(2);

        if(ret)
        {
          vkdt.wstate.busy += 2;
          if(ImGui::GetIO().KeyCtrl)
          {
            if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
              dt_db_selection_remove(&vkdt.db, i);
            else
              dt_db_selection_add(&vkdt.db, i);
          }
          else if(ImGui::GetIO().KeyShift)
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
            if(dt_db_selection_contains(&vkdt.db, i) ||
              (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)))
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

        if(++i >= cnt) break;
        if(k < ipl-1) ImGui::SameLine();
      }
    }
  }
  // lt hotkeys in same scope as center window (scroll)
  switch(hotkey)
  {
    case s_hotkey_scroll_cur:
      dt_gui_lt_scroll_current();
      break;
    case s_hotkey_scroll_end:
      dt_gui_lt_scroll_bottom();
      break;
    case s_hotkey_scroll_top:
      dt_gui_lt_scroll_top();
      break;
    case s_hotkey_duplicate:
      dt_gui_lt_duplicate();
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
    default: break;
  }
  dt_gui_lt_scroll_basename(0); // clear basename scrolling state and set if it was requested

  // draw context sensitive help overlay
  if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();

  ImGui::End(); // lt center window
}


// export bg job stuff. put into api.hh?
struct export_job_t
{ // this memory belongs to the export thread and will not change behind its back.
  uint32_t *sel;
  dt_token_t output_module;
  int wd, ht;
  float quality;
  uint32_t cnt;
  uint32_t overwrite;
  uint32_t last_frame_only;
  char basename[1000];
  uint8_t *pdata;
  uint32_t abort;
  int taskid;
  dt_graph_t graph;
};
void export_job_cleanup(void *arg)
{ // task is done, every thread will call this (but we put only one)
  export_job_t *j = (export_job_t *)arg;
  free(j->sel);
  free(j->pdata);
  dt_graph_cleanup(&j->graph);
}
void export_job_work(uint32_t item, void *arg)
{
  export_job_t *j = (export_job_t *)arg;
  if(j->abort) return;

  char filename[PATH_MAX], infilename[PATH_MAX], filedir[PATH_MAX];
  dt_db_image_path(&vkdt.db, j->sel[item], filedir, sizeof(filedir));
  fs_expand_export_filename(j->basename, sizeof(j->basename), filename, sizeof(filename), filedir, item);

  dt_gui_notification("exporting to %s", filename);

  dt_db_image_path(&vkdt.db, j->sel[item], infilename, sizeof(infilename));
  dt_graph_export_t param = {0};
  param.output_cnt = 1;
  param.output[0].p_filename = filename;
  param.output[0].max_width  = j->wd;
  param.output[0].max_height = j->ht;
  param.output[0].quality    = j->quality;
  param.output[0].mod        = j->output_module;
  param.output[0].p_pdata    = (char *)j->pdata;
  param.last_frame_only      = j->last_frame_only;
  param.p_cfgfile = infilename;
  if(dt_graph_export(&j->graph, &param))
    dt_gui_notification("export %s failed!\n", infilename);
  dt_graph_reset(&j->graph);
  glfwPostEmptyEvent(); // redraw status bar
}
int export_job(
    export_job_t *j,
    dt_export_widget_t *w)
{
  j->abort = 0;
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
  j->last_frame_only = w->last_frame_only;
  size_t psize = dt_module_total_param_size(w->modid[w->format]);
  j->pdata = (uint8_t *)malloc(sizeof(uint8_t)*psize);
  memcpy(j->pdata, w->pdata[w->format], psize);
  dt_graph_init(&j->graph);
  // TODO:
  // fs_mkdir(j->dst, 0777); // try and potentially fail to create destination directory
  j->taskid = threads_task("export", j->cnt, -1, j, export_job_work, export_job_cleanup);
  return j->taskid;
}
// end export bg job stuff


void render_lighttable_right_panel(int hotkey)
{ // right panel
  ImGui::SetNextWindowPos (ImVec2(
        ImGui::GetMainViewport()->Pos.x + qvk.win_width - vkdt.state.panel_wd,
        ImGui::GetMainViewport()->Pos.y + 0),   ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
  ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
  ImGui::Begin("panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

  float lineht = ImGui::GetTextLineHeight();
  float bwd = 0.47f;
  ImVec2 size(bwd*vkdt.state.panel_wd, 1.6*lineht);

  // lt hotkeys in same scope as buttons as modals (right panel)
  switch(hotkey)
  {
    case s_hotkey_assign_tag:
      dt_gui_lt_assign_tag();
      break;
    case s_hotkey_select_all:
      dt_gui_lt_toggle_select_all();
      break;
    case s_hotkey_export: // handled later
      break;
    case s_hotkey_copy_hist:
      dt_gui_lt_copy();
      break;
    case s_hotkey_paste_hist:
      dt_gui_lt_paste_history();
      break;
  }

  if(ImGui::CollapsingHeader("settings"))
  {
    ImGui::Indent();
    if(ImGui::Button("hotkeys"))
      ImGui::OpenPopup("edit hotkeys");
    ImHotKey::Edit(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]), "edit hotkeys");
    ImGui::Unindent();
  }

  if(ImGui::CollapsingHeader("collect"))
  {
    ImGui::Indent();
    int32_t filter_prop = static_cast<int32_t>(vkdt.db.collection_filter);
    int32_t sort_prop   = static_cast<int32_t>(vkdt.db.collection_sort);

    if(ImGui::Combo("sort", &sort_prop, dt_db_property_text))
    {
      vkdt.db.collection_sort = static_cast<dt_db_property_t>(sort_prop);
      dt_db_update_collection(&vkdt.db);
      dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
    }
    if(ImGui::Combo("filter", &filter_prop, dt_db_property_text))
    {
      vkdt.db.collection_filter = static_cast<dt_db_property_t>(filter_prop);
      dt_db_update_collection(&vkdt.db);
      dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
    }
    int filter_val = static_cast<int>(vkdt.db.collection_filter_val);
    if(filter_prop == s_prop_labels)
    {
      const ImVec4 col[] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.2f, 0.2f, 0.8f, 1.0f),
        ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f),
        ImVec4(0.1f, 0.1f, 0.1f, 1.0f),
        ImVec4(0.1f, 0.1f, 0.1f, 1.0f),};
      for(int k=0;k<7;k++)
      {
        ImGui::PushID(k);
        ImGui::PushStyleColor(ImGuiCol_Button, col[k]);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.5, .1, .5, 1));
        int sel = filter_val & (1<<k);
        if(sel) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, vkdt.wstate.fontsize*0.2);
        if(ImGui::Button( k==5 ? "m" : k==6 ? "[ ]" : " "))
        {
          filter_val ^= (1<<k);
          vkdt.db.collection_filter_val = static_cast<uint64_t>(filter_val);
          dt_db_update_collection(&vkdt.db);
          dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
        }
        if(ImGui::IsItemHovered())
          dt_gui_set_tooltip(k==0?"red":k==1?"green":k==2?"blue":k==3?"yellow":k==4?"purple":k==5?"video":"bracket");
        if(k<6) ImGui::SameLine();
        ImGui::PopStyleColor(2);
        if(sel) ImGui::PopStyleVar();
        ImGui::PopID();
      }
    }
    else if(filter_prop == s_prop_filetype)
    {
      char *filter_type = dt_token_str(vkdt.db.collection_filter_val);
      if(ImGui::InputText("filetype", filter_type+2, 6))
      {
        filter_type[0] = 'i'; filter_type[1] = '-';
        dt_db_update_collection(&vkdt.db);
        dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
      }
      if(ImGui::IsItemHovered())
        dt_gui_set_tooltip("enter the responsible input module here, for instance\n"
                          "raw : raw files\n"
                          "jpg : jpg files\n"
                          "vid : video files\n"
                          "mlv : raw video files");
    }
    else
    {
      if(ImGui::InputInt("filter value", &filter_val, 1, 100, 0))
      {
        vkdt.db.collection_filter_val = static_cast<uint64_t>(filter_val);
        dt_db_update_collection(&vkdt.db);
        dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
      }
    }

    if(ImGui::Button("open directory", size))
      dt_view_switch(s_view_files);

    ImGui::Unindent();
  }

  if(ImGui::CollapsingHeader("tags"))
  {
    ImGui::Indent();
    // ==============================================================
    // assign tag modal popup:
    if(vkdt.db.selection_cnt)
      if(ImGui::Button("assign tag..", size))
        dt_gui_lt_assign_tag();

    // ==============================================================
    // recently used tags:
    ImGui::PushID("tags");
    char filename[PATH_MAX+100];
    for(int i=0;i<vkdt.tag_cnt;i++)
    {
      if(ImGui::Button(vkdt.tag[i], ImVec2(size.x*0.49, size.y)))
      { // load tag collection:
        snprintf(filename, sizeof(filename), "%s/tags/%s", vkdt.db.basedir, vkdt.tag[i]);
        dt_gui_switch_collection(filename);
      }
      if(((i & 3) != 3) && (i != vkdt.tag_cnt-1)) ImGui::SameLine();
    }
    ImGui::PopID();
    // button to jump to original folder of selected image if it is a symlink
    uint32_t main_imgid = dt_db_current_imgid(&vkdt.db);
    if(main_imgid != -1u)
    {
      dt_db_image_path(&vkdt.db, main_imgid, filename, sizeof(filename));
      struct stat buf;
      lstat(filename, &buf);
      if(((buf.st_mode & S_IFMT)== S_IFLNK) && ImGui::Button("jump to original collection", ImVec2(-1, 0)))
      {
        char *resolved = fs_realpath(filename, 0);
        if(resolved)
        {
          char *bn = fs_basename(resolved);
          size_t len = strlen(bn);
          if(len > 4) bn[len-4] = 0; // cut away .cfg
          fs_dirname(resolved);
          dt_gui_switch_collection(resolved);
          dt_gui_lt_scroll_basename(bn);
          free(resolved);
        }
      }
    }
    ImGui::Unindent();
  } // end collapsing header "recent tags"

  if(ImGui::CollapsingHeader("recent collections"))
  { // recently used collections in ringbuffer:
    ImGui::Indent();
    recently_used_collections();
    ImGui::Unindent();
  } // end collapsing header "recent collections"

  if(vkdt.db.selection_cnt > 0 && ImGui::CollapsingHeader("selected images"))
  {
    ImGui::Indent();
    // ==============================================================
    // copy/paste history stack
    if(ImGui::Button("copy history stack", size))
      dt_gui_lt_copy();

    if(vkdt.wstate.copied_imgid != -1u)
    {
      ImGui::SameLine();
      if(ImGui::Button("paste history stack", size))
        dt_gui_lt_paste_history();
    }

    // ==============================================================
    // delete images
    static int really_delete = 0;
    if(really_delete) { if(ImGui::Button("no, don't delete!", size)) really_delete = 0; }
    else
    {
      if(ImGui::Button("delete image[s]", size)) really_delete = 1;
      if(ImGui::IsItemHovered()) dt_gui_set_tooltip("will ask you again");
    }

    if(really_delete)
    {
      ImGui::SameLine();
      if(ImGui::Button("*really* delete image[s]", size))
      {
        dt_db_remove_selected_images(&vkdt.db, &vkdt.thumbnails, 1);
        really_delete = 0;
      }
      if(ImGui::IsItemHovered()) dt_gui_set_tooltip(
          "this button will physically delete the .cfg files of the selection.\n"
          "it will only delete the source image file if its filename is\n"
          "exacty the .cfg file name without the .cfg postfix.\n"
          "this means duplicates or tag collections will keep the source\n"
          "image file name on disk untouched, but only remove the duplicate\n"
          "or the tag from the image");
    }

    // ==============================================================
    // reset history stack
    if(ImGui::Button("reset history stack", size))
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
    if(ImGui::Button("duplicate", size))
    {
      dt_gui_lt_duplicate();
    }

    if(vkdt.db.selection_cnt > 1)
    {
    // ==============================================================
    // create timelapse video
    if(ImGui::Button("create video", size))
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
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip(
        "update the first image to become a timelapse video of the selected images.\n"
        "this assumes consecutive numbering of the image file names in the last four digits:\n"
        "for example IMG_0001.CR2..IMG_0020.CR2.\n"
        "does not work on tag collections or duplicates.");

    // ==============================================================
    // merge/align images
    ImGui::SameLine();
    if(ImGui::Button("low light bracket", size))
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
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip(
        "align and stack selected images for low light photography. they have to be same exposure to work.");
    } // end if multiple images are selected
    ImGui::Unindent();
  } // end collapsing header "selected"

  if(vkdt.db.selection_cnt > 0 && ImGui::CollapsingHeader("metadata"))
  {
    ImGui::Indent();
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
          imgid = vkdt.db.current_imgid;
          pclose(f);
        }
      }
    }
    ImGui::TextUnformatted(text, text_end);
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip("customise what is shown here in config.rc");
    ImGui::Unindent();
  } // end collapsing header "metadata"

  // ==============================================================
  // export selection
  if(vkdt.db.selection_cnt > 0 && ImGui::CollapsingHeader("export selection"))
  {
    ImGui::Indent();
    static dt_export_widget_t w = {0};
    dt_export(&w);
    static export_job_t job[4] = {{0}};
    int32_t num_idle = 0;
    for(int k=0;k<4;k++)
    { // list of four jobs to copy stuff simultaneously
      ImGui::PushID(k);
      if(job[k].cnt == 0)
      { // idle job
        if(num_idle++)
        { // show at max one idle job
          ImGui::PopID();
          break;
        }
        if(hotkey == s_hotkey_export || ImGui::Button("export"))
        { // TODO: make sure we don't start a job that is already running in another job[.]
          export_job(job+k, &w);
        }
        if(ImGui::IsItemHovered()) dt_gui_set_tooltip("export current selection");
      }
      else if(job[k].cnt > 0 && threads_task_running(job[k].taskid))
      { // running
        if(ImGui::Button("abort")) job[k].abort = 1;
        ImGui::SameLine();
        ImGui::ProgressBar(threads_task_progress(job[k].taskid), ImVec2(-1, 0));
        if(job[k].graph.frame_cnt > 1)
          ImGui::ProgressBar(job[k].graph.frame / (float)job[k].graph.frame_cnt, ImVec2(-1, 0));
      }
      else
      { // done/aborted
        if(ImGui::Button(job[k].abort ? "aborted" : "done"))
        { // reset
          memset(job+k, 0, sizeof(export_job_t));
        }
        if(ImGui::IsItemHovered()) dt_gui_set_tooltip("click to reset");
      }
      ImGui::PopID();
    }
    ImGui::Unindent();
  } // end collapsing header "export"

  if(!dt_gui_imgui_input_blocked())
  { // global enter/exit key accels only if no popup is active
    if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
       ImGui::IsKeyPressed(ImGuiKey_Escape)||
       ImGui::IsKeyPressed(ImGuiKey_CapsLock))
      dt_view_switch(s_view_files);
    if(ImGui::IsKeyPressed(ImGuiKey_Enter))
      if(dt_db_current_imgid(&vkdt.db) != -1u)
        dt_view_switch(s_view_darkroom);
  }
  dt_gui_lt_modals(); // draw modal windows from buttons/hotkeys

  ImGui::End(); // lt right panel
}

} // end anonymous namespace


void render_lighttable()
{
  int hotkey = ImHotKey::GetHotKey(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
  render_lighttable_right_panel(hotkey);
  render_lighttable_center(hotkey);
}

void render_lighttable_init()
{
  vkdt.wstate.copied_imgid = -1u; // reset to invalid
  ImHotKey::Deserialise("lighttable", hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
}

void render_lighttable_cleanup()
{
  ImHotKey::Serialise("lighttable", hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
}
