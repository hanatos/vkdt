#include "modules/api.h"
#include "core/core.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct pfminput_buf_t
{
  char filename[PATH_MAX];
  uint32_t frame;
  uint32_t width, height;
  size_t data_begin;
  FILE *f;
  int channels;
}
pfminput_buf_t;

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 2 || parid == 3) // noise model
  {
    const float noise_a = dt_module_param_float(module, 2)[0];
    const float noise_b = dt_module_param_float(module, 3)[0];
    module->img_param.noise_a = noise_a;
    module->img_param.noise_b = noise_b;
    return s_graph_run_all; // need no do modify_roi_out again to read noise model from file
  }
  return s_graph_run_record_cmd_buf;
}

static int 
read_header(
    dt_module_t *mod,
    uint32_t    frame,
    const char *filename)
{
  pfminput_buf_t *pfm = mod->data;
  if(pfm && !strcmp(pfm->filename, filename) && pfm->frame == frame)
    return 0; // already loaded
  assert(pfm); // this should be inited in init()

  pfm->channels = 3;
  if(pfm->f) fclose(pfm->f);
  pfm->f = dt_graph_open_resource(mod->graph, frame, filename, "rb");
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
    mod->img_param.white[k]        = 65535.0f; // XXX should probably be fixed in the denoise module instead
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.crop_aabb[0] = 0;
  mod->img_param.crop_aabb[1] = 0;
  mod->img_param.crop_aabb[2] = wd;
  mod->img_param.crop_aabb[3] = ht;
  mod->img_param.noise_a = dt_module_param_float(mod, 2)[0];
  mod->img_param.noise_b = dt_module_param_float(mod, 3)[0];
  mod->img_param.filters = 0;
  mod->img_param.colour_primaries = s_colour_primaries_2020;
  mod->img_param.colour_trc       = s_colour_trc_linear;

  snprintf(pfm->filename, sizeof(pfm->filename), "%s", filename);
  pfm->frame = frame;
  return 0;
error:
  fprintf(stderr, "[i-pfm] could not load file `%s'!\n", filename);
  pfm->filename[0] = 0;
  pfm->frame = -1;
  return 1;
}

static int
read_plain(
    pfminput_buf_t *pfm, float *out)
{
  fseek(pfm->f, pfm->data_begin, SEEK_SET);
  const int stride = pfm->channels == 1 ? 1 : 4;
  for(int64_t k=0;k<pfm->width*(int64_t)pfm->height;k++)
  {
    float in[3];
    fread(in, pfm->channels, sizeof(float), pfm->f);
    for(int i=0;i<pfm->channels;i++) out[stride*k+i] = in[i];
    if(stride == 4) out[stride*k+3] = 1.0;
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
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(strstr(filename, "%")) // reading a sequence of raws as a timelapse animation
    mod->flags = s_module_request_read_source;
  if(read_header(mod, id+graph->frame, filename))
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
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, mod->graph->frame+id, filename)) return 1;
  pfminput_buf_t *pfm = mod->data;
  return read_plain(pfm, mapped);
}
