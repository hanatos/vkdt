#include "modules/api.h"
#include "modules/o-pfm/half.h"

#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>

typedef struct pfminput_buf_t
{
  char filename[PATH_MAX];
  uint32_t width, height;
  size_t data_begin;
  FILE *f;
}
pfminput_buf_t;

static int 
read_header(
    dt_module_t *mod,
    const char *filename)
{
  pfminput_buf_t *pfm = mod->data;
  if(pfm && !strcmp(pfm->filename, filename))
    return 0; // already loaded
  assert(pfm); // this should be inited in init()

  if(pfm->f) fclose(pfm->f);
  pfm->f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!pfm->f) goto error;

  int wd, ht;
  if(fscanf(pfm->f, "PF\n%d %d\n%*[^\n]", &wd, &ht) != 2)
  {
    fclose(pfm->f);
    goto error;
  }
  fgetc(pfm->f);

  pfm->width  = wd;
  pfm->height = ht;
  pfm->data_begin = ftell(pfm->f);

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.filters = 0;

  snprintf(pfm->filename, sizeof(pfm->filename), "%s", filename);
  return 0;
error:
  fprintf(stderr, "[i-pfm] could not load file `%s'!\n", filename);
  return 1;
}

static int
read_plain(
    pfminput_buf_t *pfm, uint16_t *out)
{
  fseek(pfm->f, pfm->data_begin, SEEK_SET);
  uint16_t one = float_to_half(1.0f);
  for(int64_t k=0;k<pfm->width*pfm->height;k++)
  {
    float in[3];
    fread(in, 3, sizeof(float), pfm->f);
    for(int i=0;i<3;i++) out[4*k+i] = float_to_half(in[i]);
    out[4*k+3] = one;
  }
  return 0;
}

int init(dt_module_t *mod)
{
  pfminput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  pfminput_buf_t *pfm = mod->data;
  if(pfm->filename[0])
  {
    if(pfm->f) fclose(pfm->f);
    pfm->filename[0] = 0;
  }
  free(pfm);
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
  pfminput_buf_t *pfm = mod->data;
  mod->connector[0].roi.full_wd = pfm->width;
  mod->connector[0].roi.full_ht = pfm->height;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename)) return 1;
  pfminput_buf_t *pfm = mod->data;
  return read_plain(pfm, mapped);
}
