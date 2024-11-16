#include "modules/api.h"
#include "core/core.h"
#include "rgbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "rgbe.c"

typedef struct hdrinput_buf_t
{
  char filename[PATH_MAX];
  uint32_t frame;
  uint32_t width, height;
  size_t data_begin;
  FILE *f;
}
hdrinput_buf_t;

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
  hdrinput_buf_t *hdr = mod->data;
  if(hdr && !strcmp(hdr->filename, filename) && hdr->frame == frame)
    return 0; // already loaded
  assert(hdr); // this should be inited in init()

  if(hdr->f) fclose(hdr->f);
  hdr->f = dt_graph_open_resource(mod->graph, frame, filename, "rb");
  if(!hdr->f) goto error;

  int wd, ht;
  rgbe_header_info info;
  if(RGBE_ReadHeader(hdr->f, &wd, &ht, &info)) goto error;
  hdr->width  = wd;
  hdr->height = ht;
  hdr->data_begin = ftell(hdr->f);

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
  mod->img_param.colour_primaries  = s_colour_primaries_2020; // hmm would we get sRGB here?
  mod->img_param.colour_trc        = s_colour_trc_linear;
  mod->img_param.cam_to_rec2020[0] = 0.0f/0.0f;

  snprintf(hdr->filename, sizeof(hdr->filename), "%s", filename);
  hdr->frame = frame;
  return 0;
error:
  fprintf(stderr, "[i-hdr] could not load file `%s'!\n", filename);
  hdr->filename[0] = 0;
  hdr->frame = -1;
  return 1;
}

static int
read_plain(
    hdrinput_buf_t *hdr, float *out)
{
  fseek(hdr->f, hdr->data_begin, SEEK_SET);
  return RGBE_ReadPixels_RLE(hdr->f, out, hdr->width, hdr->height);
}

int init(dt_module_t *mod)
{
  hdrinput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  hdrinput_buf_t *hdr = mod->data;
  if(hdr->filename[0])
  {
    if(hdr->f) fclose(hdr->f);
    hdr->filename[0] = 0;
  }
  free(hdr);
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
    mod->connector[0].roi.full_wd = 32;
    mod->connector[0].roi.full_ht = 32;
    return;
  }
  hdrinput_buf_t *hdr = mod->data;
  mod->connector[0].roi.full_wd = hdr->width;
  mod->connector[0].roi.full_ht = hdr->height;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, mod->graph->frame+id, filename)) return 1;
  hdrinput_buf_t *hdr = mod->data;
  return read_plain(hdr, mapped);
}
