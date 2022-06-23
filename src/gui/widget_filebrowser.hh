#pragma once

#include "gui/render.h"
#include <dirent.h>

// store some state
struct dt_filebrowser_widget_t
{
  char cwd[PATH_MAX+100];  // current working directory
  struct dirent **ent;     // cached directory entries
  int ent_cnt;             // number of cached directory entries
  const char *selected;    // points to selected file name ent->d_name
  int selected_type;       // copy of d_type
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
  for(int i=0;i<w->ent_cnt;i++)
    free(w->ent[i]);
  free(w->ent);
  w->ent_cnt = 0;
  w->ent = 0;
  w->selected = 0;
  w->selected_type = 0;
}

namespace {

int dt_filebrowser_sort_dir_first(const struct dirent **a, const struct dirent **b)
{
  if((*a)->d_type == DT_DIR && (*b)->d_type != DT_DIR) return 0;
  if((*a)->d_type != DT_DIR && (*b)->d_type == DT_DIR) return 1;
  return strcmp((*a)->d_name, (*b)->d_name);
}

int dt_filebrowser_filter_dir(const struct dirent *d)
{
  if(d->d_name[0] == '.' && d->d_name[1] != '.') return 0; // filter out hidden files
  if(d->d_type != DT_DIR) return 0; // filter out non-dirs too
  return 1;
}

int dt_filebrowser_filter_file(const struct dirent *d)
{
  if(d->d_name[0] == '.' && d->d_name[1] != '.') return 0; // filter out hidden files
  return 1;
}

}

inline void
dt_filebrowser(
    dt_filebrowser_widget_t *w,
    const char               mode) // 'f' or 'd'
{
  if(w->cwd[0] == 0) w->cwd[0] = '/';
  if(!w->ent)
  { // no cached entries, scan directory:
    w->ent_cnt = scandir(w->cwd, &w->ent,
        mode == 'd' ?
        &dt_filebrowser_filter_dir :
        &dt_filebrowser_filter_file,
        &dt_filebrowser_sort_dir_first);
    if(w->ent_cnt == -1)
    {
      w->ent = 0;
      w->ent_cnt = 0;
    }
  }

  // print cwd
  ImGui::PushFont(dt_gui_imgui_get_font(2));
  ImGui::Text("%s", w->cwd);
  ImGui::PopFont();
  // display list of file names
  ImGui::PushFont(dt_gui_imgui_get_font(1));
  for(int i=0;i<w->ent_cnt;i++)
  {
    char name[260];
    snprintf(name, sizeof(name), "%s %s",
        w->ent[i]->d_name,
        w->ent[i]->d_type == DT_DIR ? "/":"");
    int selected = w->ent[i]->d_name == w->selected;
    int select = ImGui::Selectable(name, selected, ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_DontClosePopups);
    select |= ImGui::IsItemFocused(); // has key/gamepad focus?
    if(select)
    {
      w->selected = w->ent[i]->d_name; // mark as selected
      w->selected_type = w->ent[i]->d_type;
      if((dt_gui_imgui_nav_input(ImGuiNavInput_Activate) > 0.0f ||
          ImGui::IsMouseDoubleClicked(0)) && 
          w->ent[i]->d_type == DT_DIR)
      { // directory double-clicked
        // change cwd by appending to the string
        int len = strnlen(w->cwd, sizeof(w->cwd));
        char *c = w->cwd;
        if(!strcmp(w->ent[i]->d_name, ".."))
        { // go up one dir
          c += len;
          *(--c) = 0;
          while(c > w->cwd && *c != '/') *(c--) = 0;
        }
        else
        { // append dir name
          snprintf(c+len, sizeof(w->cwd)-len-1, "%s/", w->ent[i]->d_name);
        }
        // and then clean up the dirent cache
        dt_filebrowser_cleanup(w);
      }
    }
  }
  ImGui::PopFont();
}
