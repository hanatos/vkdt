#pragma once
#include "imgui.h"
#include "widget_filteredlist.hh"
extern "C" {
#include "api.h"
#include "pipe/modules/api.h"
#include "pipe/graph-io.h"
#include "pipe/graph-export.h"
#include "pipe/graph-history.h"
}
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// api functions for gui interactions. these can be
// triggered by pressing buttons or by issuing hotkey combinations.
// thus, they cannot have parameters, but they can read
// global state variables, as well as assume some imgui context.
// in particular, to realise modal dialogs, all modals are rendered
// in the dt_gui_*_modals() callback.

inline void
dt_gui_lt_modals()
{
  if(ImGui::BeginPopupModal("assign tag", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  {
    static char filter[256] = "all time best";
    static char name[PATH_MAX];
    int ok = filteredlist(0, "%s/tags", filter, name, sizeof(name), 1);
    if(ok) ImGui::CloseCurrentPopup(); // got some answer
    ImGui::EndPopup();
    if(ok == 1)
    {
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
        dt_db_add_to_collection(&vkdt.db, sel[i], name);
      dt_gui_read_tags();
    }
  }
}

inline void
dt_gui_lt_assign_tag()
{
  if(vkdt.db.selection_cnt <= 0)
  {
    dt_gui_notification("need to select at least one image to assign a tag!");
    return;
  }
  ImGui::OpenPopup("assign tag");
  vkdt.wstate.busy += 5;
}

inline void
dt_gui_lt_toggle_select_all()
{
  if(vkdt.db.selection_cnt > 0) // select none
    dt_db_selection_clear(&vkdt.db);
  else // select all
    for(uint32_t i=0;i<vkdt.db.collection_cnt;i++)
      dt_db_selection_add(&vkdt.db, i);
}

inline void
dt_gui_lt_copy()
{
  vkdt.wstate.copied_imgid = dt_db_current_imgid(&vkdt.db);
}

inline void
dt_gui_lt_paste_history()
{
  if(vkdt.wstate.copied_imgid == -1u)
  {
    dt_gui_notification("need to copy first!");
    return;
  }
  // TODO: background job
  char filename[1024];
  uint32_t cid = vkdt.wstate.copied_imgid;
  dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
  FILE *fin = fopen(filename, "rb");
  if(!fin)
  {
    dt_gui_notification("could not open %s!", filename);
    return;
  }
  fseek(fin, 0, SEEK_END);
  size_t fsize = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  uint8_t *buf = (uint8_t*)malloc(fsize);
  fread(buf, fsize, 1, fin);
  fclose(fin);
  // this only works if the copied source is "simple", i.e. cfg corresponds to exactly
  // one input raw file that is appearing under param:i-raw:main:filename.
  // it then copies the history to the selected images, replacing their filenames in the config.
  const uint32_t *sel = dt_db_selection_get(&vkdt.db);
  for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
  {
    if(sel[i] == cid) continue; // don't copy to self
    dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
    FILE *fout = fopen(filename, "wb");
    if(fout)
    {
      fwrite(buf, fsize, 1, fout);
      // replace (relative) image file name
      const char *fn = vkdt.db.image[sel[i]].filename;
      size_t len = strlen(fn);
      if(len > 4 && !strncasecmp(fn+len-4, ".mlv", 4))
        fprintf(fout, "param:i-mlv:main:filename:%s\n", fn);
      else if(len > 4 && !strncasecmp(fn+len-4, ".pfm", 4))
        fprintf(fout, "param:i-pfm:main:filename:%s\n", fn);
      else if(len > 4 && !strncasecmp(fn+len-4, ".jpg", 4))
        fprintf(fout, "param:i-jpg:main:filename:%s\n", fn);
      else
        fprintf(fout, "param:i-raw:main:filename:%s\n", fn);
      fclose(fout);
    }
  }
  free(buf);
  dt_thumbnails_cache_list(
      &vkdt.thumbnail_gen,
      &vkdt.db,
      sel, vkdt.db.selection_cnt);
}

inline void
dt_gui_lt_export()
{
  if(vkdt.db.selection_cnt <= 0)
  {
    dt_gui_notification("need to select at least one image to export!");
    return;
  }
  const dt_token_t format_mod[] = {dt_token("o-jpg"), dt_token("o-pfm"), dt_token("o-ffmpeg")};
  // TODO: put in background job, implement job scheduler
  const uint32_t *sel = dt_db_selection_get(&vkdt.db);
  const char *basename = dt_rc_get(&vkdt.rc, "gui/export/basename", "/tmp/img");
  const int wd = dt_rc_get_int(&vkdt.rc, "gui/export/wd", 0);
  const int ht = dt_rc_get_int(&vkdt.rc, "gui/export/ht", 0);
  const int fm = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/export/format", 0),
          (int)0, (int)(sizeof(format_mod)/sizeof(format_mod[0])-1));
  const float qy = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90.0f);
  char filename[256], infilename[256];
  dt_graph_t graph;
  dt_graph_init(&graph);
  for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
  {
    snprintf(filename, sizeof(filename), "%s_%04d", basename, i);
    dt_gui_notification("exporting to %s", filename); // <== will actually not show unless in bg job
    dt_db_image_path(&vkdt.db, sel[i], infilename, sizeof(infilename));
    dt_graph_export_t param = {0};
    param.output_cnt = 1;
    param.output[0].p_filename = filename;
    param.output[0].max_width  = wd;
    param.output[0].max_height = ht;
    param.output[0].quality    = qy;
    param.output[0].mod        = format_mod[fm];
    param.p_cfgfile = infilename;
    if(dt_graph_export(&graph, &param))
      dt_gui_notification("export %s failed!\n", infilename);
    dt_graph_reset(&graph);
  }
  dt_graph_cleanup(&graph);
}

