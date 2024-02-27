#pragma once
#include "core/sort.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// just wraps imgui but sets the viewport to the existing one.
// avoids swapchain recomputation and issues with tiling window managers.
inline void
dt_gui_set_tooltip(const char *fmt, ...)
{
  ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
  ImGui::PushTextWrapPos(vkdt.state.panel_wd);
  va_list args;
  va_start(args, fmt);
  ImGui::SetTooltipV(fmt, args);
  va_end(args);
}


// load a one-liner heading from the readme.md file in the directory
static inline char*
filteredlist_get_heading(
    const char *basedir,
    const char *dirname)
{
  char fn[256], *res = 0;
  size_t r = snprintf(fn, sizeof(fn), "%s/%s/readme.md", basedir, dirname);
  if(r >= sizeof(fn)) return 0; // truncated
  FILE *f = fopen(fn, "rb");
  if(f)
  {
    res = (char*)calloc(256, 1);
    r = snprintf(res, 10, "%s", dirname);
    if(r >= 10) res[10] = 0; // whatever, just to silence gcc
    fscanf(f, "# %255[^\n]", res);
    fclose(f);
  }
  return res;
}

enum filteredlist_flags_t
{
  s_filteredlist_default      = 0,
  s_filteredlist_allow_new    = 1, // provide "new" button
  s_filteredlist_descr_opt    = 2, // descriptions in dir/readme.md are optionally read and shown
  s_filteredlist_descr_req    = 4, // descriptions are mandatory, else the entry is filtered out
  s_filteredlist_descr_any    = 6, // both the above (for testing only, don't pass this)
  s_filteredlist_return_short = 8, // return only the short filename, not the absolute one
};

namespace {
int dt_filteredlist_compare(const void *aa, const void *bb, void *buf)
{
  const struct dirent *a = (const struct dirent *)aa;
  const struct dirent *b = (const struct dirent *)bb;
  return strcmp(a->d_name, b->d_name);
}
}

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
    filteredlist_flags_t flags)
  // TODO: custom filter rule?
{
  int ok = 0;
  int pick = -1, local = 0;
#define FREE_ENT do {\
  if(desc)       for(int i=0;i<ent_cnt;i++) free(desc[i]);\
  if(desc_local) for(int i=0;i<ent_cnt;i++) free(desc_local[i]);\
  free(desc); free(desc_local); \
  desc = desc_local = 0; \
  free(ent_local); ent_local = 0; ent_local_cnt = 0;\
  free(ent); ent = 0; ent_cnt = 0; } while(0)
  static struct dirent *ent = 0, *ent_local = 0;
  static int ent_cnt = 0, ent_local_cnt = 0;
  static char dirname[PATH_MAX];
  static char dirname_local[PATH_MAX];
  static char **desc = 0, **desc_local = 0;
  if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
  if(ImGui::InputText("filter", filter, 256, ImGuiInputTextFlags_EnterReturnsTrue))
    ok = 1;
  if(ImGui::IsItemHovered())
    dt_gui_set_tooltip(
        "type to filter the list\n"
        "press enter to apply top item\n"
        "press escape to close");
  if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
     ImGui::IsKeyPressed(ImGuiKey_Escape)||
     ImGui::IsKeyPressed(ImGuiKey_CapsLock))
  { FREE_ENT; return 2; }

  if(!ent_cnt)
  { // open directory
    if(dir)
    {
      snprintf(dirname, sizeof(dirname), dir, dt_pipe.basedir);
      DIR* dirp = opendir(dirname);
      ent_cnt = 0;
      struct dirent *it = 0;
      if(dirp)
      { // first count valid entries
        while((it = readdir(dirp)))
          ent_cnt++;
        if(ent_cnt)
        {
          rewinddir(dirp); // second pass actually record stuff
          ent = (struct dirent *)malloc(sizeof(ent[0])*ent_cnt);
          ent_cnt = 0;
          while((it = readdir(dirp)))
            ent[ent_cnt++] = *it;
        }
        sort(ent, ent_cnt, sizeof(ent[0]), dt_filteredlist_compare, 0);
        closedir(dirp);
      }
      if(ent_cnt && (flags & s_filteredlist_descr_any))
      {
        desc = (char**)malloc(sizeof(char*)*ent_cnt);
        for(int i=0;i<ent_cnt;i++)
          desc[i] = filteredlist_get_heading(dirname, ent[i].d_name);
      }
    }
    if(dir_local)
    {
      snprintf(dirname_local, sizeof(dirname_local), dir_local, vkdt.db.basedir);
      DIR* dirp = opendir(dirname_local);
      ent_local_cnt = 0;
      struct dirent *it = 0;
      if(dirp)
      { // first count valid entries
        while((it = readdir(dirp)))
          ent_local_cnt++;
        if(ent_local_cnt)
        {
          rewinddir(dirp); // second pass actually record stuff
          ent_local = (struct dirent *)malloc(sizeof(ent_local[0])*ent_local_cnt);
          ent_local_cnt = 0;
          while((it = readdir(dirp)))
            ent_local[ent_local_cnt++] = *it;
        }
        sort(ent, ent_cnt, sizeof(ent[0]), dt_filteredlist_compare, 0);
        closedir(dirp);
      }
      if(ent_local_cnt && (flags & s_filteredlist_descr_any))
      {
        desc_local = (char**)malloc(sizeof(char*)*ent_local_cnt);
        for(int i=0;i<ent_local_cnt;i++)
          desc_local[i] = filteredlist_get_heading(dirname_local, ent_local[i].d_name);
      }
    }
    else ent_local_cnt = 0;
  }


  ImGui::BeginChild("filteredlist-scrollpane", ImVec2(0.0f, 0.75f*vkdt.state.center_ht));
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
#define LIST(E, D, L) do { \
  for(int i=0;i<E##_cnt;i++)\
  if((strstr(E[i].d_name, filter) || (D && D[i] && strstr(D[i], filter)))\
      && E[i].d_name[0] != '.' && (!(flags & s_filteredlist_descr_req) || (D && D[i]))) {\
    if(pick < 0) { local = L; pick = i; } \
    if(ImGui::Button((D && D[i]) ? D[i] : E[i].d_name, ImVec2(-1, 0))) {\
      ok = 1; pick = i; local = L;\
    } } } while(0)
  LIST(ent_local, desc_local, 1);
  LIST(ent, desc, 0);
