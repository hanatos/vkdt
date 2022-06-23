#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// displays a filtered list of directory entries.
// this is useful to select from existing presets, blocks, tags, etc.
// call this between BeginPopupModal and EndPopup.
// return values != 0 mean you should call ImGui::CloseCurrentPopup().
static inline int            // return 0 - nothing, 1 - ok, 2 - cancel
filteredlist(
    const char *dir,         // optional. %s will be replaced as global basedir
    const char *dir_local,   // optional. if exists will be shown first. %s will be replaced by local basedir
    char        filter[256], // initial filter string (will be updated)
    char       *retstr,      // selection will be written here
    int         retstr_len,  // buffer size
    int         allow_new)   // if != 0 will display an optional 'new' button
  // TODO: custom filter rule?
{
  int ok = 0;
  int pick = -1, local = 0;
#define FREE_ENT do {\
  for(int i=0;i<ent_cnt;i++) free(ent[i]);\
  for(int i=0;i<ent_local_cnt;i++) free(ent_local[i]);\
  free(ent_local); ent_local = 0; ent_local_cnt = 0;\
  free(ent); ent = 0; ent_cnt = 0; } while(0)
  static struct dirent **ent = 0, **ent_local = 0;
  static int ent_cnt = 0, ent_local_cnt = 0;
  static char dirname[PATH_MAX+20];
  static char dirname_local[PATH_MAX+20];
  if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
  if(ImGui::InputText("##edit", filter, 256, ImGuiInputTextFlags_EnterReturnsTrue))
    ok = 1;
  if(ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "type to filter the list\n"
        "press enter to apply top item\n"
        "press escape to close");
  if(dt_gui_imgui_nav_input(ImGuiNavInput_Cancel) > 0.0f)
  { FREE_ENT; return 2; }

  if(!ent_cnt)
  { // open preset directory
    if(dir)
    {
      snprintf(dirname, sizeof(dirname), dir, dt_pipe.basedir);
      ent_cnt = scandir(dirname, &ent, 0, alphasort);
      if(ent_cnt == -1) ent_cnt = 0;
    }
    if(dir_local)
    {
      snprintf(dirname_local, sizeof(dirname_local), dir_local, vkdt.db.basedir);
      ent_local_cnt = scandir(dirname_local, &ent_local, 0, alphasort);
      if(ent_local_cnt == -1) ent_local_cnt = 0;
    }
    else ent_local_cnt = 0;
  }
#define LIST(E, L) do { \
  for(int i=0;i<E##_cnt;i++)\
  if(strstr(E[i]->d_name, filter) && E[i]->d_name[0] != '.') {\
    if(pick < 0) { local = L; pick = i; } \
    if(ImGui::Button(E[i]->d_name)) {\
      ok = 1; pick = i; local = L;\
    } } } while(0)
  LIST(ent_local, 1);
  LIST(ent, 0);
#undef LIST

  ImGuiStyle &style = ImGui::GetStyle();
  int bwd = 200; // TODO: dynamically scaled!
  int twd = style.ItemSpacing.x * (allow_new ? 2 : 1) + bwd * (allow_new ? 3 : 2);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - twd);
  if (allow_new && ImGui::Button("create new", ImVec2(bwd, 0))) { pick = -1; ok = 1; }
  if (allow_new) ImGui::SameLine();
  if (ImGui::Button("cancel", ImVec2(bwd, 0))) {FREE_ENT; return 2;}
  ImGui::SameLine();
  if (ImGui::Button("ok", ImVec2(bwd, 0))) ok = 1;

  if(ok == 1)
  {
    if(pick < 0)   snprintf(retstr, retstr_len, "%.*s", retstr_len-1, filter);
    else if(local) snprintf(retstr, retstr_len, "%.*s/%s", retstr_len-257, dirname_local, ent_local[pick]->d_name);
    else           snprintf(retstr, retstr_len, "%.*s/%s", retstr_len-257, dirname, ent[pick]->d_name);
    FREE_ENT;
  }
  return ok;
}
#undef FREE_ENT
