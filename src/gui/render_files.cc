// imgui rendering for the files view
extern "C" {
#include "gui/gui.h"
#include "gui/view.h"
#include "core/fs.h"
}
#include "gui/render_view.hh"
#include "gui/widget_filebrowser.hh"
#include "gui/widget_recentcollect.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace { // anonymous namespace

static dt_filebrowser_widget_t filebrowser = {{0}};

void set_cwd(const char *dir, int up)
{
  if(dir != filebrowser.cwd) snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", dir);
  if(up)
  {
    char *c = filebrowser.cwd + strlen(filebrowser.cwd) - 2; // ignore '/' as last character
    for(;*c!='/'&&c>filebrowser.cwd;c--);
    if(c > filebrowser.cwd) strcpy(c, "/"); // truncate at last '/' to remove subdir
  }
  struct stat statbuf;
  int ret = stat(filebrowser.cwd, &statbuf);
  if(ret || (statbuf.st_mode & S_IFMT) != S_IFDIR) // don't point to non existing/non directory
    strcpy(filebrowser.cwd, "/");
  size_t len = strlen(filebrowser.cwd);
  if(filebrowser.cwd[len-1] != '/') strcpy(filebrowser.cwd + len, "/");
  dt_filebrowser_cleanup(&filebrowser); // make it re-read cwd
}

