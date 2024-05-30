#pragma once
#include "api_gui.h"
static inline int
recently_used_collections()
{
  int32_t num = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/ruc_num", 0), 0, 10);
  for(int i=0;i<num;i++)
  {
    char entry[512];
    snprintf(entry, sizeof(entry), "gui/ruc_entry%02d", i);
    const char *dir = dt_rc_get(&vkdt.rc, entry, "null");
    if(strcmp(dir, "null"))
    {
      const char *last = dir;
      for(const char *c=dir;*c!=0;c++) if(*c=='/') last = c+1;
      nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
      dt_tooltip(dir);
      if(nk_button_text(&vkdt.ctx, last, strlen(last)))
      {
        dt_gui_switch_collection(dir);
        return 1; // return immediately since switching collections invalidates dir (by sorting/compacting the gui/ruc_num entries)
      }
      nk_style_pop_flags(&vkdt.ctx);
    }
  }
  return 0;
}
