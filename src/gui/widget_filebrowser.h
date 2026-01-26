#pragma once

#include "gui/render.h"
#include "core/fs.h"
#include "core/sort.h"
#include "widget_filteredlist.h" // for tooltip
#include <dirent.h>

// store some state
typedef struct dt_filebrowser_widget_t
{
  char cwd[PATH_MAX];      // current working directory
  struct dirent *ent;      // cached directory entries
  int ent_cnt;             // number of cached directory entries
  int selected_idx;        // index of selected ent
  const char *selected;    // points to selected file name ent->d_name
  int selected_isdir;      // selected is a directory

  int focus_filter;        // signal we want gui focus
  int focus_path;
  int scroll_to_selected;  // keyboard navigation needs scrolling to selected entry
}
dt_filebrowser_widget_t;

// no need to explicitly call it, just sets 0
inline void
dt_filebrowser_init(
    dt_filebrowser_widget_t *w)
{
  memset(w, 0, sizeof(*w));
}

// this makes sure all memory is freed and also that the directory is re-read for new cwd.
static inline void
dt_filebrowser_cleanup(
    dt_filebrowser_widget_t *w)
{
  free(w->ent);
  w->ent_cnt = 0;
  w->ent = 0;
  w->selected_idx = -1;
  w->selected = 0;
  w->selected_isdir = 0;
  w->focus_filter = w->focus_path = w->scroll_to_selected = 0;
}

static int dt_filebrowser_sort_dir_first(const void *aa, const void *bb, void *cw)
{
  const struct dirent *a = (const struct dirent *)aa;
  const struct dirent *b = (const struct dirent *)bb;
  const char *cwd = (const char *)cw;
  if( fs_isdir(cwd, a) && !fs_isdir(cwd, b)) return -1;
  if(!fs_isdir(cwd, a) &&  fs_isdir(cwd, b)) return 1;
  return strcoll(a->d_name, b->d_name);
}

static int dt_filebrowser_filter_dir(const struct dirent *d, const char *cwd, const char *filter)
{
  if(d->d_name[0] == '.' && d->d_name[1] != '.') return 0; // filter out hidden files
  if(filter[0] && !strstr(d->d_name, filter)) return 0; // if filter required, apply it
  if(!fs_isdir(cwd, d)) return 0; // filter out non-dirs too
  return 1;
}

static int dt_filebrowser_filter_file(const struct dirent *d, const char *cwd, const char *filter)
{
  if(d->d_name[0] == '.' && d->d_name[1] != '.') return 0; // filter out hidden files
  if(filter[0] && !strstr(d->d_name, filter)) return 0; // if filter required, apply it
  return 1;
}

static inline void
dt_filebrowser(
    dt_filebrowser_widget_t *w,
    const char               mode) // 'f' or 'd'
{
  struct nk_context *ctx = &vkdt.ctx;
  static char filter[100] = {0};
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

  nk_style_push_font(ctx, nk_glfw3_font(2));
  float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  float wds[] = {0, 0.5, 0.5};
  nk_layout_row(ctx, NK_DYNAMIC, row_height, 3, wds);
  nk_label(ctx, "", 0); // introduce fake object to match left alignment with list below
  nk_style_push_font(ctx, nk_glfw3_font(0));
  dt_tooltip("current working directory.\nedit to taste and press enter to change");
  nk_style_pop_font(ctx);
  if(w->focus_path) nk_edit_focus(ctx, 0);
  nk_flags ret = nk_tab_edit_string_zero_terminated(ctx,
      (w->focus_path ? NK_EDIT_AUTO_SELECT|NK_EDIT_SELECTABLE : 0) |
      NK_EDIT_ALWAYS_INSERT_MODE|NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, w->cwd, sizeof(w->cwd), nk_filter_default);
  if(ret & NK_EDIT_COMMITED)
  {
    int len = strlen(w->cwd);
    if(len + 1 < sizeof(w->cwd) && len && w->cwd[len-1] != '/')
    {
      w->cwd[len]   = '/';
      w->cwd[len+1] = 0;
    }
    nk_edit_unfocus(ctx); // make keyboard nav in list below work
    dt_filebrowser_cleanup(w);
  }
  nk_style_push_font(ctx, nk_glfw3_font(0));
  dt_tooltip("filter the displayed filenames.\ntype a search string and press enter to apply");
  nk_style_pop_font(ctx);
  if(w->focus_filter) nk_edit_focus(ctx, 0);
  ret = nk_tab_edit_string_zero_terminated(ctx,
      (w->focus_filter ? NK_EDIT_AUTO_SELECT|NK_EDIT_SELECTABLE : 0) |
      NK_EDIT_ALWAYS_INSERT_MODE|NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, filter, 256, nk_filter_default);
  if(ret & NK_EDIT_COMMITED)
  {
    nk_edit_unfocus(ctx); // make keyboard nav in list below work
    dt_filebrowser_cleanup(w);
  }
  w->focus_path = w->focus_filter = 0;
#ifdef _WIN64
  int dm = GetLogicalDrives();
  char letter = 'A';
  int haveone = 0;
  nk_layout_row_dynamic(ctx, row_height, 16);
  for(;dm;dm>>=1,letter++)
  {
    if(dm & 1)
    {
      char drive[10];
      snprintf(drive, sizeof(drive), "%c:\\", letter);
      dt_tooltip("click to change to this drive");
      if(nk_button_label(ctx, drive))
      {
        snprintf(w->cwd, sizeof(w->cwd), "%s", drive);
        dt_filebrowser_cleanup(w);
      }
      haveone = 1;
    }
  }
#endif
  nk_style_pop_font(ctx);
  int spacing = 0;
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, spacing));
  nk_style_push_vec2(ctx, &ctx->style.window.group_padding, nk_vec2(ctx->style.window.group_padding.x,0));
  row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  int scroll_to = -1;

  struct nk_rect total_space = nk_window_get_content_region(&vkdt.ctx);
  nk_layout_row_dynamic(ctx, total_space.h-2*row_height, 1);
  nk_group_begin(ctx, "scroll files", 0);
  nk_style_push_font(ctx, nk_glfw3_font(1));
  nk_layout_row_dynamic(ctx, row_height, 1);
  struct nk_rect content = nk_window_get_content_region(&vkdt.ctx);
  for(int i=0;i<w->ent_cnt;i++)
  {
    if(i == w->selected_idx && w->scroll_to_selected)
    {
      struct nk_rect row = nk_widget_bounds(&vkdt.ctx);
      if(row.y < content.y) // scrolled out of top with key nav
        scroll_to = 1+(row.h + spacing) * i;
      if(row.y + row.h > content.y + 0.9*content.h) // about to scroll out of bottom
        scroll_to = 1 - 0.9*content.h + (row.h + spacing) * i;
    }
    char name[260];
    snprintf(name, sizeof(name), "%s %s",
        w->ent[i].d_name,
        fs_isdir(w->cwd, w->ent+i) ? "/":"");
    int selected = w->ent[i].d_name == w->selected;
    nk_bool select = nk_selectable_label(ctx, name, NK_TEXT_LEFT, &selected);
    if(select)
    {
      w->selected_idx = i;
      w->selected = w->ent[i].d_name; // mark as selected
      w->selected_isdir = fs_isdir(w->cwd, w->ent+i);
    }
  }
  nk_style_pop_font(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
  if(scroll_to >= 0) nk_group_set_scroll(ctx, "scroll files", 0, scroll_to);
  w->scroll_to_selected = 0;
  nk_group_end(ctx);
}