// scroll to top of collection
inline void
dt_gui_lt_scroll_top()
{
  ImGui::SetScrollY(0.0f);
}

// scroll to show current image
inline void
dt_gui_lt_scroll_current()
{
  uint32_t colid = dt_db_current_colid(&vkdt.db);
  if(colid == -1u) return;
  const int ipl = 6; // hardcoded images per line :(
  const float p = (colid/ipl)/(float)(vkdt.db.collection_cnt/ipl) * ImGui::GetScrollMaxY();
  ImGui::SetScrollY(p);
}

// scroll to end of collection
inline void
dt_gui_lt_scroll_bottom()
{
  ImGui::SetScrollY(ImGui::GetScrollMaxY());
}





// darkroom mode accessors
// XXX these modals should likely go into render.cc or something else!
// XXX they cannot be called from anywhere else and context still depends on them!
inline void
dt_gui_dr_modals()
{
  if(ImGui::BeginPopupModal("create preset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  {
    static char  preset[32] = "default";
    static char  filter[32] = "";
    static int   line_cnt = 0;
    static char *line[1024] = {0};
    static char  sel[1024];
    static char *buf = 0;
    static int   ok = 0;
    if(!buf)
    {
      ok = 0;
      char filename[1024] = {0};
      uint32_t cid = dt_db_current_imgid(&vkdt.db);
      if(cid != -1u) dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
      if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
        dt_graph_write_config_ascii(&vkdt.graph_dev, filename);
      FILE *f = fopen(filename, "rb");
      size_t s = 0;
      if(f)
      {
        fseek(f, 0, SEEK_END);
        s = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = (char *)malloc(s);
        fread(buf, s, 1, f);
        fclose(f);
      }
      line_cnt = 1;
      line[0] = buf;
      for(uint32_t i=0;i<s-1 && line_cnt < 1024;i++)
        if(buf[i] == '\n') { line[line_cnt++] = buf+i+1; buf[i] = 0; }
      if(buf[s-1] == '\n') buf[s-1] = 0;
    }
    
    if(ok == 0)
    {
      if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
      if(ImGui::InputText("##edit", filter, IM_ARRAYSIZE(filter), ImGuiInputTextFlags_EnterReturnsTrue))
        ok = 1;
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip("type to filter the list\n"
                          "press enter to accept\n"
                          "press escape to close");
      if(dt_gui_imgui_nav_input(ImGuiNavInput_Cancel) > 0.0f)
        ok = 3;

      for(int i=0;i<line_cnt;i++)
      {
        if(filter[0] == 0 || strstr(line[i], filter))
        {
          const int selected = sel[i];
          if(selected)
          {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8, 0.2, 0.1, 1.0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0, 0.4, 0.2, 1.0));
          }
          if(ImGui::Button(line[i])) sel[i] ^= 1;
          if(ImGui::IsItemHovered())
          {
            if(selected) ImGui::SetTooltip("click to drop from preset");
            else         ImGui::SetTooltip("click to include in preset");
          }
          if(selected) ImGui::PopStyleColor(2);
        }
      }

      if (ImGui::Button("cancel", ImVec2(120, 0))) ok = 3;
      ImGui::SameLine();
      if (ImGui::Button("ok", ImVec2(120, 0))) ok = 1;
    }
    else if(ok == 1)
    {
      ImGui::Text("enter preset name");
      if(ImGui::InputText("##preset", preset, IM_ARRAYSIZE(preset), ImGuiInputTextFlags_EnterReturnsTrue))
        ok = 2;
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip("presets will be stored as\n"
                          "~/.config/vkdt/presets/<this>.pst");
      if(ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
        ok = 3;
      if (ImGui::Button("cancel##2", ImVec2(120, 0))) ok = 3;
      ImGui::SameLine();
      if (ImGui::Button("ok##2", ImVec2(120, 0))) ok = 2;
    }

    if(ok == 2)
    {
      char filename[PATH_MAX+100];
      snprintf(filename, sizeof(filename), "%s/presets", vkdt.db.basedir);
      mkdir(filename, 0755);
      snprintf(filename, sizeof(filename), "%s/presets/%s.pst", vkdt.db.basedir, preset);
      FILE *f = fopen(filename, "wb");
      if(f)
      {
        for(int i=0;i<line_cnt;i++)
          if(sel[i]) fprintf(f, "%s\n", line[i]);
        fclose(f);
      }
      else dt_gui_notification("failed to write %s!", filename);
    }

    if(ok > 1)
    {
      free(buf);
      buf = 0;
      line_cnt = 0;
      ok = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if(ImGui::BeginPopupModal("apply preset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  {
    char filename[1024] = {0};
    uint32_t cid = dt_db_current_imgid(&vkdt.db);
    if(cid != -1u) dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
    if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
      dt_graph_write_config_ascii(&vkdt.graph_dev, filename);
    static char filter[256];
    int ok = filteredlist("%s/data/presets", "%s/presets", filter, filename, sizeof(filename), 0);
    if(ok) ImGui::CloseCurrentPopup();
    if(ok == 1)
    {
      FILE *f = fopen(filename, "rb");
      uint32_t lno = 0;
      if(f)
      {
        char line[300000];
        while(!feof(f))
        {
          fscanf(f, "%299999[^\n]", line);
          if(fgetc(f) == EOF) break; // read \n
          lno++;
          // > 0 are warnings, < 0 are fatal, 0 is success
          if(dt_graph_read_config_line(&vkdt.graph_dev, line) < 0) goto error;
          dt_graph_history_line(&vkdt.graph_dev, line);
        }
        fclose(f);
        vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
        darkroom_reset_zoom();
      }
      else
      {
error:
        if(f) fclose(f);
        dt_gui_notification("failed to read %s line %d", filename, lno);
      }
    } // end if ok == 1
    ImGui::EndPopup();
  } // end BeginPopupModal
}

inline void
dt_gui_dr_preset_create()
{
  ImGui::OpenPopup("create preset");
  vkdt.wstate.busy += 5;
}

inline void
dt_gui_dr_preset_apply()
{
  ImGui::OpenPopup("apply preset");
  vkdt.wstate.busy += 5;
}
