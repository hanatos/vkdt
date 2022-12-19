#include "modules/api.h"
#include "core/half.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct pfminput_buf_t
{
  char filename[PATH_MAX];
  uint32_t width, height;
  size_t data_begin;
  FILE *f;
  int channels;
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

  pfm->channels = 3;
  if(pfm->f) fclose(pfm->f);
  pfm->f = dt_graph_open_resource(mod->graph, 0, filename, "rb");
  if(!pfm->f) goto error;

  int wd, ht;
  if(fscanf(pfm->f, "PF\n%d %d\n%*[^\n]", &wd, &ht) != 2)
  {
    if(fscanf(pfm->f, "f\n%d %d\n%*[^\n]", &wd, &ht) != 2)
    {
      fclose(pfm->f);
      goto error;
    }
    pfm->channels = 1;
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
  const int stride = pfm->channels == 1 ? 1 : 4;
  for(int64_t k=0;k<pfm->width*pfm->height;k++)
  {
    float in[3];
    fread(in, pfm->channels, sizeof(float), pfm->f);
    for(int i=0;i<pfm->channels;i++) out[stride*k+i] = float_to_half(in[i]);
    if(stride == 4) out[stride*k+3] = one;
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
    mod->connector[0].chan = dt_token("rgba");
    mod->connector[0].roi.full_wd = 32;
    mod->connector[0].roi.full_ht = 32;
    return;
  }
  pfminput_buf_t *pfm = mod->data;
  mod->connector[0].chan = pfm->channels == 1 ? dt_token("y") : dt_token("rgba");
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
