#pragma once
#include "imgui.h"

// api functions for gui interactions. these can be
// triggered by pressing buttons or by issuing hotkey combinations.
// thus, they cannot have parameters, but they can read
// global state variables, as well as assume some imgui context.
// in particular, to realise modal dialogs, all modals are rendered
// in the dt_gui_*_modals() callback.

void
dt_gui_lt_modals()
{
  if(ImGui::BeginPopupModal("assign tag", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  {
    static char name[32] = "all time best";
    int ok = 0;
    if (g_busy >= 4)
      ImGui::SetKeyboardFocusHere();
    if(ImGui::InputText("##edit", name, IM_ARRAYSIZE(name), ImGuiInputTextFlags_EnterReturnsTrue))
    {
      ok = 1;
      ImGui::CloseCurrentPopup(); // accept
    }
    if(ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
      ImGui::CloseCurrentPopup(); // discard
    // ImGui::SetItemDefaultFocus();
    if (ImGui::Button("cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
    ImGui::SameLine();
    if (ImGui::Button("ok", ImVec2(120, 0)))
    {
      ok = 1;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
    if(ok)
    {
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      for(int i=0;i<vkdt.db.selection_cnt;i++)
        dt_db_add_to_collection(&vkdt.db, sel[i], name);
      dt_gui_read_tags();
    }
  }
}

void
dt_gui_lt_assign_tag()
{
  if(vkdt.db.selection_cnt <= 0)
  {
    dt_gui_notification("need to select at least one image to assign a tag!");
    return;
  }
  ImGui::OpenPopup("assign tag");
  g_busy += 5;
}

void
dt_gui_lt_toggle_select_all()
{
  if(vkdt.db.selection_cnt > 0) // select none
    dt_db_selection_clear(&vkdt.db);
  else // select all
    for(uint32_t i=0;i<vkdt.db.collection_cnt;i++)
      dt_db_selection_add(&vkdt.db, i);
}

void
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
          0, sizeof(format_mod)/sizeof(format_mod[0])-1);
  const float qy = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90.0f);
  char filename[256], infilename[256];
  dt_graph_t graph;
  dt_graph_init(&graph);
  for(int i=0;i<vkdt.db.selection_cnt;i++)
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
