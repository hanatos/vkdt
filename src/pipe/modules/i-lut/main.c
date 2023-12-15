#include "modules/api.h"
#include "core/strexpand.h"
#include "core/lut.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct lutinput_buf_t
{
  char filename[PATH_MAX];
  char errormsg[256];
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
  // fprintf(stderr, "[i-lut] %"PRItkn" loading `%s'!\n", dt_token_str(mod->inst), filename);

  if(lut && !strcmp(lut->filename, filename))
    return 0; // already loaded
  assert(lut); // this should be inited in init()

  lut->f = dt_graph_open_resource(mod->graph, 0, filename, "rb");
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
  fprintf(stderr, "[i-lut] %"PRItkn" could not load file `%s'!\n", dt_token_str(mod->inst), filename);
  snprintf(lut->errormsg, sizeof(lut->errormsg), "i-lut: %"PRItkn" could not load file `%s'!\n", dt_token_str(mod->inst), filename);
  mod->graph->gui_msg = lut->errormsg;
  lut->filename[0] = 0;
  return 1;
}

static int
read_plain(
    lutinput_buf_t *lut,
    void           *out)
{
  if(!lut->f) return 1;
  fseek(lut->f, lut->data_begin, SEEK_SET);
  int datatype = lut->header.datatype;
  if(datatype >= dt_lut_header_ssbo_f16) datatype -= dt_lut_header_ssbo_f16;
  size_t sz = datatype == dt_lut_header_f16 ? sizeof(uint16_t) : sizeof(float);
  fread(out, lut->header.wd*(uint64_t)lut->header.ht*(uint64_t)lut->header.channels, sz, lut->f);
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
  if(lut->header.channels == 1) mod->connector[0].chan   = dt_token("r");
  if(lut->header.channels == 2) mod->connector[0].chan   = dt_token("rg");
  if(lut->header.channels == 4) mod->connector[0].chan   = dt_token("rgba");
  int dtype = lut->header.datatype;
  if(dtype >= dt_lut_header_ssbo_f16)
  {
    mod->connector[0].chan = dt_token("ssbo");
    dtype -= dt_lut_header_ssbo_f16;
  }
  if(dtype == 0) mod->connector[0].format = dt_token("f16");
  if(dtype == 1) mod->connector[0].format = dt_token("f32");
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
