// the imgui render functions for the lighttable view
extern "C"
{
#include "gui/view.h"
#include "gui/gui.h"
#include "db/thumbnails.h"
#include "db/rc.h"
#include "core/strexpand.h"
}
#include "gui/render_view.hh"
#include "gui/hotkey.hh"
#include "gui/widget_thumbnail.hh"
#include "gui/widget_recentcollect.hh"
#include "gui/api.hh"
#include "gui/render.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
};

void render_lighttable_center()
{ // center image view
  if(!dt_gui_imgui_input_blocked())
  { // global enter/exit key accels only if no popup is active
    if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
       ImGui::IsKeyPressed(ImGuiKey_Escape)||
       ImGui::IsKeyPressed(ImGuiKey_CapsLock))
    { dt_view_switch(s_view_files); return; }
    if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)||
       ImGui::IsKeyPressed(ImGuiKey_Enter))
      if(dt_db_current_imgid(&vkdt.db) != -1u)
      { dt_view_switch(s_view_darkroom); return; }
  }

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
  ImGui::SetNextWindowPos (ImVec2(vkdt.state.center_x,  vkdt.state.center_y),  ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.center_wd, vkdt.state.center_ht), ImGuiCond_Always);
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
  int hotkey = ImHotKey::GetHotKey(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
  switch(hotkey)
  {
    case 5:
      dt_gui_lt_scroll_current();
      break;
    case 6:
      dt_gui_lt_scroll_bottom();
      break;
    case 7:
      dt_gui_lt_scroll_top();
      break;
    case 8:
      dt_gui_lt_duplicate();
  }

  // draw context sensitive help overlay
  if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();

  ImGui::End(); // lt center window
}


// export bg job stuff. put into api.hh?
struct export_job_t
{
  uint32_t *sel;
  dt_token_t output_module;
  int wd, ht;
  float quality;
  uint32_t cnt;
  uint32_t overwrite;
  uint32_t abort;
  int taskid;
  char basename[1000];
  dt_graph_t graph;
};
void export_job_cleanup(void *arg)
{ // task is done, every thread will call this (but we put only one)
  export_job_t *j = (export_job_t *)arg;
  free(j->sel);
  dt_graph_cleanup(&j->graph);
}
void export_job_work(uint32_t item, void *arg)
{
  export_job_t *j = (export_job_t *)arg;
  if(j->abort) return;

  char dst[1000];
  time_t t = time(0);
  struct tm *tm = localtime(&t);
  char date[10] = {0}, yyyy[5] = {0};
  strftime(date, sizeof(date), "%Y%m%d", tm);
  strftime(yyyy, sizeof(yyyy), "%Y", tm);
  const char *key[] = { "home", "yyyy", "date", "seq", 0};
  const char *val[] = { getenv("HOME"), yyyy, date, "%04d", 0};
  dt_strexpand(j->basename, sizeof(j->basename), dst, sizeof(dst), key, val);

  char filename[PATH_MAX], infilename[PATH_MAX];
  snprintf(filename, sizeof(filename), dst, item);
  dt_gui_notification("exporting to %s", filename);

  dt_db_image_path(&vkdt.db, j->sel[item], infilename, sizeof(infilename));
  dt_graph_export_t param = {0};
  param.output_cnt = 1;
  param.output[0].p_filename = filename;
  param.output[0].max_width  = j->wd;
  param.output[0].max_height = j->ht;
  param.output[0].quality    = j->quality;
  param.output[0].mod        = j->output_module;
  param.p_cfgfile = infilename;
  // TODO: add a parameter to overwrite or not
  if(dt_graph_export(&j->graph, &param))
    dt_gui_notification("export %s failed!\n", infilename);
  dt_graph_reset(&j->graph);
  glfwPostEmptyEvent(); // redraw status bar
}
int export_job(
    export_job_t *j,
    int overwrite)
{
  j->abort = 0;
  j->overwrite = overwrite;
  if(vkdt.db.selection_cnt <= 0)
  {
    dt_gui_notification("need to select at least one image to export!");
    return -1;
  }
  j->cnt = vkdt.db.selection_cnt;
  j->sel = (uint32_t *)malloc(sizeof(uint32_t)*j->cnt);
  memcpy(j->sel, dt_db_selection_get(&vkdt.db), sizeof(uint32_t)*j->cnt);
  snprintf(j->basename, sizeof(j->basename), "%.*s", (int)sizeof(j->basename)-1, dt_rc_get(&vkdt.rc, "gui/export/basename", "/tmp/img_${seq}"));
  j->wd = dt_rc_get_int(&vkdt.rc, "gui/export/wd", 0);
  j->ht = dt_rc_get_int(&vkdt.rc, "gui/export/ht", 0);
  const dt_token_t format_mod[] = {dt_token("o-jpg"), dt_token("o-pfm"), dt_token("o-ffmpeg")};
  const int fm = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/export/format", 0),
          (int)0, (int)(sizeof(format_mod)/sizeof(format_mod[0])-1));
  j->output_module = format_mod[fm];
  j->quality = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90.0f);
  dt_graph_init(&j->graph);
  // TODO:
  // fs_mkdir(j->dst, 0777); // try and potentially fail to create destination directory
  j->taskid = threads_task(j->cnt, -1, j, export_job_work, export_job_cleanup);
  return j->taskid;
}
// end export bg job stuff


