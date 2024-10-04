#pragma once
#include "api_gui.h"
#include "widget_thumbnail.h"
#include <ctype.h>

static inline void
recently_used_collections_parse(
    const char     *dir,
    dt_db_filter_t *ft,
    int             update_collection)
{
  char *end = 0;
  if(dir)
  { // find '&' in dir and replace by '\0', remember the position
    const int len = strlen(dir);
    for(int i=0;i<len;i++) if(dir[i] == '&') { end = (char *)(dir + i); break; }
    if(end) *end = 0;
  }

  if(update_collection)
  {
    vkdt.wstate.copied_imgid = -1u; // invalidate
    dt_thumbnails_cache_abort(&vkdt.thumbnail_gen); // this is essential since threads depend on db
    dt_db_cleanup(&vkdt.db);
    dt_db_init(&vkdt.db);
    QVKL(&qvk.queue[qvk.qid[s_queue_graphics]].mutex, vkQueueWaitIdle(qvk.queue[qvk.qid[s_queue_graphics]].queue));
    dt_db_load_directory(&vkdt.db, &vkdt.thumbnails, dir);
    dt_thumbnails_cache_collection(&vkdt.thumbnail_gen, &vkdt.db, &glfwPostEmptyEvent);
  }

  if(end)
  { // now set db filter based on stuff we parse behind &
    *end = '&'; // restore & (because the string was const, right..?)
    ft->active = 0;
    for(int i=0;i<s_prop_cnt;i++)
    {
      end++;
      char filter[20] = {0}, val[30] = {0}, ext[10] = {0};
      sscanf(end, "%[^:]:%[^&|]:%[^&]", filter, val, ext);
      if(!strcmp(filter, "all"))         ft->active = s_prop_none;
      if(!strcmp(filter, "filename"))    { ft->active |= 1<<s_prop_filename;   snprintf(ft->filename, sizeof(ft->filename), "%s", val); }
      if(!strcmp(filter, "rating"))      { ft->active |= 1<<s_prop_rating;     ft->rating = atol(val); ft->rating_cmp = !strcmp(ext, "==") ? 1 : !strcmp(ext, "<") ? 2 : 0; }
      if(!strcmp(filter, "labels"))      { ft->active |= 1<<s_prop_labels;     ft->labels = atol(val); }
      if(!strcmp(filter, "createdate"))  { ft->active |= 1<<s_prop_createdate; snprintf(ft->createdate, sizeof(ft->createdate), "%s", val); }
      if(!strcmp(filter, "filetype"))    { ft->active |= 1<<s_prop_filetype;   strncpy(dt_token_str(ft->filetype), val, 8); }
      while(*end != '&' && *end != 0) end++;
      if(*end == 0) break;
    }
    if(update_collection)
    {
      vkdt.db.collection_filter = *ft;
      dt_db_update_collection(&vkdt.db);
    }
  }
}

static inline void
recently_used_collections_draw(
    struct nk_rect  bounds,
    const char     *dir,
    dt_db_filter_t *f)
{
  const char *last = dir;
  for(const char *c=dir;*c!=0;c++) if(*c=='/') last = c+1;
  char str[100];
  int len = sizeof(str);
  char date[10] = {0}, desc[100];
  sscanf(last, "%8s_%99[^&]s", date, desc);
  char *c = str;
  int off;
  if(isdigit(date[0]) && isdigit(date[1]) && isdigit(date[2]) && isdigit(date[3]))
    off = snprintf(c, len, "%.4s %s", date, desc);
  else
  {
    int l = 0;
    for(;last[l]&&last[l]!='&';l++) {}
    off = snprintf(c, len, "%.*s", l, last);
  }
  c += off; len -= off;

  if(len > 0 && (f->active & (1<<s_prop_createdate)))
  {
    if(!strncmp(date, f->createdate, 4))
      off = snprintf(c, len, " %s", f->createdate+5);
    else
      off = snprintf(c, len, " %s", f->createdate);
    for(int i=0;c[i];i++) if(c[i] == ':') c[i] = ' ';
    c += off; len -= off;
  }
  if(len > 0 && (f->active & (1<<s_prop_filename)))
  {
    off = snprintf(c, len, " %s", f->filename);
    c += off; len -= off;
  }
  if(len > 0 && (f->active & (1<<s_prop_filetype)))
  {
    off = snprintf(c, len, " %"PRItkn, dt_token_str(f->filetype));
    c += off; len -= off;
  }
  float wd = vkdt.ctx.style.font->height;
  float pos = bounds.x + bounds.w;
  float ypos = bounds.y + vkdt.ctx.style.tab.padding.y;
  nk_draw_text(nk_window_get_canvas(&vkdt.ctx),
      (struct nk_rect){.x=bounds.x+vkdt.ctx.style.tab.padding.x, .w=bounds.w, .y=ypos, .h=wd},
      str, strlen(str),
      vkdt.ctx.style.font, nk_rgba(0,0,0,0), vkdt.style.colour[NK_COLOR_TEXT]);

  if((f->active & (1<<s_prop_rating)) && f->rating > 0)
  {
    pos -= f->rating * wd;
    dt_draw_rating(pos, ypos+wd*0.5, wd, f->rating);
    pos -= 1.5*wd;
    if(f->rating_cmp == 0)
      nk_draw_text(nk_window_get_canvas(&vkdt.ctx),
          (struct nk_rect){.x=pos, .w=bounds.w, .y=ypos, .h=wd},
          ">=", 2,
          vkdt.ctx.style.font, nk_rgba(0,0,0,0), vkdt.style.colour[NK_COLOR_TEXT]);
    if(f->rating_cmp == 2)
      nk_draw_text(nk_window_get_canvas(&vkdt.ctx),
          (struct nk_rect){.x=pos, .w=bounds.w, .y=ypos, .h=wd},
          "<", 1,
          vkdt.ctx.style.font, nk_rgba(0,0,0,0), vkdt.style.colour[NK_COLOR_TEXT]);
  }
  if((f->active & (1<<s_prop_labels)) && f->labels > 0)
  {
    for(int i=0;i<5;i++) if(f->labels & (1<<i)) pos -= wd;
    dt_draw_labels(pos, ypos+wd*0.5, wd, f->labels);
  }
}

static inline int
recently_used_collections()
{
  int32_t num = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/ruc_num", 0), 0, 10);
  dt_db_filter_t f;
  for(int i=0;i<num;i++)
  {
    char entry[512];
    snprintf(entry, sizeof(entry), "gui/ruc_entry%02d", i);
    const char *dir = dt_rc_get(&vkdt.rc, entry, "null");
    if(strcmp(dir, "null"))
    {
      recently_used_collections_parse(dir, &f, 0);
      dt_tooltip(dir);
      struct nk_rect bounds = nk_widget_bounds(&vkdt.ctx);
      if(nk_button_text(&vkdt.ctx, "", 0))
      {
        dt_gui_switch_collection(dir);
        nk_style_pop_flags(&vkdt.ctx);
        return 1; // return immediately since switching collections invalidates dir (by sorting/compacting the gui/ruc_num entries)
      }
      recently_used_collections_draw(bounds, dir, &f);
    }
  }
  return 0;
}
