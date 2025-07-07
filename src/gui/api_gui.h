#pragma once
#include "widget_filteredlist.h"
#include "widget_image.h"
#include "api.h"
#include "view.h"
#include "core/fs.h"
#include "pipe/modules/api.h"
#include "pipe/graph-io.h"
#include "pipe/graph-export.h"
#include "pipe/graph-history.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// api functions for gui interactions. these can be
// triggered by pressing buttons or by issuing hotkey combinations.
// thus, they cannot have parameters, but they can read
// global state variables, as well as assume some gui context.

static inline void
dt_gui_edit_hotkeys()
{
  vkdt.wstate.popup = s_popup_edit_hotkeys;
  vkdt.wstate.popup_appearing = 1;
  vkdt.wstate.busy += 5;
}

static inline void
dt_gui_lt_assign_tag()
{
  if(vkdt.db.selection_cnt <= 0)
  {
    dt_gui_notification("need to select at least one image to assign a tag!");
    return;
  }
  vkdt.wstate.popup = s_popup_assign_tag;
  vkdt.wstate.popup_appearing = 1;
  vkdt.wstate.busy += 5;
}

static inline void
dt_gui_lt_toggle_select_all()
{
  if(vkdt.db.selection_cnt > 0) // select none
    dt_db_selection_clear(&vkdt.db);
  else // select all
    for(uint32_t i=0;i<vkdt.db.collection_cnt;i++)
      dt_db_selection_add(&vkdt.db, i);
  dt_gui_notification("selected %d/%d images", vkdt.db.selection_cnt, vkdt.db.collection_cnt);
}

static inline void
dt_gui_copy_history()
{
  vkdt.wstate.copied_imgid = dt_db_current_imgid(&vkdt.db);
  dt_gui_notification("copied image history to clipboard");
}

static inline void
dt_gui_paste_history()
{
  if(vkdt.wstate.copied_imgid == -1u)
  {
    dt_gui_notification("need to copy first!");
    return;
  }
  // TODO: background job
  char filename[PATH_MAX];
  uint32_t cid = vkdt.wstate.copied_imgid;
  dt_db_image_path(&vkdt.db, cid, filename, sizeof(filename));
  char *src = fs_realpath(filename, 0);
  FILE *fin = fopen(src, "rb");
  if(!fin)
  { // also fails if src is 0
    dt_gui_notification("could not open %s!", src);
    return;
  }
  fseek(fin, 0, SEEK_END);
  size_t fsize = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  uint8_t *buf = (uint8_t*)malloc(fsize);
  fread(buf, fsize, 1, fin);
  fclose(fin);
  // this only works if the copied source is "simple", i.e. cfg corresponds to exactly
  // one input raw file that is appearing under param:i-raw:main:filename.
  // it then copies the history to the selected images, replacing their filenames in the config.
  char dst[PATH_MAX];
  const uint32_t *sel = dt_db_selection_get(&vkdt.db);
  for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
  {
    if(sel[i] == cid) continue; // don't copy to self
    dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
    fs_realpath(filename, dst);
    FILE *fout = fopen(dst, "wb");
    if(fout)
    {
      fwrite(buf, fsize, 1, fout);
      // replace (relative) image file name
      char *fn = fs_basename(dst);
      size_t len = strlen(fn);
      if(len > 4)
      {
        fn[len-=4] = 0; // cut off .cfg
        if(len > 4 && !strncasecmp(fn+len-4, ".mlv", 4))
          fprintf(fout, "param:i-mlv:main:filename:%s\n", fn);
        else if(len > 6 && !strncasecmp(fn+len-6, ".mcraw", 6))
          fprintf(fout, "param:i-mcraw:main:filename:%s\n", fn);
        else if(len > 4 && !strncasecmp(fn+len-4, ".pfm", 4))
          fprintf(fout, "param:i-pfm:main:filename:%s\n", fn);
        else if(len > 4 && !strncasecmp(fn+len-4, ".exr", 4))
          fprintf(fout, "param:i-exr:main:filename:%s\n", fn);
        else if(len > 4 && !strncasecmp(fn+len-4, ".lut", 4))
          fprintf(fout, "param:i-lut:main:filename:%s\n", fn);
        else if(len > 4 && !strncasecmp(fn+len-4, ".bc1", 4))
          fprintf(fout, "param:i-bc1:main:filename:%s\n", fn);
        else if(len > 4 && !strncasecmp(fn+len-4, ".jpg", 4))
          fprintf(fout, "param:i-jpg:main:filename:%s\n", fn);
        else
          fprintf(fout, "param:i-raw:main:filename:%s\n", fn);
        fn[len] = '.'; // put .cfg back
      }
      fclose(fout);
    }
  }
  free(buf);
  free(src);
  if(vkdt.view_mode == s_view_lighttable)
  {
    dt_thumbnails_cache_list(
        &vkdt.thumbnail_gen,
        &vkdt.db,
        sel, vkdt.db.selection_cnt,
        &glfwPostEmptyEvent);
  }
  else if(vkdt.view_mode == s_view_darkroom)
  {
    if(dt_graph_read_config_ascii(&vkdt.graph_dev, dst))
      dt_gui_notification("arrrggh!");
    dt_graph_history_reset(&vkdt.graph_dev);
    vkdt.graph_dev.runflags = s_graph_run_all;
  }
  dt_gui_notification("pasted image history");
}

static inline void
dt_gui_dr_preset_create()
{
  vkdt.wstate.popup = s_popup_create_preset;
  vkdt.wstate.popup_appearing = 1;
  vkdt.wstate.busy += 5;
}

static inline void
dt_gui_preset_apply()
{
  vkdt.wstate.popup = s_popup_apply_preset;
  vkdt.wstate.popup_appearing = 1;
  vkdt.wstate.busy += 5;
}

static inline void
dt_gui_dr_module_add()
{
  vkdt.wstate.popup = s_popup_add_module;
  vkdt.wstate.popup_appearing = 1;
  vkdt.wstate.busy += 5;
}

static inline void
dt_gui_dr_assign_tag()
{
  vkdt.wstate.popup = s_popup_assign_tag;
  vkdt.wstate.popup_appearing = 1;
  vkdt.wstate.busy += 5;
}

static inline void
dt_gui_dr_zoom()
{ // zoom 1:1
  // where does the mouse look in the current image?
  double x, y;
  dt_view_get_cursor_pos(vkdt.win.window, &x, &y);
  dt_image_zoom(&vkdt.wstate.img_widget, x, y);
}
