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
  typedef struct idx_t
  {
    // uint32_t mat :  8; // XXX possibly the order was wrong
    uint32_t vi;//  : 24;
    uint32_t st;
  }
  idx_t;
  geo_t *geo = mod->data;
  uint32_t vtx_cnt = mod->connector[0].roi.full_wd;
  uint32_t idx_cnt = mod->connector[0].roi.full_ht;
  idx_t *idx = mapped;
  uint32_t prm_cnt = geo->prims.shape[0].num_prims;
  // align to sizeof vertex:
  uint32_t vtx_off = (idx_cnt+1)/2;
  uint32_t i = 0;
  for(uint32_t p=0;i<idx_cnt&&p<prm_cnt;p++)
  {
    // TODO: should check mb!
    uint32_t vi   = geo->prims.shape[0].primid[p].vi;
    uint32_t vcnt = geo->prims.shape[0].primid[p].vcnt;
    const prims_vtxidx_t *vix = geo->prims.shape[0].vtxidx + vi;
    const int mat = geo->prims.shape[0].primid[p].shapeid; // TODO: translate to shader/material index
    if(vcnt >= 3)
    {
      idx[i++] = (idx_t) { /*.mat = mat, */vi = vtx_off + vix[0].v, .st = vix[0].uv };
      idx[i++] = (idx_t) { /*.mat = mat, */vi = vtx_off + vix[1].v, .st = vix[1].uv };
      idx[i++] = (idx_t) { /*.mat = mat, */vi = vtx_off + vix[2].v, .st = vix[2].uv };
    }
    if(vcnt == 4)
    {
      idx[i++] = (idx_t) { /*.mat = mat, */vi = vtx_off + vix[0].v, .st = vix[0].uv };
      idx[i++] = (idx_t) { /*.mat = mat, */vi = vtx_off + vix[2].v, .st = vix[2].uv };
      idx[i++] = (idx_t) { /*.mat = mat, */vi = vtx_off + vix[3].v, .st = vix[3].uv };
    }
  }
  // write all vertices and normals:
  memcpy(idx + 2*vtx_off, geo->prims.shape[0].vtx, sizeof(float)*4*vtx_cnt);
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
  uint32_t prm_cnt = geo->prims.shape[0].num_prims;
  uint32_t tri_cnt = 0;
  for(int p=0;p<prm_cnt;p++)
  {
    if(geo->prims.shape[0].primid[p].vcnt == 4) tri_cnt += 2;
    if(geo->prims.shape[0].primid[p].vcnt == 3) tri_cnt += 1;
  }
  uint32_t idx_cnt = tri_cnt * 3;
  mod->connector[0].roi.scale = 1;
  mod->connector[0].roi.wd = mod->connector[0].roi.full_wd = vtx_cnt;
  mod->connector[0].roi.ht = mod->connector[0].roi.full_ht = idx_cnt;
  // fprintf(stderr, "[i-geo] got %u vtx %u idx\n", vtx_cnt, idx_cnt);
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
  uint32_t i = 0;
  for(uint32_t p=0;i<idx_cnt&&p<prm_cnt;p++)
  {
    // TODO: should check mb!
    uint32_t vi   = geo->prims.shape[0].primid[p].vi;
    uint32_t vcnt = geo->prims.shape[0].primid[p].vcnt;
    if(vcnt >= 3)
    {
      idx[i++] = geo->prims.shape[0].vtxidx[vi+0].v;
      idx[i++] = geo->prims.shape[0].vtxidx[vi+1].v;
      idx[i++] = geo->prims.shape[0].vtxidx[vi+2].v;
    }
    if(vcnt == 4)
    {
      idx[i++] = geo->prims.shape[0].vtxidx[vi+0].v;
      idx[i++] = geo->prims.shape[0].vtxidx[vi+2].v;
      idx[i++] = geo->prims.shape[0].vtxidx[vi+3].v;
    }
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
