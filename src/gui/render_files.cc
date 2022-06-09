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

struct copy_job_t
{ // copy contents of a folder
  char src[1000], dst[1000];
  struct dirent **ent;
  uint32_t cnt;
  uint32_t done;
  uint32_t move;  // set to non-zero to remove src after copy
  uint32_t abort;
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
  if(dt_file_copy(dst, src)) j->abort = 1;
  else if(j->move) dt_file_delete(src);
}
int copy_job(
    copy_job_t *j,
    const char *dst,
    const char *src)
{
  if(j->done < j->cnt) return 1; // old job still going
  j->abort = 0;
  j->done  = 0;
  snprintf(j->src, sizeof(j->src), "%s", src);
  snprintf(j->dst, sizeof(j->dst), "%s", dst);
  dt_mkdir(j->dst, 0777); // try and potentially fail to create destination directory
  j->cnt = scandir(src, &j->ent, 0, alphasort);
  if(j->cnt < 0) return 2;
  return threads_task(j->cnt, 0, &j->done, j, copy_job_work, copy_job_cleanup);
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
      ImGui::Unindent();
      // TODO: show images/show only directories
      // TODO: feature to show only raw/whatever?
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
    if(ImGui::CollapsingHeader("import"))
    {
      ImGui::Indent();
      static char pattern[100] = {0};
      if(pattern[0] == 0) snprintf(pattern, sizeof(pattern), dt_rc_get(&vkdt.rc, "gui/copy_destination", "${home}/Pictures/${date}_${dest}"));
      if(ImGui::InputText("", pattern, sizeof(pattern))) dt_rc_set(&vkdt.rc, "gui/copy_destination", pattern);
      ImGui::SameLine();
      ImGui::Text("pattern");
      static char dest[20];
      ImGui::InputText("", dest, sizeof(dest));
      ImGui::SameLine();
      ImGui::Text("dest");
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "enter a descriptive string to be used as the ${dest} variable when expanding\n"
            "the 'gui/copy_destination' pattern from the config.rc file. it is currently\n"
            "`%s'", pattern);
      static copy_job_t job[4] = {{0}};
      int32_t copy_mode = 0, num_idle = 0;
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
            char date[10] = {0};
            strftime(date, sizeof(date), "%Y%m%d", tm);
            const char *key[] = { "home", "date", "dest", 0};
            const char *val[] = { getenv("HOME"), date, dest, 0};
            dt_strexpand(pattern, strlen(pattern), dst, sizeof(dst), key, val);
            copy_job(job+k, dst, filebrowser.cwd);
          }
          if(ImGui::IsItemHovered())
            ImGui::SetTooltip("copy contents of %s\nto %s,\n%s",
                filebrowser.cwd, pattern, copy_mode ? "delete original files after copying" : "keep original files");
        }
        else if(job[k].cnt > 0 && job[k].done < job[k].cnt)
        { // running
          if(ImGui::Button("abort")) job[k].abort = 1;
          ImGui::SameLine();
          ImGui::ProgressBar((float)job[k].cnt/(float)job[k].done, ImVec2(-1, 0));
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
    // TODO: keyboard nav in filebrowser?
    ImGui::End();
  }
}