void render_lighttable_right_panel()
{ // right panel
  ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
  ImGui::Begin("panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

  float lineht = ImGui::GetTextLineHeight();
  float bwd = 0.5f;
  ImVec2 size(bwd*vkdt.state.panel_wd, 1.6*lineht);

  // lt hotkeys in same scope as buttons as modals (right panel)
  int hotkey = ImHotKey::GetHotKey(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
  switch(hotkey)
  {
    case 0: // assign tag
      dt_gui_lt_assign_tag();
      break;
    case 1: // toggle select all
      dt_gui_lt_toggle_select_all();
      break;
    case 2: // handled later
      break;
    case 3:
      dt_gui_lt_copy();
      break;
    case 4:
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
    }
    if(ImGui::Combo("filter", &filter_prop, dt_db_property_text))
    {
      vkdt.db.collection_filter = static_cast<dt_db_property_t>(filter_prop);
      dt_db_update_collection(&vkdt.db);
    }
    int filter_val = static_cast<int>(vkdt.db.collection_filter_val);
    if(filter_prop == s_prop_labels)
    {
      const ImVec4 col[5] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.2f, 0.2f, 0.8f, 1.0f),
        ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f),};
      for(int k=0;k<5;k++)
      {
        ImGui::PushID(k);
        ImGui::PushStyleColor(ImGuiCol_Button, col[k]);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.5, .1, .5, 1));
        int sel = filter_val & (1<<k);
        if(sel) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, vkdt.wstate.fontsize*0.2);
        if(ImGui::Button(" "))
        {
          filter_val ^= (1<<k);
          vkdt.db.collection_filter_val = static_cast<uint64_t>(filter_val);
          dt_db_update_collection(&vkdt.db);
        }
        if(k<4) ImGui::SameLine();
        ImGui::PopStyleColor(2);
        if(sel) ImGui::PopStyleVar();
        ImGui::PopID();
      }
    }
    else
    {
      if(ImGui::InputInt("filter value", &filter_val, 1, 100, 0))
      {
        vkdt.db.collection_filter_val = static_cast<uint64_t>(filter_val);
        dt_db_update_collection(&vkdt.db);
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
    char filename[PATH_MAX+100];
    for(int i=0;i<vkdt.tag_cnt;i++)
    {
      if(ImGui::Button(vkdt.tag[i], ImVec2(size.x*0.495, size.y)))
      { // load tag collection:
        snprintf(filename, sizeof(filename), "%s/tags/%s", vkdt.db.basedir, vkdt.tag[i]);
        dt_gui_switch_collection(filename);
      }
      if(((i & 3) != 3) && (i != vkdt.tag_cnt-1)) ImGui::SameLine();
    }
    // button to jump to original folder of selected image if it is a symlink
    uint32_t main_imgid = dt_db_current_imgid(&vkdt.db);
    if(main_imgid != -1u)
    {
      dt_db_image_path(&vkdt.db, main_imgid, filename, sizeof(filename));
      struct stat buf;
      lstat(filename, &buf);
      if(((buf.st_mode & S_IFMT)== S_IFLNK) && ImGui::Button("jump to original collection", size))
      {
        char *resolved = realpath(filename, 0);
        if(resolved)
        {
          fs_dirname(resolved);
          dt_gui_switch_collection(resolved);
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
    if(ImGui::Button("delete image[s]", size))
      really_delete ^= 1;
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("will ask you again");

    if(really_delete)
    {
      ImGui::SameLine();
      if(ImGui::Button("*really* delete image[s]", size))
      {
        dt_db_remove_selected_images(&vkdt.db, &vkdt.thumbnails, 1);
        really_delete = 0;
      }
      if(ImGui::IsItemHovered()) ImGui::SetTooltip(
          "this button will physically delete the .cfg files of the selection.\n"
          "it will only delete the source image file if its filename is\n"
          "exacty the .cfg file name without the .cfg postfix.\n"
          "this means duplicates or tag collections will keep the source\n"
          "image file name on disk untouched.");
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
        realpath(filename, realname);
        unlink(realname);
        dt_thumbnails_invalidate(&vkdt.thumbnail_gen, filename);
      }
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

    // ==============================================================
    // merge/align images
    if(vkdt.db.selection_cnt > 1)
    if(ImGui::Button("merge into current", size))
    {
      // overwrite .cfg for this image file:
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
      realpath(filename, realname);
      char *base = fs_basename(realname);
      base[strlen(base)-4] = 0;
      fprintf(f, "param:i-raw:main:filename:%s\n", base);
      int ii = 1;
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      {
        if(sel[i] == main_imgid) continue;
        dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
        realpath(filename, realname);
        char *base = fs_basename(realname);
        base[strlen(base)-4] = 0;
        fprintf(f, "param:i-raw:%02d:filename:%s\n", ii, base);
        fprintf(f,
            "connect:i-raw:%02d:output:align:%02d:alignsrc\n"
            "connect:i-raw:%02d:output:align:%02d:input\n"
            "connect:align:%02d:output:blend:%02d:back\n"
            "connect:align:%02d:mask:blend:%02d:mask\n",
            ii, ii, ii, ii, ii, ii, ii, ii);
        if(ii == 1) fprintf(f, "connect:i-raw:main:output:blend:%02d:input\n", ii);
        else        fprintf(f, "connect:blend:%02d:output:blend:%02d:input\n", ii-1, ii);
        fprintf(f, "connect:i-raw:main:output:align:%02d:aligndst\n", ii);
        fprintf(f,
            "param:blend:%02d:opacity:%g\n"
            "param:align:%02d:merge_n:0.05\n"
            "param:align:%02d:merge_k:30\n"
            "param:align:%02d:blur0:2\n"
            "param:align:%02d:blur1:16\n"
            "param:align:%02d:blur2:32\n"
            "param:align:%02d:blur3:64\n",
            ii, pow(0.5, ii),
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
      // now redo/delete thumbnail of main_imgid
      dt_thumbnails_cache_list(
          &vkdt.thumbnail_gen,
          &vkdt.db,
          &main_imgid, 1,
          &glfwPostEmptyEvent);
    }
    ImGui::Unindent();
  } // end collapsing header "selected"

  // ==============================================================
  // export selection
  if(vkdt.db.selection_cnt > 0 && ImGui::CollapsingHeader("export selection"))
  {
    ImGui::Indent();
    static int wd = dt_rc_get_int(&vkdt.rc, "gui/export/wd", 0);
    static int ht = dt_rc_get_int(&vkdt.rc, "gui/export/ht", 0);
    static int format = dt_rc_get_int(&vkdt.rc, "gui/export/format", 0);
    static float quality = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90);
    static char basename[240] = "";
    if(basename[0] == 0) strncpy(basename,
        dt_rc_get(&vkdt.rc, "gui/export/basename", "/tmp/img_${seq}"),
        sizeof(basename)-1);
    const char format_data[] = "jpg\0pfm\0ffmpeg\0\0";
    if(ImGui::InputInt("width", &wd, 1, 100, 0))
      dt_rc_set_int(&vkdt.rc, "gui/export/wd", wd);
    if(ImGui::InputInt("height", &ht, 1, 100, 0))
      dt_rc_set_int(&vkdt.rc, "gui/export/ht", ht);
    if(ImGui::InputText("filename", basename, sizeof(basename)))
      dt_rc_set(&vkdt.rc, "gui/export/basename", basename);
    if(ImGui::IsItemHovered()) ImGui::SetTooltip(
        "basename of exported files. the following will be replaced:\n"
        "${home} -- home directory\n"
        "${yyyy} -- current year\n"
        "${date} -- today's date\n"
        "${seq}  -- sequence number");
    if(ImGui::InputFloat("quality", &quality, 1, 100, 0))
      dt_rc_set_float(&vkdt.rc, "gui/export/quality", quality);
    if(ImGui::Combo("format", &format, format_data))
      dt_rc_set_int(&vkdt.rc, "gui/export/format", format);
    static export_job_t job[4] = {{0}};
    static int32_t overwrite_mode = 0;
    int32_t num_idle = 0;
    // TODO: this is not wired in the backend so hidden from gui for now too:
    // const char *overwrite_mode_str = "keep\0overwrite\0\0";
    // ImGui::Combo("existing files", &overwrite_mode, overwrite_mode_str);
    for(int k=0;k<4;k++)
    { // list of four jobs to copy stuff simultaneously
      if(job[k].cnt == 0)
      { // idle job
        if(num_idle++) break; // show at max one idle job
        if(hotkey == 2 || ImGui::Button("export"))
        { // TODO: make sure we don't start a job that is already running in another job[.]
          export_job(job+k, overwrite_mode);
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("export current selection");
      }
      else if(job[k].cnt > 0 && threads_task_running(job[k].taskid))
      { // running
        if(ImGui::Button("abort")) job[k].abort = 1;
        ImGui::SameLine();
        ImGui::ProgressBar(threads_task_progress(job[k].taskid), ImVec2(-1, 0));
      }
      else
      { // done/aborted
        if(ImGui::Button(job[k].abort ? "aborted" : "done"))
        { // reset
          memset(job+k, 0, sizeof(export_job_t));
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("click to reset");
      }
    }
    ImGui::Unindent();
  } // end collapsing header "export"

  dt_gui_lt_modals(); // draw modal windows from buttons/hotkeys

  ImGui::End(); // lt right panel
}

} // end anonymous namespace


void render_lighttable()
{
  render_lighttable_right_panel();
  render_lighttable_center();
}

void render_lighttable_init()
{
  ImHotKey::Deserialise("lighttable", hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
}

void render_lighttable_cleanup()
{
  ImHotKey::Serialise("lighttable", hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
}
