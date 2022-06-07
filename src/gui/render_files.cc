// imgui rendering for the files view
extern "C" {
#include "gui.h"
}
#include "gui/render_view.hh"
#include "widget_filebrowser.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void render_files()
{
  static int inited = 0;
  static dt_filebrowser_widget_t filebrowser = {{0}};
  if(!inited)
  {
    const char *mru = dt_rc_get(&vkdt.rc, "gui/ruc_entry00", "null");
    if(strcmp(mru, "null"))
    {
      snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", mru);
      char *c = filebrowser.cwd + strlen(filebrowser.cwd) - 1;
      for(;*c!='/'&&c>filebrowser.cwd;c--);
      if(c > filebrowser.cwd) strcpy(c, "/"); // truncate at last '/' to remove subdir
      struct stat statbuf;
      int ret = stat(filebrowser.cwd, &statbuf);
      if(ret || (statbuf.st_mode & S_IFMT) != S_IFDIR) // don't point to non existing/non directory
        strcpy(filebrowser.cwd, "/");
      inited = 1;
    }
  }

  { // center window
    // TODO: wire double click/enter/(x) -> lighttable mode and from there and esc <-
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    ImVec2 border = ImVec2(2*win_x, 2*win_y);
    ImGui::SetNextWindowPos (ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+border.x, win_h+border.y), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("files center", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    dt_filebrowser(&filebrowser, 'd');
    ImGui::End();
    ImGui::PopStyleColor(1);
  } // end center window

  { // TODO: right panel
    ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::Begin("files panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    // TODO: enter lt button
    // TODO: show images/show only directories
    // TODO: feature to show only
    // TODO: gio mount
    ImGui::End();
  }
}
