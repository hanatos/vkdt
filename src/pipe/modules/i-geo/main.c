#include "modules/api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct geo_t
{
  char filename[256];
  FILE *f;
  uint32_t vtx_cnt;
  uint32_t idx_cnt;
}
geo_t;

static int 
read_header(
    dt_module_t *mod,
    const char  *filename)
{
  geo_t *geo = mod->data;
  if(geo && !strcmp(geo->filename, filename))
    return 0; // already loaded

  geo->f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!geo->f) goto error;

  // XXX

  snprintf(geo->filename, sizeof(geo->filename), "%s", filename);
  return 0;
error:
  fprintf(stderr, "[i-geo] could not load file `%s'!\n", filename);
  return 1;
}

static int
read_plain(dt_module_t *mod, void *mapped)
{
  geo_t *geo = mod->data;
  uint32_t vtx_cnt = mod->connector[0].roi.full_wd;
  uint32_t idx_cnt = mod->connector[0].roi.full_ht;
  float    *vtx = mapped;
  uint32_t *idx = mapped; idx += 4*vtx_cnt;
  // .. uv =, mat = 

  // XXX

  return 0;
}

int init(dt_module_t *mod)
{
  geo_t *geo = malloc(sizeof(*geo));
  memset(geo, 0, sizeof(*geo));
  mod->data = geo;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  geo_t *geo = mod->data;
  if(geo->filename[0])
  {
    if(geo->f) fclose(geo->f);
    geo->filename[0] = 0;
  }
  free(geo);
  mod->data = 0;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename))
  { // one triangle:
    mod->connector[0].roi.full_wd = 3;
    mod->connector[0].roi.full_ht = 3;
    return;
  }
  geo_t *geo = mod->data;
  mod->connector[0].roi.full_wd = geo->vtx_cnt;
  mod->connector[0].roi.full_ht = geo->idx_cnt;
}

// extra callback for rt accel struct build
int read_geo(
    dt_module_t *mod,
    void *vtx,
    void *idx)
{
  // connector has chan = "geo", format = "ssbo"
  return 1;
}

// callback for geometry in our format to access in glsl
int read_source(
    dt_module_t *mod,
    void        *mapped)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename)) return 1;
  return read_plain(mod, mapped);
}