#undef LIST
  ImGui::PopStyleVar();
  ImGui::EndChild(); // scrollable list

  int bwd = vkdt.state.panel_wd / ((flags & s_filteredlist_allow_new) ? 3 : 2);
  ImGui::Dummy(ImVec2(0, 0.05f*vkdt.state.panel_wd));
  ImGui::Dummy(ImVec2(0.8f*vkdt.state.center_wd-2.0f*bwd, 0)); ImGui::SameLine();
  if((flags & s_filteredlist_allow_new) && ImGui::Button("create new", ImVec2(bwd, 0))) { pick = -1; ok = 1; }
  if (flags & s_filteredlist_allow_new) ImGui::SameLine();
  if (ImGui::Button("cancel", ImVec2(bwd, 0))) {FREE_ENT; return 2;}
  ImGui::SameLine();
  if (ImGui::Button("ok", ImVec2(bwd, 0))) ok = 1;

  if(ok == 1)
  {
    if(flags & s_filteredlist_return_short)
    {
      if(pick < 0)   snprintf(retstr, retstr_len, "%.*s", retstr_len-1, filter);
      else if(local) snprintf(retstr, retstr_len, "%.*s", retstr_len-1, ent_local[pick].d_name);
      else           snprintf(retstr, retstr_len, "%.*s", retstr_len-1, ent[pick].d_name);
    }
    else
    {
      if(pick < 0)   snprintf(retstr, retstr_len, "%.*s", retstr_len-1, filter);
      else if(local) snprintf(retstr, retstr_len, "%.*s/%s", retstr_len-257, dirname_local, ent_local[pick].d_name);
      else           snprintf(retstr, retstr_len, "%.*s/%s", retstr_len-257, dirname, ent[pick].d_name);
    }
    FREE_ENT;
  }
  return ok;
}
#undef FREE_ENT
