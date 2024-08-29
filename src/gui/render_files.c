// rendering for the files view
#include "gui/gui.h"
#include "gui/view.h"
#include "core/fs.h"
#include "gui/render_view.h"
#include "gui/hotkey.h"
#include "gui/widget_filebrowser.h"
#include "gui/widget_recentcollect.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static dt_filebrowser_widget_t filebrowser = {{0}};

static hk_t hk_files[] = {
  {"focus filter",    "move the gui focus to the filter edit box",      {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_F}},
  {"focus path",      "move the gui focus to the path edit box",        {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_L}},
};

enum hotkey_names_t
{ // for sane access in code
  s_hotkey_focus_filter = 0,
  s_hotkey_focus_path   = 1,
};

void set_cwd(const char *dir, int up)
{
  if(dir != filebrowser.cwd) snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", dir);
  // now strip filter, i.e. replace '&' by '\0' in filebrowser.cwd, it might come from recently used collections
  for(int i=0;i<sizeof(filebrowser.cwd) && filebrowser.cwd[i];i++) if(filebrowser.cwd[i] == '&') { filebrowser.cwd[i] = 0; break; }
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

typedef struct copy_job_t
{ // copy contents of a folder
  char src[1000], dst[1000];
  struct dirent *ent;
  uint32_t cnt;
  uint32_t move;  // set to non-zero to remove src after copy
  _Atomic uint32_t abort;
  _Atomic uint32_t state;
  int taskid;
} copy_job_t;
void copy_job_cleanup(void *arg)
{ // task is done, every thread will call this (but we put only one)
  copy_job_t *j = (copy_job_t *)arg;
  if(j->ent) free(j->ent);
  j->state = 2;
}
void copy_job_work(uint32_t item, void *arg)
{
  copy_job_t *j = (copy_job_t *)arg;
  if(j->abort) return;
  char src[1300], dst[1300];
  snprintf(src, sizeof(src), "%s/%s", j->src, j->ent[item].d_name);
  snprintf(dst, sizeof(dst), "%s/%s", j->dst, j->ent[item].d_name);
  if(fs_copy(dst, src)) j->abort = 2;
  else if(j->move) fs_delete(src);
  glfwPostEmptyEvent(); // redraw status bar
}
int copy_job(
    copy_job_t *j,
    const char *dst, // destination directory
    const char *src) // source directory
{
  j->abort = 0;
  j->state = 1;
  snprintf(j->src, sizeof(j->src), "%.*s", (int)sizeof(j->src)-1, src);
  snprintf(j->dst, sizeof(j->dst), "%.*s", (int)sizeof(j->dst)-1, dst);
  fs_mkdir_p(j->dst, 0777); // try and potentially fail to create destination directory

  DIR* dirp = opendir(j->src);
  j->cnt = 0;
  struct dirent *ent = 0;
  if(!dirp) return 2;
  // first count valid entries
  while((ent = readdir(dirp)))
    if(!fs_isdir(j->src, ent))
      j->cnt++;
  if(!j->cnt) return 2;
  rewinddir(dirp); // second pass actually record stuff
  j->ent = (struct dirent *)malloc(sizeof(j->ent[0])*j->cnt);
  j->cnt = 0;
  while((ent = readdir(dirp)))
    if(!fs_isdir(j->src, ent))
      j->ent[j->cnt++] = *ent;
  closedir(dirp);

  j->taskid = threads_task("copy", j->cnt, -1, j, copy_job_work, copy_job_cleanup);
  return j->taskid;
}

void render_files()
{
  static int just_entered = 1;
  if(just_entered)
  {
    const char *mru = dt_rc_get(&vkdt.rc, "gui/ruc_entry00", "null");
    if(strcmp(mru, "null")) set_cwd(mru, 1);
    just_entered = 0;
  }

  struct nk_context *ctx = &vkdt.ctx;
  struct nk_rect bounds = {qvk.win_width - vkdt.state.panel_wd, 0, vkdt.state.panel_wd, qvk.win_height};
  const float ratio[] = {vkdt.state.panel_wd*0.6, vkdt.state.panel_wd*0.3}; // XXX padding?
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  const struct nk_vec2 size = {ratio[0], ratio[0]};
  const int disabled = vkdt.wstate.popup;
  if(nk_begin(ctx, "files panel right", bounds, disabled ? NK_WINDOW_NO_INPUT : 0))
  { // right panel
    if(nk_tree_push(ctx, NK_TREE_TAB, "settings", NK_MINIMIZED))
    {
      nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
      if(nk_button_label(ctx, "hotkeys"))
        dt_gui_edit_hotkeys();
      nk_tree_pop(ctx);
    }

    if(nk_tree_push(ctx, NK_TREE_TAB, "drives", NK_MINIMIZED))
    {
      static int cnt = 0;
      static char devname[20][20] = {{0}}, mountpoint[20][50] = {{0}};
      char command[1000];
      nk_layout_row_dynamic(ctx, row_height, 1);
      if(nk_button_label(ctx, "refresh list"))
        cnt = fs_find_usb_block_devices(devname, mountpoint);
      nk_layout_row_dynamic(ctx, row_height, 2);
      for(int i=0;i<cnt;i++)
      {
        int red = mountpoint[i][0];
        if(red)
        {
          nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
        }
        dt_tooltip(red ? "click to unmount" : "click to mount");
        if(nk_button_label(ctx, devname[i]))
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
        if(red)
        {
          nk_style_pop_style_item(ctx);
          dt_tooltip("%s", mountpoint[i]);
          if(nk_button_label(ctx, "go to mountpoint"))
            set_cwd(mountpoint[i], 0);
        }
        else nk_label(ctx, "", 0); // fill row
      }
      nk_tree_pop(ctx);
    } // end drives

    if(nk_tree_push(ctx, NK_TREE_TAB, "recent collections", NK_MINIMIZED))
    { // recently used collections in ringbuffer:
      if(recently_used_collections())
      {
        set_cwd(vkdt.db.dirname, 0);
        dt_filebrowser_cleanup(&filebrowser); // make it re-read cwd
      }
      nk_tree_pop(ctx);
    } // end collapsing header "recent collections"

    static copy_job_t job[4] = {{{0}}};
    int active = job[0].state | job[1].state | job[2].state | job[3].state;
    if(nk_tree_push(ctx, NK_TREE_TAB, "import", active ? NK_MAXIMIZED : NK_MINIMIZED))
    {
      const float ratio[] = {0.7f, 0.3f};
      nk_layout_row(ctx, NK_DYNAMIC, row_height, 2, ratio);
      static char pattern[100] = {0};
      if(pattern[0] == 0) snprintf(pattern, sizeof(pattern), "%s", dt_rc_get(&vkdt.rc, "gui/copy_destination", "${home}/Pictures/${date}_${dest}"));
      dt_tooltip("destination directory pattern. expands the following:\n"
          "${home} - home directory\n"
          "${date} - YYYYMMDD date\n"
          "${yyyy} - four char year\n"
          "${dest} - dest string just below");
      nk_flags ret = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, pattern, sizeof(pattern), nk_filter_default);
      if(ret & NK_EDIT_COMMITED) dt_rc_set(&vkdt.rc, "gui/copy_destination", pattern);
      nk_label(ctx, "destination", NK_TEXT_LEFT);
      static char dest[20];
      dt_tooltip(
          "enter a descriptive string to be used as the ${dest} variable when expanding\n"
          "the 'gui/copy_destination' pattern from the config.rc file. it is currently\n"
          "`%s'", pattern);
      nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, dest, sizeof(dest), nk_filter_default);
      nk_label(ctx, "dest", NK_TEXT_LEFT);
      static int32_t copy_mode = 0;
      int32_t num_idle = 0;
      const char *copy_mode_str = "keep original\0delete original\0\0";
      nk_combobox_string(ctx, copy_mode_str, &copy_mode, 0x7fff, row_height, size);
      nk_label(ctx, "overwrite", NK_TEXT_LEFT);
      for(int k=0;k<4;k++)
      { // list of four jobs to copy stuff simultaneously
        if(job[k].state == 0)
        { // idle job
          if(num_idle++) // show at max one idle job
            break;
          dt_tooltip("copy contents of %s\nto %s,\n%s",
              filebrowser.cwd, pattern, copy_mode ? "delete original files after copying" : "keep original files");
          if(nk_button_label(ctx, "copy"))
          { // make sure we don't start a job that is already running in another job[.]
            int duplicate = 0;
            for(int k2=0;k2<4;k2++)
            {
              if(k2 == k) continue; // our job is not a dupe
              if(!strcmp(job[k2].src, filebrowser.cwd)) duplicate = 1;
            }
            if(duplicate)
            { // this doesn't sound right
              dt_tooltip("another job already has the current directory as source."
                  "it may still be running or be aborted or have finished already,"
                  "but either way you may want to double check you actually want to"
                  "start this again (and if so reset the job in question)");
              nk_label(ctx, "duplicate warning!", NK_TEXT_LEFT);
            }
            else
            { // green light :)
              job[k].move = copy_mode;
              char dst[1000];
              fs_expand_import_filename(pattern, strlen(pattern), dst, sizeof(dst), dest);
              copy_job(job+k, dst, filebrowser.cwd);
              nk_label(ctx, "", 0);
            }
          }
          else nk_label(ctx, "", 0);
        }
        else if(job[k].state == 1)
        { // running
          dt_tooltip("copying %s to %s", job[k].src, job[k].dst);
          float progress = threads_task_progress(job[k].taskid);
          struct nk_rect bb = nk_widget_bounds(ctx);
          nk_prog(ctx, 1024*progress, 1024, nk_false);
          char text[50];
          snprintf(text, sizeof(text), "%.0f%%", 100.0*progress);
          nk_draw_text(nk_window_get_canvas(ctx), bb, text, strlen(text), &dt_gui_get_font(0)->handle, nk_rgba(0,0,0,0), nk_rgba(255,255,255,255));
          if(nk_button_label(ctx, "abort")) job[k].abort = 1;
        }
        else
        { // done/aborted
          dt_tooltip(
              job[k].abort == 1 ? "copy from %s aborted by user. click to reset" :
             (job[k].abort == 2 ? "copy from %s incomplete. file system full?\nclick to reset" :
              "copy from %s done. click to reset"),
             job[k].src);
          if(nk_button_label(ctx, job[k].abort ? "aborted" : "done"))
          { // reset
            job[k].state = 0;
          }
          if(!job[k].abort)
          {
            dt_tooltip("open %s in lighttable mode", job[k].dst);
            if(nk_button_label(ctx, "view copied files"))
            {
              set_cwd(job[k].dst, 1);
              dt_gui_switch_collection(job[k].dst);
              job[k].state = 0;
              dt_view_switch(s_view_lighttable);
            }
          }
          else nk_label(ctx, "", 0);
        }
      } // end for jobs
      nk_tree_pop(ctx);
    } // end import
    NK_UPDATE_ACTIVE;
    nk_end(ctx);
  }

  bounds = (struct nk_rect){vkdt.state.center_x, vkdt.state.center_ht, vkdt.state.center_wd, vkdt.state.center_y};
  nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
  if(nk_begin(ctx, "files buttons", bounds, NK_WINDOW_NO_SCROLLBAR | (disabled ? NK_WINDOW_NO_INPUT : 0)))
  { // bottom panel with buttons
    nk_layout_row_dynamic(ctx, row_height, 5);
    nk_label(ctx, "", 0); nk_label(ctx, "", 0); nk_label(ctx, "", 0);
    dt_tooltip("return to lighttable mode without changing directory");
    if(nk_button_label(ctx, "back to lighttable"))
    {
      dt_view_switch(s_view_lighttable);
    }
    dt_tooltip("open current directory in light table mode");
    if(nk_button_label(ctx, "open in lighttable"))
    {
      dt_gui_switch_collection(filebrowser.cwd);
      dt_view_switch(s_view_lighttable);
    }
    NK_UPDATE_ACTIVE;
    nk_end(ctx);
  }

  bounds = (struct nk_rect){vkdt.state.center_x, vkdt.state.center_y, vkdt.state.center_wd, vkdt.state.center_ht-vkdt.state.center_y};
  if(nk_begin(ctx, "files center", bounds, NK_WINDOW_NO_SCROLLBAR | (disabled ? NK_WINDOW_NO_INPUT : 0)))
  {
    dt_filebrowser(&filebrowser, 'f');
    // draw context sensitive help overlay
    if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();
    NK_UPDATE_ACTIVE;
    nk_end(ctx);
  } // end center window
  nk_style_pop_style_item(&vkdt.ctx);

  // popup windows
  bounds = nk_rect(vkdt.state.center_x+0.2*vkdt.state.center_wd, vkdt.state.center_y+0.2*vkdt.state.center_ht,
    0.6*vkdt.state.center_wd, 0.6*vkdt.state.center_ht);
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
  {
    if(nk_begin(&vkdt.ctx, "edit files hotkeys", bounds, NK_WINDOW_NO_SCROLLBAR))
    {
      int ok = hk_edit(hk_files, NK_LEN(hk_files));
      if(ok) vkdt.wstate.popup = 0;
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
}

void
files_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
  dt_filebrowser_widget_t *w = &filebrowser;
  if(action == GLFW_RELEASE)
  { // double clicked a selected thing?
    static double beg = 0;
    double end = dt_time();
    double diff_ms = 1000.0*(end-beg);
    beg = end;
    if(diff_ms > 2 && diff_ms < 500)
    {
      if(w->selected && w->selected_isdir)
      { // directory double-clicked
        // change cwd by appending to the string
        int len = strnlen(w->cwd, sizeof(w->cwd));
        char *c = w->cwd;
        if(!strcmp(w->selected, ".."))
        { // go up one dir
          c += len;
          *(--c) = 0;
          while(c > w->cwd && (*c != '/' && *c != '\\')) *(c--) = 0;
        }
        else
        { // append dir name
          snprintf(c+len, sizeof(w->cwd)-len-1, "%s/", w->selected);
        }
        // and then clean up the dirent cache
        dt_filebrowser_cleanup(w);
      }
    }
  }
}

