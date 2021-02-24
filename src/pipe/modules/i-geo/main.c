#include "modules/api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct geo_t
{
  char filename[256];
  FILE *f;
}
geo_t;

#if 0
static int 
read_header(
    dt_module_t *mod,
    const char  *filename)
{
  return 1;
#if 0
  pfminput_buf_t *pfm = mod->data;
  if(pfm && !strcmp(pfm->filename, filename))
    return 0; // already loaded
  assert(pfm); // this should be inited in init()

  pfm->f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!pfm->f) goto error;

  // XXX

  snprintf(pfm->filename, sizeof(pfm->filename), "%s", filename);
  return 0;
error:
  fprintf(stderr, "[i-pfm] could not load file `%s'!\n", filename);
  return 1;
#endif
}

static int
read_plain(geo_t *geo, uint16_t *out)
{
  // fseek(pfm->f, pfm->data_begin, SEEK_SET);
  // XXX

  return 0;
}
#endif

int init(dt_module_t *mod)
{
#if 0
  // XXX
  pfminput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
#endif
  return 0;
}

void cleanup(dt_module_t *mod)
{
#if 0
  if(!mod->data) return;
  pfminput_buf_t *pfm = mod->data;
  if(pfm->filename[0])
  {
    if(pfm->f) fclose(pfm->f);
    pfm->filename[0] = 0;
  }
  free(pfm);
  mod->data = 0;
#endif
}

#if 0
// XXX determine buffer size/full_wd,full_ht to fit our geometry
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename))
  {
    mod->connector[0].roi.full_wd = 32;
    mod->connector[0].roi.full_ht = 32;
    return;
  }
  pfminput_buf_t *pfm = mod->data;
  mod->connector[0].roi.full_wd = pfm->width;
  mod->connector[0].roi.full_ht = pfm->height;
}
#endif

// TODO: need extra callback:
int read_geo(
    dt_module_t *mod,
    void *vtx,
    void *idx)
{
  // connector has chan = "rtgeo", format = "vtx" ? or chan = "rtvtx"|"rtidx" and reserve format for quantised storage?
  return 1;
}
// XXX alternatively move read_source to nodes (should do anyways to allow multiple per module)

#if 0
int read_source(
    dt_module_t *mod,
    void *mapped)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename)) return 1;
  pfminput_buf_t *pfm = mod->data;
  return read_plain(pfm, mapped);
}
#endif
