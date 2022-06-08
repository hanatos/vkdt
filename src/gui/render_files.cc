// imgui rendering for the files view
extern "C" {
#include "gui.h"
#include "view.h"
}
#include "gui/render_view.hh"
#include "widget_filebrowser.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace { // anonymous namespace

static dt_filebrowser_widget_t filebrowser = {{0}};

int find_usb_block_devices(
    char devname[20][20],
    int  mounted[20])
{ // pretty much ls /sys/class/scsi_disk/*/device/block/{sda,sdb,sdd,..}/{..sdd1..} and then grep for it in /proc/mounts
  int cnt = 0;
  char block[1000];
  struct dirent **ent, **ent2;
  int ent_cnt = scandir("/sys/class/scsi_disk/", &ent, 0, alphasort);
  if(ent_cnt < 0) return 0;
  for(int i=0;i<ent_cnt&&cnt<20;i++)
  {
    snprintf(block, sizeof(block), "/sys/class/scsi_disk/%s/device/block", ent[i]->d_name);
    int ent2_cnt = scandir(block, &ent2, 0, alphasort);
    if(ent2_cnt < 0) goto next;
    for(int j=0;j<ent2_cnt&&cnt<20;j++)
    {
      for(int k=1;k<10&&cnt<20;k++)
      {
        snprintf(block, sizeof(block), "/sys/class/scsi_disk/%s/device/block/%s/%.13s%d", ent[i]->d_name,
            ent2[j]->d_name, ent2[j]->d_name, k);
        struct stat sb;
        if(stat(block, &sb)) break;
        if(sb.st_mode & S_IFDIR)
          snprintf(devname[cnt++], sizeof(devname[0]), "/dev/%.13s%d", ent2[j]->d_name, k%10u);
      }
      free(ent2[j]);
    }
    free(ent2);
next:
    free(ent[i]);
  }
  free(ent);
  for(int i=0;i<cnt;i++) mounted[i] = 0;
  FILE *f = fopen("/proc/mounts", "r");
  if(f) while(!feof(f))
  {
    fscanf(f, "%999s %*[^\n]", block);
    for(int i=0;i<cnt;i++) if(!strcmp(block, devname[i])) mounted[i] = 1;
  }
  if(f) fclose(f);
  return cnt;
}

void set_cwd(const char *dir, int up)
{
  snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", dir);
  if(up)
  {
    char *c = filebrowser.cwd + strlen(filebrowser.cwd) - 1;
    for(;*c!='/'&&c>filebrowser.cwd;c--);
    if(c > filebrowser.cwd) strcpy(c, "/"); // truncate at last '/' to remove subdir
  }
  struct stat statbuf;
  int ret = stat(filebrowser.cwd, &statbuf);
  if(ret || (statbuf.st_mode & S_IFMT) != S_IFDIR) // don't point to non existing/non directory
    strcpy(filebrowser.cwd, "/");
  dt_filebrowser_cleanup(&filebrowser); // make it re-read cwd
}

} // end anonymous namespace

void render_files()
{
  static int just_entered = 1;
  if(just_entered)
  {
    const char *mru = dt_rc_get(&vkdt.rc, "gui/ruc_entry00", "null");
    if(strcmp(mru, "null")) set_cwd(mru, 1);
    just_entered = 0;
  }

  { // center window
    // TODO: wire double click/enter/(x) -> lighttable mode
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

  { // right panel
    ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::Begin("files panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    // if(ImGui::CollapsingHeader("settings")) ??
    if(ImGui::CollapsingHeader("folder"))
    {
      ImGui::Indent();
      ImGui::Text(filebrowser.cwd);
      if(ImGui::Button("open in lighttable"))
      {
        just_entered = 1; // remember for next time we get here
        dt_gui_switch_collection(filebrowser.cwd);
        dt_view_switch(s_view_lighttable);
      }
      if(ImGui::Button("back to lighttable"))
      {
        just_entered = 1; // remember for next time we get here
        dt_view_switch(s_view_lighttable);
      }
      // TODO: button to copy over a usb directory to some specific
      // TODO: "quit" button?
      ImGui::Unindent();
    }
    if(ImGui::CollapsingHeader("drives"))
    {
      ImGui::Indent();
      static int cnt = 0, mounted[20];
      static char devname[20][20] = {0};
      char mountpoint[1000];
      if(ImGui::Button("refresh list"))
        cnt = find_usb_block_devices(devname, mounted);
      for(int i=0;i<cnt;i++)
      {
        int red = mounted[i];
        if(red)
        {
          ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        }
        if(ImGui::Button(devname[i]))
        {
          if(mounted[i]) snprintf(mountpoint, sizeof(mountpoint), "/usr/bin/udisksctl unmount -b %s", devname[i]);
          else           snprintf(mountpoint, sizeof(mountpoint), "/usr/bin/udisksctl mount -b %s", devname[i]);
          FILE *f = popen(mountpoint, "r");
          if(f)
          {
            if(!mounted[i])
            {
              fscanf(f, "Mounted %*s at %999s", mountpoint);
              set_cwd(mountpoint, 0);
              mounted[i] = 1;
            }
            else mounted[i] = 0;
            if(pclose(f)) mounted[i] ^= 1; // did not work, roll back
          }
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip(mounted[i] ? "click to unmount" : "click to mount");
        if(red) ImGui::PopStyleColor(3);
      }
      ImGui::Unindent();
    }
    // if(ImGui::CollapsingHeader("recently used collections")) here too?
    // TODO: keyboard nav in filebrowser?
    // if(action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_CAPS_LOCK)) back to lt mode
    // TODO: show images/show only directories
    // TODO: feature to show only raw/whatever?
    ImGui::End();
  }
}