void
files_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
    return hk_keyboard(hk_files, window, key, scancode, action, mods);
  if(dt_gui_input_blocked()) return;
  dt_filebrowser_widget_t *w = &filebrowser;

  int hotkey = action == GLFW_PRESS ? hk_get_hotkey(hk_files, sizeof(hk_files)/sizeof(hk_files[0]), key) : -1;
  switch(hotkey)
  {
    case s_hotkey_focus_filter:
      w->focus_filter = 1;
      break;
    case s_hotkey_focus_path:
      w->focus_path = 1;
      break;
    default:;
  }

  if((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_UP)
  { // up arrow: select entry above
    w->selected_idx = CLAMP(w->selected_idx - 1, 0, w->ent_cnt-1);
    w->selected = w->ent[w->selected_idx].d_name;
    w->selected_isdir = fs_isdir(w->cwd, w->ent+w->selected_idx);
  }
  else if((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_DOWN)
  { // down arrow: select entry below
    w->selected_idx = CLAMP(w->selected_idx + 1, 0, w->ent_cnt-1);
    w->selected = w->ent[w->selected_idx].d_name;
    w->selected_isdir = fs_isdir(w->cwd, w->ent+w->selected_idx);
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_SPACE)
  { // space bar to descend into directory in file browser
    if(w->selected_isdir)
    { // directory double-clicked
      // change cwd by appending to the string
      int len = strnlen(w->cwd, sizeof(w->cwd));
      char *c = w->cwd;
      if(!strcmp(w->selected, ".."))
      { // go up one dir
        c += len;
        *(--c) = 0;
        while(c > w->cwd && (*c != '/' && *c != '\\')) *(c--) = 0;
      }
      else
      { // append dir name
        snprintf(c+len, sizeof(w->cwd)-len-1, "%s/", w->selected);
      }
      // and then clean up the dirent cache
      dt_filebrowser_cleanup(w);
    }
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
  { // escape to go back to light table
    dt_view_switch(s_view_lighttable);
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_BACKSPACE)
  { // backspace to go up once
    int len = strnlen(w->cwd, sizeof(w->cwd));
    char *c = w->cwd + len;
    *(--c) = 0;
    while(c > w->cwd && *c != '/') *(c--) = 0;
    dt_filebrowser_cleanup(w);
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_ENTER)
  { // enter to go to lighttable with new folder
    if(filebrowser.selected)
    { // open selected in lt without changing cwd
      char newdir[PATH_MAX];
      if(!strcmp(filebrowser.selected, ".."))
        set_cwd(filebrowser.cwd, 1);
      else
      {
        if(filebrowser.selected_isdir)
        {
          if(snprintf(newdir, sizeof(newdir), "%s%s", filebrowser.cwd, filebrowser.selected) < (int)sizeof(newdir)-1)
            dt_gui_switch_collection(newdir);
        }
        else dt_gui_switch_collection(filebrowser.cwd);
        dt_view_switch(s_view_lighttable);
      }
    }
  }
}

void render_files_init()
{
  hk_deserialise("files", hk_files, sizeof(hk_files)/sizeof(hk_files[0]));
}

void render_files_cleanup()
{
  hk_serialise("files", hk_files, sizeof(hk_files)/sizeof(hk_files[0]));
}
