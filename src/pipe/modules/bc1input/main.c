#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>

int init(dt_module_t *mod)
{
  // jpginput_buf_t *dat = malloc(sizeof(*dat));
  // memset(dat, 0, sizeof(*dat));
  // mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  // jpginput_buf_t *jpg = mod->data;
  // if(jpg->filename[0])
  // {
  //   if(jpg->f) fclose(jpg->f);
  //   jpg->filename[0] = 0;
  // }
  // free(jpg);
  // mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  // const char *filename = dt_module_param_string(mod, 0);
  // if(read_header(mod, filename)) return;
  // jpginput_buf_t *jpg = mod->data;
  // mod->connector[0].roi.full_wd = jpg->width;
  // mod->connector[0].roi.full_ht = jpg->height;
}

int read_source(
    dt_module_t *mod,
    void *mapped)
{
  // const char *filename = dt_module_param_string(mod, 0);
  // if(read_header(mod, filename)) return 1;
  // jpginput_buf_t *jpg = mod->data;
  // jpeg_read(jpg, mapped);
  return 0;
}
