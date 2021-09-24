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
