// imgui rendering for the files view
extern "C" {
#include "gui/gui.h"
#include "gui/view.h"
#include "core/fs.h"
#include "core/strexpand.h"
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
  if(fs_copy(dst, src)) j->abort = 1;
  else if(j->move) fs_delete(src);
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
  j->taskid = threads_task(j->cnt, -1, j, copy_job_work, copy_job_cleanup);
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

  if(dt_gui_imgui_nav_input(ImGuiNavInput_Cancel) > 0.0f)
    dt_view_switch(s_view_lighttable);

  { // right panel
    ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::Begin("files panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    // if(ImGui::CollapsingHeader("settings")) ??
    if(ImGui::CollapsingHeader("folder"))
    {
      ImGui::Indent();
      ImGui::Text("%s", filebrowser.cwd);
      if(ImGui::Button("open in lighttable"))
      {
        dt_gui_switch_collection(filebrowser.cwd);
        dt_view_switch(s_view_lighttable);
      }
      if(ImGui::Button("back to lighttable"))
      {
        dt_view_switch(s_view_lighttable);
      }
      ImGui::Unindent();
      // TODO: show images/show only directories
      // TODO: feature to show only raw/whatever?
    }
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
          ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
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
        if(ImGui::IsItemHovered()) ImGui::SetTooltip(red ? "click to unmount" : "click to mount");
        if(red)
        {
          ImGui::PopStyleColor(3);
          ImGui::SameLine();
          if(ImGui::Button("go to mountpoint", ImVec2(-1,0)))
            set_cwd(mountpoint[i], 0);
          if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", mountpoint[i]);
        }
      }
      ImGui::Unindent();
    }
    if(ImGui::CollapsingHeader("import"))
    {
      ImGui::Indent();
      static char pattern[100] = {0};
      if(pattern[0] == 0) snprintf(pattern, sizeof(pattern), "%s", dt_rc_get(&vkdt.rc, "gui/copy_destination", "${home}/Pictures/${date}_${dest}"));
      if(ImGui::InputText("pattern", pattern, sizeof(pattern))) dt_rc_set(&vkdt.rc, "gui/copy_destination", pattern);
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip("destination directory pattern. expands the following:\n"
                          "${home} - home directory\n"
                          "${date} - YYYYMMDD date\n"
                          "${yyyy} - four char year\n"
                          "${dest} - dest string just below");
      static char dest[20];
      ImGui::InputText("dest", dest, sizeof(dest));
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip(
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
        if(job[k].cnt == 0)
        { // idle job
          if(num_idle++) break; // show at max one idle job
          if(ImGui::Button("copy"))
          {
            // TODO: make sure we don't start a job that is already running in another job[.]
            job[k].move = copy_mode;
            char dst[1000];
            time_t t = time(0);
            struct tm *tm = localtime(&t);
            char date[10] = {0}, yyyy[5] = {0};
            strftime(date, sizeof(date), "%Y%m%d", tm);
            strftime(yyyy, sizeof(yyyy), "%Y", tm);
            const char *key[] = { "home", "yyyy", "date", "dest", 0};
            const char *val[] = { getenv("HOME"), yyyy, date, dest, 0};
            dt_strexpand(pattern, strlen(pattern), dst, sizeof(dst), key, val);
            copy_job(job+k, dst, filebrowser.cwd);
          }
          if(ImGui::IsItemHovered())
            ImGui::SetTooltip("copy contents of %s\nto %s,\n%s",
                filebrowser.cwd, pattern, copy_mode ? "delete original files after copying" : "keep original files");
        }
        else if(job[k].cnt > 0 && threads_task_running(job[k].taskid))
        { // running
          if(ImGui::Button("abort")) job[k].abort = 1;
          ImGui::SameLine();
          ImGui::ProgressBar(threads_task_progress(job[k].taskid), ImVec2(-1, 0));
        }
        else
        { // done/aborted
          if(ImGui::Button(job[k].abort ? "aborted" : "done"))
          { // reset
            memset(job+k, 0, sizeof(copy_job_t));
          }
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
    ImGui::End();
  }

  { // center window (last so it has key focus by default)
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    ImVec2 border = ImVec2(2*win_x, 2*win_y);
    ImGui::SetNextWindowPos (ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+border.x, win_h+border.y), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("files center", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    dt_filebrowser(&filebrowser, 'f');

    if(dt_gui_imgui_nav_input(ImGuiNavInput_Input) > 0.0f) // triangle or enter
    { // open selected in lt without changing cwd
      char newdir[PATH_MAX];
      if(filebrowser.selected_type == DT_DIR)
      {
        snprintf(newdir, sizeof(newdir), "%s%s", filebrowser.cwd, filebrowser.selected);
        dt_gui_switch_collection(newdir);
      }
      else dt_gui_switch_collection(filebrowser.cwd);
      dt_view_switch(s_view_lighttable);
    }

    ImGui::End();
    ImGui::PopStyleColor(1);
  } // end center window
}
