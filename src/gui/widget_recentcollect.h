#pragma once
#include "api_gui.h"
inline int
recently_used_collections()
{
  ImGui::PushID("ruc");
  int32_t num = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/ruc_num", 0), 0, 10);
  for(int i=0;i<num;i++)
  {
    char entry[512];
    snprintf(entry, sizeof(entry), "gui/ruc_entry%02d", i);
    const char *dir = dt_rc_get(&vkdt.rc, entry, "null");
    if(strcmp(dir, "null"))
    {
      const char *last = dir;
      for(const char *c=dir;*c!=0;c++) if(*c=='/') last = c+1;
      ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f,0.5f));
      if(ImGui::Button(last, ImVec2(-1, 0)))
      {
        dt_gui_switch_collection(dir);
        ImGui::PopStyleVar(1);
        ImGui::PopID();
        return 1; // return immediately since switching collections invalidates dir (by sorting/compacting the gui/ruc_num entries)
      }
      if(ImGui::IsItemHovered())
        dt_gui_set_tooltip("%s", dir);
      ImGui::PopStyleVar(1);
    }
  }
  ImGui::PopID();
  return 0;
}