struct copy_job_t
{ // copy contents of a folder
  char src[1000], dst[1000];
  struct dirent **ent;
  uint32_t cnt;
  uint32_t move;  // set to non-zero to remove src after copy
  uint32_t abort;
  int taskid;
};
void copy_job_cleanup(void *arg)
{ // task is done, every thread will call this (but we put only one)
  copy_job_t *j = (copy_job_t *)arg;
  for(uint32_t i=0;i<j->cnt;i++) free(j->ent[i]);
  if(j->ent) free(j->ent);
}
void copy_job_work(uint32_t item, void *arg)
{
  copy_job_t *j = (copy_job_t *)arg;
  if(j->abort) return;
  char src[1300], dst[1300];
  snprintf(src, sizeof(src), "%s/%s", j->src, j->ent[item]->d_name);
  snprintf(dst, sizeof(dst), "%s/%s", j->dst, j->ent[item]->d_name);
  if(fs_copy(dst, src)) j->abort = 2;
  else if(j->move) fs_delete(src);
  glfwPostEmptyEvent(); // redraw status bar
}
int copy_job(
    copy_job_t *j,
    const char *dst,
    const char *src)
{
  j->abort = 0;
  snprintf(j->src, sizeof(j->src), "%.*s", (int)sizeof(j->src)-1, src);
  snprintf(j->dst, sizeof(j->dst), "%.*s", (int)sizeof(j->dst)-1, dst);
  fs_mkdir(j->dst, 0777); // try and potentially fail to create destination directory
  j->cnt = scandir(src, &j->ent, 0, alphasort);
  if(j->cnt == -1u) return 2;
  j->taskid = threads_task("copy", j->cnt, -1, j, copy_job_work, copy_job_cleanup);
  return j->taskid;
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

  if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
     ImGui::IsKeyPressed(ImGuiKey_Escape)||
     ImGui::IsKeyPressed(ImGuiKey_CapsLock))
    dt_view_switch(s_view_lighttable);

  { // right panel
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + qvk.win_width - vkdt.state.panel_wd,
          ImGui::GetMainViewport()->Pos.y + 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::Begin("files panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    // if(ImGui::CollapsingHeader("settings")) ??
#if 0
    if(ImGui::CollapsingHeader("folder"))
    {
      ImGui::Indent();
      ImGui::Unindent();
      // TODO: show images/show only directories
      // TODO: feature to show only raw/whatever?
    }
#endif
    if(ImGui::CollapsingHeader("drives"))
    {
      ImGui::Indent();
      static int cnt = 0;
      static char devname[20][20] = {{0}}, mountpoint[20][50] = {{0}};
      char command[1000];
      if(ImGui::Button("refresh list"))
        cnt = fs_find_usb_block_devices(devname, mountpoint);
      for(int i=0;i<cnt;i++)
      {
        int red = mountpoint[i][0];
        if(red)
        {
          ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogramHovered]);
        }
        if(ImGui::Button(devname[i]))
        {
          if(red) snprintf(command, sizeof(command), "/usr/bin/udisksctl unmount -b %s", devname[i]);
          // TODO: use -f for lazy unmounting?
          // TODO: also send a (blocking?) power-off command right after that?
          else    snprintf(command, sizeof(command), "/usr/bin/udisksctl mount -b %s", devname[i]);
          FILE *f = popen(command, "r");
          if(f)
          {
            if(!red)
            {
              fscanf(f, "Mounted %*s at %49s", mountpoint[i]);
              set_cwd(mountpoint[i], 0);
            }
            else mountpoint[i][0] = 0;
            pclose(f); // TODO: if(.) need to refresh the list
          }
        }
        if(ImGui::IsItemHovered()) dt_gui_set_tooltip(red ? "click to unmount" : "click to mount");
        if(red)
        {
          ImGui::PopStyleColor(2);
          ImGui::SameLine();
          ImGui::PushID(i);
          if(ImGui::Button("go to mountpoint", ImVec2(-1,0)))
            set_cwd(mountpoint[i], 0);
          if(ImGui::IsItemHovered()) dt_gui_set_tooltip("%s", mountpoint[i]);
          ImGui::PopID();
        }
      }
      ImGui::Unindent();
    }
    if(ImGui::CollapsingHeader("recent collections"))
    { // recently used collections in ringbuffer:
      ImGui::Indent();
      if(recently_used_collections())
      {
        set_cwd(vkdt.db.dirname, 0);
        dt_filebrowser_cleanup(&filebrowser); // make it re-read cwd
      }
      ImGui::Unindent();
    } // end collapsing header "recent collections"
    if(ImGui::CollapsingHeader("import"))
    {
      ImGui::Indent();
      static char pattern[100] = {0};
      if(pattern[0] == 0) snprintf(pattern, sizeof(pattern), "%s", dt_rc_get(&vkdt.rc, "gui/copy_destination", "${home}/Pictures/${date}_${dest}"));
      if(ImGui::InputText("pattern", pattern, sizeof(pattern))) dt_rc_set(&vkdt.rc, "gui/copy_destination", pattern);
      if(ImGui::IsItemHovered())
        dt_gui_set_tooltip("destination directory pattern. expands the following:\n"
                          "${home} - home directory\n"
                          "${date} - YYYYMMDD date\n"
                          "${yyyy} - four char year\n"
                          "${dest} - dest string just below");
      static char dest[20];
      ImGui::InputText("dest", dest, sizeof(dest));
      if(ImGui::IsItemHovered())
        dt_gui_set_tooltip(
            "enter a descriptive string to be used as the ${dest} variable when expanding\n"
            "the 'gui/copy_destination' pattern from the config.rc file. it is currently\n"
            "`%s'", pattern);
      static copy_job_t job[4] = {{{0}}};
      static int32_t copy_mode = 0;
      int32_t num_idle = 0;
      const char *copy_mode_str = "keep original\0delete original\0\0";
      ImGui::Combo("copy mode", &copy_mode, copy_mode_str);
      for(int k=0;k<4;k++)
      { // list of four jobs to copy stuff simultaneously
        ImGui::PushID(k);
        if(job[k].cnt == 0)
        { // idle job
          if(num_idle++)
          { // show at max one idle job
            ImGui::PopID();
            break;
          }
          if(ImGui::Button("copy"))
          { // make sure we don't start a job that is already running in another job[.]
            int duplicate = 0;
            for(int k2=0;k2<4;k2++)
            {
              if(k2 == k) continue; // our job is not a dupe
              if(!strcmp(job[k2].src, filebrowser.cwd)) duplicate = 1;
            }
            if(duplicate)
            { // this doesn't sound right
              ImGui::SameLine();
              ImGui::Text("duplicate warning!");
              if(ImGui::IsItemHovered())
                dt_gui_set_tooltip("another job already has the current directory as source."
                                  "it may still be running or be aborted or have finished already,"
                                  "but either way you may want to double check you actually want to"
                                  "start this again (and if so reset the job in question)");
            }
            else
            { // green light :)
              job[k].move = copy_mode;
              char dst[1000];
              fs_expand_import_filename(pattern, strlen(pattern), dst, sizeof(dst), dest);
              copy_job(job+k, dst, filebrowser.cwd);
            }
          }
          if(ImGui::IsItemHovered())
            dt_gui_set_tooltip("copy contents of %s\nto %s,\n%s",
                filebrowser.cwd, pattern, copy_mode ? "delete original files after copying" : "keep original files");
        }
        else if(job[k].cnt > 0 && threads_task_running(job[k].taskid))
        { // running
          if(ImGui::Button("abort")) job[k].abort = 1;
          ImGui::SameLine();
          ImGui::ProgressBar(threads_task_progress(job[k].taskid), ImVec2(-1, 0));
          if(ImGui::IsItemHovered()) dt_gui_set_tooltip("copying %s to %s", job[k].src, job[k].dst);
        }
        else
        { // done/aborted
          if(ImGui::Button(job[k].abort ? "aborted" : "done"))
          { // reset
            memset(job+k, 0, sizeof(copy_job_t));
          }
          if(ImGui::IsItemHovered()) dt_gui_set_tooltip(
              job[k].abort == 1 ? "copy from %s aborted by user. click to reset" :
             (job[k].abort == 2 ? "copy from %s incomplete. file system full?\nclick to reset" :
              "copy from %s done. click to reset"),
             job[k].src);
          if(!job[k].abort)
          {
            ImGui::SameLine();
            if(ImGui::Button("view copied files", ImVec2(-1, 0)))
            {
              set_cwd(job[k].dst, 1);
              dt_gui_switch_collection(job[k].dst);
              memset(job+k, 0, sizeof(copy_job_t));
              dt_view_switch(s_view_lighttable);
            }
            if(ImGui::IsItemHovered()) dt_gui_set_tooltip(
                "open %s in lighttable mode",
                job[k].dst);
          }
        }
        ImGui::PopID();
      } // end for jobs
      ImGui::Unindent();
    }
    ImGui::End();
  }

  { // bottom panel with buttons
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    ImVec2 border = ImVec2(2*win_x, win_y);
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + 0,
          ImGui::GetMainViewport()->Pos.y + win_h+border.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+border.x, border.y), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::Begin("files buttons", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    int bwd = win_w / 4;
    int twd = ImGui::GetStyle().ItemSpacing.x + bwd * 2;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - twd);
    if(ImGui::Button("open in lighttable", ImVec2(bwd, 0)))
    {
      dt_gui_switch_collection(filebrowser.cwd);
      dt_view_switch(s_view_lighttable);
    }
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip("open current directory in light table mode");
    ImGui::SameLine();
    if(ImGui::Button("back to lighttable", ImVec2(bwd, 0)))
    {
      dt_view_switch(s_view_lighttable);
    }
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip("return to lighttable mode without changing directory");
    ImGui::End();
  }

  { // center window (last so it has key focus by default)
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    ImVec2 border = ImVec2(2*win_x, win_y);
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + 0,
          ImGui::GetMainViewport()->Pos.y + 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+border.x, win_h+border.y), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("files center", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    dt_filebrowser(&filebrowser, 'f');

    if(filebrowser.selected &&
      (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp) ||
       ImGui::IsKeyPressed(ImGuiKey_Enter))) // triangle or enter
    { // open selected in lt without changing cwd
      char newdir[PATH_MAX];
      if(!strcmp(filebrowser.selected, ".."))
        set_cwd(filebrowser.cwd, 1);
      else
      {
        if(filebrowser.selected_type == DT_DIR)
        {
          if(snprintf(newdir, sizeof(newdir), "%s%s", filebrowser.cwd, filebrowser.selected) < (int)sizeof(newdir)-1)
            dt_gui_switch_collection(newdir);
        }
        else dt_gui_switch_collection(filebrowser.cwd);
        dt_view_switch(s_view_lighttable);
      }
    }

    // draw context sensitive help overlay
    if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();

    ImGui::End();
    ImGui::PopStyleColor(1);
  } // end center window
}
