#include "modules/api.h"
#include "pipe/geo.h"
#include "core/half.h"
#include "core/core.h"
#include "db/stringpool.h"
#include "obj.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct objinput_buf_t
{
  char       filename[PATH_MAX];
  uint32_t   frame;
  geo_tri_t *tri;
  uint32_t   tri_cnt;
}
objinput_buf_t;

static int 
read_obj(
    dt_module_t *mod,
    uint32_t     frame,
    const char  *filename)
{
  objinput_buf_t *obj = mod->data;
  if(obj && !strcmp(obj->filename, filename) &&
    (!strstr(filename, "%") || obj->frame == frame))
    return 0; // already loaded
  assert(obj); // this should be inited in init()
  if(obj->tri) free(obj->tri);
  obj->tri = 0;

  char triname[512], fname[512];
  snprintf(triname, sizeof(triname), "%s.tri", filename);
  if(!dt_graph_get_resource_filename(mod, triname, frame, fname, sizeof(fname)))
  { // found cache file
    FILE *f = fopen(fname, "rb");
    if(f)
    {
      uint32_t tri_cnt = 0;
      fread(&tri_cnt, sizeof(uint32_t), 1, f);
      obj->tri = malloc(sizeof(geo_tri_t)*tri_cnt);
      fread(obj->tri, sizeof(geo_tri_t), tri_cnt, f);
      obj->tri_cnt = tri_cnt;
      fclose(f);
    }
  }
  else
  {
    dt_stringpool_t sp;
    dt_stringpool_init(&sp, 200, 50); // 100 objects with avg 50 chars in their names (minus _base or _norm)

    const int pid = dt_module_get_param(mod->so, dt_token("texids"));
    const char *texfile = dt_module_param_string(mod, pid);
    if(!dt_graph_get_resource_filename(mod, texfile, frame, fname, sizeof(fname)))
    {
      FILE *tf = fopen(fname, "rb");
      if(tf)
      { // now init the string pool from our input text file
        char line[2048];
        while(fscanf(tf, "%[^\n]", line) != EOF)
        {
          char *colon = 0;
          if((colon = strstr(line, ":")))
          {
            *colon = 0;
            int tid = atoi(colon+1);
            dt_stringpool_get(&sp, line, strlen(line), tid, 0);
          }
          fgetc(tf); // eat newline
        }
        fclose(tf);
      }
    }
    if(dt_graph_get_resource_filename(mod, filename, frame, fname, sizeof(fname))) goto error;
    obj->tri = geo_obj_read(fname, &obj->tri_cnt, &sp);
    dt_stringpool_cleanup(&sp);
    snprintf(triname, sizeof(triname), "%s.tri", fname);
    FILE *f = fopen(triname, "wb");
    if(f)
    {
      fwrite(&obj->tri_cnt, sizeof(uint32_t), 1, f);
      fwrite(obj->tri, sizeof(geo_tri_t), obj->tri_cnt, f);
      fclose(f);
    }
  }
  if(!obj->tri) goto error;

  snprintf(obj->filename, sizeof(obj->filename), "%s", filename);
  obj->frame = frame;
  return 0;
error:
  fprintf(stderr, "[i-obj] could not load file `%s'!\n", filename);
  obj->filename[0] = 0;
  obj->frame = -1;
  return 1;
}

int init(dt_module_t *mod)
{
  objinput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  objinput_buf_t *obj = mod->data;
  if(obj->filename[0])
  {
    if(obj->tri) free(obj->tri);
    obj->tri = 0;
    obj->tri_cnt = 0;
    obj->filename[0] = 0;
  }
  free(obj);
  mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(strstr(filename, "%")) // reading a sequence of raws as a timelapse animation
    mod->flags = s_module_request_read_source;
  if(read_obj(mod, id+graph->frame, filename))
  { // XXX connect dummy data
    mod->connector[0].roi.full_wd = 1;
    mod->connector[0].roi.full_ht = 1;
    return;
  }
  objinput_buf_t *obj = mod->data;
  mod->connector[0].roi.full_wd = obj->tri_cnt;
  mod->connector[0].roi.full_ht = 1;
  // XXX TODO: something static allocation with enough triangle count?
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(read_obj(mod, mod->graph->frame+id, filename)) return 1;
  objinput_buf_t *obj = mod->data;
  // XXX need to crop output triangles to allocation size! (animated obj)
  fprintf(stderr, "[i-obj] uploading %d tris\n", obj->tri_cnt);
  memcpy(mapped, obj->tri, sizeof(geo_tri_t)*obj->tri_cnt);
  return 0;
}
