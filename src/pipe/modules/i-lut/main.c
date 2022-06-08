#include "modules/api.h"
#include "core/strexpand.h"
#include "core/lut.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct lutinput_buf_t
{
  char filename[PATH_MAX];
  dt_lut_header_t header;
  size_t data_begin;
  FILE *f;
}
lutinput_buf_t;

static int 
read_header(
    dt_module_t *mod,
    const char  *pattern)
{
  lutinput_buf_t *lut = mod->data;

  char filename[PATH_MAX];
  const char *key[] = { "maker", "model", 0};
  const char *val[] = { mod->graph->main_img_param.maker, mod->graph->main_img_param.model, 0};
  dt_strexpand(pattern, strlen(pattern), filename, sizeof(filename), key, val);
  // fprintf(stderr, "[i-lut] loading %s\n", filename);

  if(lut && !strcmp(lut->filename, filename))
    return 0; // already loaded
  assert(lut); // this should be inited in init()

  lut->f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!lut->f) goto error;

  if(fread(&lut->header, sizeof(dt_lut_header_t), 1, lut->f) != 1 || lut->header.version != 2)
  {
    fclose(lut->f);
    goto error;
  }

  lut->data_begin = ftell(lut->f);

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.filters = 0;

  snprintf(lut->filename, sizeof(lut->filename), "%s", filename);
  return 0;
error:
  fprintf(stderr, "[i-lut] could not load file `%s'!\n", filename);
  return 1;
}

static int
read_plain(
    lutinput_buf_t *lut,
    uint16_t       *out)
{
  fseek(lut->f, lut->data_begin, SEEK_SET);
  size_t sz = lut->header.datatype == dt_lut_header_f16 ? sizeof(uint16_t) : sizeof(float);
  fread(out, lut->header.wd*lut->header.ht*lut->header.channels, sz, lut->f);
  return 0;
}

int init(dt_module_t *mod)
{
  lutinput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  lutinput_buf_t *lut = mod->data;
  if(lut->filename[0])
  {
    if(lut->f) fclose(lut->f);
    lut->filename[0] = 0;
  }
  free(lut);
  mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
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
  lutinput_buf_t *lut = mod->data;
  // adjust output connector channels:
  if(lut->header.channels == 2) mod->connector[0].chan = dt_token("rg");
  if(lut->header.channels == 4) mod->connector[0].chan = dt_token("rgba");
  if(lut->header.datatype == 0) mod->connector[0].format = dt_token("f16");
  if(lut->header.datatype == 1) mod->connector[0].format = dt_token("f32");
  mod->connector[0].roi.full_wd = lut->header.wd;
  mod->connector[0].roi.full_ht = lut->header.ht;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename)) return 1;
  lutinput_buf_t *lut = mod->data;
  return read_plain(lut, mapped);
}
