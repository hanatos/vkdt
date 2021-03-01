#include "modules/api.h"
#include "corona/prims.h"
#include "corona/geo.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct geo_t
{
  // XXX do we need this? do we load nra2 lists of geo?
  char filename[256];
  FILE *f;
  uint32_t vtx_cnt;
  uint32_t idx_cnt;

  prims_t prims;
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

  // XXX if f close it and deallocate prims

  // geo->f = dt_graph_open_resource(mod->graph, filename, "rb");
  // if(!geo->f) goto error;

  prims_allocate(&geo->prims, 1);
  if(prims_load_with_flags(&geo->prims, filename, "none", 0, 'r', mod->graph->searchpath))
    goto error;

  snprintf(geo->filename, sizeof(geo->filename), "%s", filename);
  return 0;
error:
  fprintf(stderr, "[i-geo] could not load file `%s'!\n", filename);
  return 1;
}

static int
read_plain(dt_module_t *mod, void *mapped)
{
  // TODO: implement this
#if 0
  geo_t *geo = mod->data;
  uint32_t vtx_cnt = mod->connector[0].roi.full_wd;
  uint32_t idx_cnt = mod->connector[0].roi.full_ht;
  float    *vtx = mapped;
  uint32_t *idx = mapped; idx += 4*vtx_cnt;
  // .. uv =, mat = 

  // XXX
#endif

  return 0;
}

int init(dt_module_t *mod)
{
  geo_t *geo = malloc(sizeof(*geo));
  memset(geo, 0, sizeof(*geo));
  prims_init(&geo->prims);
  mod->data = geo;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  geo_t *geo = mod->data;
  if(geo->filename[0])
  {
    // if(geo->f) fclose(geo->f);
    geo->filename[0] = 0;
  }
  prims_cleanup(&geo->prims);
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
  uint32_t vtx_cnt = prims_get_shape_vtx_cnt(&geo->prims, 0);
  uint32_t tri_cnt = geo->prims.shape[0].num_prims;
  uint32_t idx_cnt = tri_cnt * 3;
  mod->connector[0].roi.scale = 1;
  mod->connector[0].roi.wd = mod->connector[0].roi.full_wd = vtx_cnt;
  mod->connector[0].roi.ht = mod->connector[0].roi.full_ht = idx_cnt;
  fprintf(stderr, "[i-geo] got %u vtx %u idx\n", vtx_cnt, idx_cnt);
}

// extra callback for rt accel struct build
int read_geo(
    dt_module_t *mod,
    float       *vtx,
    uint32_t    *idx)
{
  geo_t *geo = mod->data;
  uint32_t vtx_cnt = mod->connector[0].roi.full_wd;
  uint32_t idx_cnt = mod->connector[0].roi.full_ht;
  uint32_t prm_cnt = geo->prims.shape[0].num_prims;
  for(uint32_t i=0,p=0;i<idx_cnt&&p<prm_cnt;p++)
  {
    // TODO: should check mb and vcnt on this primitive.
    uint32_t vi = geo->prims.shape[0].primid[p].vi;
    idx[i++] = geo->prims.shape[0].vtxidx[vi+0].v;
    idx[i++] = geo->prims.shape[0].vtxidx[vi+1].v;
    idx[i++] = geo->prims.shape[0].vtxidx[vi+2].v;
  }
  for(uint32_t v=0;v<vtx_cnt;v++)
  {
    vtx[3*v+0] = geo->prims.shape[0].vtx[v].v[0];
    vtx[3*v+1] = geo->prims.shape[0].vtx[v].v[1];
    vtx[3*v+2] = geo->prims.shape[0].vtx[v].v[2];
  }
  // memcpy(vtx, geo->prims.shape[0].vtx, sizeof(float)*3*vtx_cnt);
  return 0;
}

// callback for geometry in our format to access in glsl
int read_source(
    dt_module_t *mod,
    void        *mapped)
{
  // connector has format = "geo", chan = "ssbo"
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename)) return 1;
  return read_plain(mod, mapped);
}
