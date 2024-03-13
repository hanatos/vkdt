#pragma once

#include "gui/render.h"
#include "core/fs.h"
#include "core/sort.h"
#include "widget_filteredlist.h" // for tooltip
#include <dirent.h>

// store some state
struct dt_filebrowser_widget_t
{
  char cwd[PATH_MAX];      // current working directory
  struct dirent *ent;      // cached directory entries
  int ent_cnt;             // number of cached directory entries
  const char *selected;    // points to selected file name ent->d_name
  int selected_isdir;      // selected is a directory
};

// no need to explicitly call it, just sets 0
inline void
dt_filebrowser_init(
    dt_filebrowser_widget_t *w)
{
  memset(w, 0, sizeof(*w));
}

// this makes sure all memory is freed and also that the directory is re-read for new cwd.
inline void
dt_filebrowser_cleanup(
    dt_filebrowser_widget_t *w)
{
  free(w->ent);
  w->ent_cnt = 0;
  w->ent = 0;
  w->selected = 0;
  w->selected_isdir = 0;
}

namespace {

int dt_filebrowser_sort_dir_first(const void *aa, const void *bb, void *cw)
{
  const struct dirent *a = (const struct dirent *)aa;
  const struct dirent *b = (const struct dirent *)bb;
  const char *cwd = (const char *)cw;
  if( fs_isdir(cwd, a) && !fs_isdir(cwd, b)) return 0;
  if(!fs_isdir(cwd, a) &&  fs_isdir(cwd, b)) return 1;
  return strcmp(a->d_name, b->d_name);
}

int dt_filebrowser_filter_dir(const struct dirent *d, const char *cwd, const char *filter)
{
  if(d->d_name[0] == '.' && d->d_name[1] != '.') return 0; // filter out hidden files
  if(filter[0] && !strstr(d->d_name, filter)) return 0; // if filter required, apply it
  if(!fs_isdir(cwd, d)) return 0; // filter out non-dirs too
  return 1;
}

int dt_filebrowser_filter_file(const struct dirent *d, const char *cwd, const char *filter)
{
  if(d->d_name[0] == '.' && d->d_name[1] != '.') return 0; // filter out hidden files
  if(filter[0] && !strstr(d->d_name, filter)) return 0; // if filter required, apply it
  return 1;
}

}

inline void
dt_filebrowser(
    dt_filebrowser_widget_t *w,
    const char               mode) // 'f' or 'd'
{
  static char filter[100] = {0};
  static int setfocus = 0;
#ifdef _WIN64
  if(w->cwd[0] == 0) strcpy(w->cwd, dt_pipe.homedir);
#else
  if(w->cwd[0] == 0) w->cwd[0] = '/';
#endif
  if(!w->ent)
  { // no cached entries, scan directory:
    DIR* dirp = opendir(w->cwd);
    w->ent_cnt = 0;
    struct dirent *ent = 0;
    if(dirp)
    { // first count valid entries
      while((ent = readdir(dirp)))
        if((mode == 'd' && dt_filebrowser_filter_dir (ent, w->cwd, filter)) ||
           (mode != 'd' && dt_filebrowser_filter_file(ent, w->cwd, filter)))
           w->ent_cnt++;
      if(w->ent_cnt)
      {
        rewinddir(dirp); // second pass actually record stuff
        w->ent = (struct dirent *)malloc(sizeof(w->ent[0])*w->ent_cnt);
        w->ent_cnt = 0;
        while((ent = readdir(dirp)))
          if((mode == 'd' && dt_filebrowser_filter_dir (ent, w->cwd, filter)) ||
             (mode != 'd' && dt_filebrowser_filter_file(ent, w->cwd, filter)))
            w->ent[w->ent_cnt++] = *ent;
        sort(w->ent, w->ent_cnt, sizeof(w->ent[0]), dt_filebrowser_sort_dir_first, w->cwd);
      }
      closedir(dirp);
    }
    else
    {
      free(w->ent);
      w->ent = 0;
      w->ent_cnt = 0;
    }
  }

  // print cwd
  ImGui::PushFont(dt_gui_imgui_get_font(2));
  if(ImGui::InputText("##cwd", w->cwd, sizeof(w->cwd), ImGuiInputTextFlags_EnterReturnsTrue))
    if(ImGui::IsKeyDown(ImGuiKey_Enter))
    { dt_filebrowser_cleanup(w); setfocus = 1; }
  if(ImGui::IsItemHovered())
    dt_gui_set_tooltip("current working directory, edit to taste and press enter to change");
  ImGui::SameLine();
  if(ImGui::InputText("##filter", filter, sizeof(filter), ImGuiInputTextFlags_EnterReturnsTrue))
    if(ImGui::IsKeyDown(ImGuiKey_Enter))
    { dt_filebrowser_cleanup(w); setfocus = 1; }
  if(ImGui::IsItemHovered())
    dt_gui_set_tooltip("filter the displayed filenames. type a search string and press enter to apply");
  ImGui::PopFont();
  ImGui::BeginChild("scroll files");
  // display list of file names
  ImGui::PushFont(dt_gui_imgui_get_font(1));
  for(int i=0;i<w->ent_cnt;i++)
  {
    if(setfocus || (i == 0 && ImGui::IsWindowAppearing())) { ImGui::SetKeyboardFocusHere(); setfocus = 0; }
    char name[260];
    snprintf(name, sizeof(name), "%s %s",
        w->ent[i].d_name,
        fs_isdir(w->cwd, w->ent+i) ? "/":"");
    int selected = w->ent[i].d_name == w->selected;
    if(selected && ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    int select = ImGui::Selectable(name, selected, ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_DontClosePopups);
    select |= ImGui::IsItemFocused(); // has key/gamepad focus?
    if(select)
    {
      w->selected = w->ent[i].d_name; // mark as selected
      w->selected_isdir = fs_isdir(w->cwd, w->ent+i);
      if((ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown) ||
          ImGui::IsKeyPressed(ImGuiKey_Space) ||
          ImGui::IsMouseDoubleClicked(0)) && 
          fs_isdir(w->cwd, w->ent+i))
      { // directory double-clicked
        // change cwd by appending to the string
        int len = strnlen(w->cwd, sizeof(w->cwd));
        char *c = w->cwd;
        if(!strcmp(w->ent[i].d_name, ".."))
        { // go up one dir
          c += len;
          *(--c) = 0;
          while(c > w->cwd && (*c != '/' && *c != '\\')) *(c--) = 0;
        }
        else
        { // append dir name
          snprintf(c+len, sizeof(w->cwd)-len-1, "%s/", w->ent[i].d_name);
        }
        // and then clean up the dirent cache
        dt_filebrowser_cleanup(w);
      }
    }
  }
  if(!dt_gui_imgui_input_blocked() &&
     !dt_gui_imgui_want_text() &&
     ImGui::IsKeyPressed(ImGuiKey_Backspace))
  { // go up one dir
    int len = strnlen(w->cwd, sizeof(w->cwd));
    char *c = w->cwd + len;
    *(--c) = 0;
    while(c > w->cwd && *c != '/') *(c--) = 0;
    dt_filebrowser_cleanup(w);
  }
  ImGui::PopFont();
  ImGui::EndChild();
}
