#pragma once
#include "core/sort.h"
#include "gui/widget_tooltip.h"
#include "pipe/res.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// load a one-liner heading from the readme.md file in the directory
static inline char*
filteredlist_get_heading(
    const char *basedir,
    const char *dirname)
{
  char fn[256], *res = 0;
  size_t r = snprintf(fn, sizeof(fn), "%s/%s/readme.md", basedir, dirname);
  if(r >= sizeof(fn)) return 0; // truncated
  FILE *f = dt_graph_open_resource(0, 0, fn, "rb");
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

typedef enum dt_filteredlist_flags_t
{
  s_filteredlist_default      = 0,
  s_filteredlist_allow_new    = 1, // provide "new" button
  s_filteredlist_descr_opt    = 2, // descriptions in dir/readme.md are optionally read and shown
  s_filteredlist_descr_req    = 4, // descriptions are mandatory, else the entry is filtered out
  s_filteredlist_descr_any    = 6, // both the above (for testing only, don't pass this)
  s_filteredlist_return_short = 8, // return only the short filename, not the absolute one
} dt_filteredlist_flags_t;

static inline int dt_filteredlist_compare(const void *aa, const void *bb, void *buf)
{
  const char *a = (const char *)aa;
  const char *b = (const char *)bb;
  return strcmp(a, b);
}

// displays a filtered list of directory entries.
// this is useful to select from existing presets, blocks, tags, etc.
// call this inside a nuklear popup
// return values != 0 mean there was an answer and the popup should close
static inline int            // return 0 - nothing, 1 - ok, 2 - cancel
filteredlist(
    const char *dir,         // optional. relative to global basedir/apk
    const char *dir_local,   // optional. if given will be shown first. relative to home dir.
    char        filter[256], // initial filter string (will be updated)
    char       *retstr,      // selection will be written here
    int         retstr_len,  // buffer size
    dt_filteredlist_flags_t flags)
  // TODO: custom filter rule?
{
  struct nk_context *ctx = &vkdt.ctx;
  int ok = 0;
  int pick = -1, local = 0;
#define FREE_ENT do {\
  if(desc)       for(int i=0;i<ent_cnt;i++) free(desc[i]);\
  if(desc_local) for(int i=0;i<ent_cnt;i++) free(desc_local[i]);\
  free(desc); free(desc_local); \
  desc = desc_local = 0; \
  free(ent_local); ent_local = 0; ent_local_cnt = 0;\
  free(ent); ent = 0; ent_cnt = 0; } while(0)
  static char fnbuf[100*PATH_MAX];
  static char **ent = 0, **ent_local = 0;
  static int ent_cnt = 0, ent_local_cnt = 0;
  static char dirname[PATH_MAX];
  static char dirname_local[PATH_MAX];
  static char **desc = 0, **desc_local = 0;

  static char *last_filter = 0;
  if(filter != last_filter)
  { // hash on passed in filter string pointer/storage
    FREE_ENT;
    last_filter = filter;
  }

  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  struct nk_rect total_space = nk_window_get_content_region(&vkdt.ctx);
  nk_layout_row_dynamic(&vkdt.ctx, row_height, 2);
  dt_tooltip(
      "type to filter the list\n"
      "press enter to apply top item\n"
      "press escape to close");
  if(vkdt.wstate.popup_appearing) nk_edit_focus(ctx, 0);
  nk_flags ret = nk_tab_edit_string_zero_terminated(ctx,
      (vkdt.wstate.popup_appearing ? NK_EDIT_AUTO_SELECT : 0)|
      NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, filter, 256, nk_filter_default);
  if(ret & NK_EDIT_COMMITED) ok = 1;
  struct nk_rect rowbound = nk_layout_widget_bounds(ctx);
  total_space.h -= rowbound.y - total_space.y;
  nk_label(ctx, "filter", NK_TEXT_LEFT);
  vkdt.wstate.popup_appearing = 0;

  if(!ent_cnt)
  { // open directory
    if(dir)
    {
      void *dirp = dt_res_opendir(dir, 1);
      ent_cnt = 0;
      const char *basename = 0;
      if(dirp)
      { // first count valid entries
        snprintf(dirname, sizeof(dirname), "%s", dir);
        while((basename = dt_res_next_basename(dirp, 1)))
          ent_cnt++;
        if(ent_cnt)
        {
          dt_res_rewinddir(dirp, 1); // second pass actually record stuff
          ent = malloc(sizeof(ent[0])*(1+ent_cnt));
          ent[0] = fnbuf;
          ent_cnt = 0;
          while((basename = dt_res_next_basename(dirp, 1)))
          { // need to take a copy because on android basename is volatile
            int len = strlen(basename);
            if(ent[ent_cnt]+len >= fnbuf + sizeof(fnbuf)) break;
            strcpy(ent[ent_cnt], basename);
            ent[ent_cnt+1] = ent[ent_cnt] + len + 1;
            ent_cnt++;
          }
        }
        sort(ent, ent_cnt, sizeof(ent[0]), dt_filteredlist_compare, 0);
        dt_res_closedir(dirp, 1);
      }
      if(ent_cnt && (flags & s_filteredlist_descr_any))
      {
        desc = (char**)malloc(sizeof(char*)*ent_cnt);
        for(int i=0;i<ent_cnt;i++)
          desc[i] = filteredlist_get_heading(dirname, ent[i]);
      }
    }
    if(dir_local)
    {
      void *dirp = dt_res_opendir(dir_local, 0);
      ent_local_cnt = 0;
      const char *basename = 0;
      if(dirp)
      { // first count valid entries
        snprintf(dirname_local, sizeof(dirname_local), "%s", dir_local);
        while((basename = dt_res_next_basename(dirp, 0)))
          ent_local_cnt++;
        if(ent_local_cnt)
        {
          dt_res_rewinddir(dirp, 0); // second pass actually record stuff
          ent_local = malloc(sizeof(ent_local[0])*(ent_local_cnt+1));
          ent_local_cnt = 0;
          ent_local[0] = ent ? ent[ent_cnt] : fnbuf;
          while((basename = dt_res_next_basename(dirp, 0)))
          {
            int len = strlen(basename);
            if(ent_local[ent_local_cnt]+len >= fnbuf + sizeof(fnbuf)) break;
            strcpy(ent_local[ent_local_cnt], basename);
            ent_local[ent_local_cnt+1] = ent_local[ent_local_cnt] + len + 1;
            ent_local_cnt++;
          }
        }
        sort(ent_local, ent_local_cnt, sizeof(ent_local[0]), dt_filteredlist_compare, 0);
        dt_res_closedir(dirp, 0);
      }
      if(ent_local_cnt && (flags & s_filteredlist_descr_any))
      {
        desc_local = (char**)malloc(sizeof(char*)*ent_local_cnt);
        for(int i=0;i<ent_local_cnt;i++)
          desc_local[i] = filteredlist_get_heading(dirname_local, ent_local[i]);
      }
    }
    else ent_local_cnt = 0;
  }

  nk_layout_row_dynamic(&vkdt.ctx, total_space.h-3*row_height, 1);
  nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
  nk_group_begin(&vkdt.ctx, "filteredlist-scrollpane", 0);
  {
    nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
#define XLIST(E, D, L) do { \
    for(int i=0;i<E##_cnt;i++)\
    if((strstr(E[i], filter) || (D && D[i] && strstr(D[i], filter)))\
        && E[i][0] != '.' && (!(flags & s_filteredlist_descr_req) || (D && D[i]))) {\
      if(pick < 0) { local = L; pick = i; } \
      if(nk_button_label(&vkdt.ctx, (D && D[i]) ? D[i] : E[i])) {\
        ok = 1; pick = i; local = L;\
      } } } while(0)
    XLIST(ent_local, desc_local, 1);
    XLIST(ent, desc, 0);
#undef XLIST
    nk_group_end(&vkdt.ctx);
  }
  nk_style_pop_flags(&vkdt.ctx);

  nk_layout_row_dynamic(&vkdt.ctx, row_height, 5);
  nk_label(&vkdt.ctx, "", NK_TEXT_LEFT);
  nk_label(&vkdt.ctx, "", NK_TEXT_LEFT);
  if( (flags & s_filteredlist_allow_new) && nk_button_label(&vkdt.ctx, "create new")) { pick = -1; ok = 1; }
  if(!(flags & s_filteredlist_allow_new)) nk_label(&vkdt.ctx, "", NK_TEXT_LEFT);
  if (nk_button_label(&vkdt.ctx, "cancel")) {FREE_ENT; return 2;}
  if (nk_button_label(&vkdt.ctx, "ok")) ok = 1;

  if(ok == 1)
  {
    if(flags & s_filteredlist_return_short)
    {
      if(pick < 0)   snprintf(retstr, retstr_len, "%.*s", retstr_len-1, filter);
      else if(local) snprintf(retstr, retstr_len, "%.*s", retstr_len-1, ent_local[pick]);
      else           snprintf(retstr, retstr_len, "%.*s", retstr_len-1, ent[pick]);
    }
    else
    {
      if(pick < 0)   snprintf(retstr, retstr_len, "%.*s", retstr_len-1, filter);
      else if(local) snprintf(retstr, retstr_len, "%.*s/%s", retstr_len-257, dirname_local, ent_local[pick]);
      else           snprintf(retstr, retstr_len, "%.*s/%s", retstr_len-257, dirname, ent[pick]);
    }
    FREE_ENT;
  }
  return ok;
}
#undef FREE_ENT
