#include "modules/api.h"
#include "core/half.h"
#include "core/core.h"
#include "core/colour.h"

#include <zlib.h>
#define TINYEXR_USE_THREAD (1)
#define TINYEXR_USE_OPENMP (0)
#define TINYEXR_USE_MINIZ (0)
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
extern "C" {

typedef struct exrinput_buf_t
{
  char filename[PATH_MAX];
  uint32_t frame;
  EXRImage  img;
  EXRHeader hdr;
}
exrinput_buf_t;

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
  if(parid == 4 || parid == 5) // colour space
  {
    const int prim = dt_module_param_int(module, 4)[0];
    const int trc  = dt_module_param_int(module, 5)[0];
    module->img_param.colour_primaries = prim;
    module->img_param.colour_trc       = trc;
    return s_graph_run_all; // propagate image params and potentially different buffer sizes downwards
  }
  return s_graph_run_record_cmd_buf;
}

static int 
read_header(
    dt_module_t *mod,
    uint32_t     frame,
    const char  *filename)
{
  exrinput_buf_t *exr = (exrinput_buf_t*)mod->data;
  if(exr && !strcmp(exr->filename, filename) && exr->frame == frame)
    return 0; // already loaded

  FreeEXRHeader(&exr->hdr);
  FreeEXRImage (&exr->img);
  memset(&exr->hdr, 0, sizeof(exr->hdr));
  memset(&exr->img, 0, sizeof(exr->img));

  char fname[2*PATH_MAX+10];
  if(dt_graph_get_resource_filename(mod, filename, frame, fname, sizeof(fname)))
    return 1;

  float *chroma = 0;
  const char *trc = 0;
  const char *err = 0;
  EXRVersion exr_version;
  {
    if(ParseEXRVersionFromFile(&exr_version, fname) != TINYEXR_SUCCESS)
      goto error;

    if (exr_version.multipart || exr_version.non_image)
      goto error;
  }
  InitEXRHeader(&exr->hdr);
  if(ParseEXRHeaderFromFile(&exr->hdr, &exr_version, fname, &err) != TINYEXR_SUCCESS)
  {
    fprintf(stderr, "[i-exr] error loading %s: %s\n", fname, err);
    FreeEXRErrorMessage(err);
    goto error;
  }
  if(exr->hdr.pixel_types[0] == TINYEXR_PIXELTYPE_HALF)
    mod->connector[0].format = dt_token("f16");
  else if(exr->hdr.pixel_types[0] == TINYEXR_PIXELTYPE_FLOAT)
    mod->connector[0].format = dt_token("f32");
  else if(exr->hdr.pixel_types[0] == TINYEXR_PIXELTYPE_UINT)
    mod->connector[0].format = dt_token("ui32");

  mod->img_param.colour_primaries = s_colour_primaries_2020;
  mod->img_param.colour_trc       = s_colour_trc_linear;

  for(int i=0;i<exr->hdr.num_custom_attributes;i++)
  {
    if(!strcmp(exr->hdr.custom_attributes[i].name, "chromaticities") && exr->hdr.custom_attributes[i].size == 32)
      chroma  = (float *)exr->hdr.custom_attributes[i].value;
    if(!strcmp(exr->hdr.custom_attributes[i].name, "trc") && !strcmp(exr->hdr.custom_attributes[i].type, "char"))
      trc = (const char *)exr->hdr.custom_attributes[i].value;
  }

  if(chroma)
  {
    chromaticities_to_matrix(mod->img_param.cam_to_rec2020, chroma, chroma+2, chroma+4, chroma+6);
    mod->img_param.colour_primaries = s_colour_primaries_custom;
  }
  if(trc)
  {
    const char *names[] = {"linear", "bt709", "sRGB", "PQ", "DCI", "HLG", "gamma"};
    for(int i=0;i<sizeof(names)/sizeof(names[0]);i++)
      if(!strcmp(trc, names[i])) mod->img_param.colour_trc = i;
  }

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 65535.0f; // XXX should probably be fixed in the denoise module instead
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.crop_aabb[0] = 0;
  mod->img_param.crop_aabb[1] = 0;
  mod->img_param.crop_aabb[2] = exr->hdr.data_window.max_x - exr->hdr.data_window.min_x + 1;
  mod->img_param.crop_aabb[3] = exr->hdr.data_window.max_y - exr->hdr.data_window.min_y + 1;
  mod->img_param.noise_a = dt_module_param_float(mod, 2)[0];
  mod->img_param.noise_b = dt_module_param_float(mod, 3)[0];
  mod->img_param.filters = 0;

  snprintf(exr->filename, sizeof(exr->filename), "%s", filename);
  exr->frame = frame;
  return 0;
error:
  FreeEXRHeader(&exr->hdr);
  FreeEXRImage (&exr->img);
  memset(&exr->hdr, 0, sizeof(exr->hdr));
  memset(&exr->img, 0, sizeof(exr->img));
  fprintf(stderr, "[i-exr] could not load file `%s'!\n", fname);
  exr->filename[0] = 0;
  exr->frame = -1;
  return 1;
}

static int get_cid(EXRHeader *h, int c)
{
  switch(h->channels[c].name[0])
  {
    case 'R': return 0;
    case 'G': return 1;
    case 'B': return 2;
    case 'A': return 3;
    default: return c;
  }
}



static int
read_plain(
    dt_module_t    *mod,
    exrinput_buf_t *exr,
    void           *out)
{
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  char fname[2*PATH_MAX+10];
  if(dt_graph_get_resource_filename(mod, filename, id+mod->graph->frame, fname, sizeof(fname)))
    return 1;
  if(LoadEXRImageFromFile(&exr->img, &exr->hdr, fname, 0) < 0)
    return 1;

  uint16_t *out_f16 = (uint16_t *)out;
  float    *out_f32 = (float    *)out;
  uint32_t *out_u32 = (uint32_t *)out;
  uint8_t  *out_u8  = (uint8_t  *)out;
  uint16_t one = float_to_half(1.0f);
  const int wd = mod->connector[0].roi.wd;
  const int ht = mod->connector[0].roi.ht;

  int pxs = 0;
  if(exr->hdr.pixel_types[0] == TINYEXR_PIXELTYPE_HALF)
  {
    pxs = sizeof(uint16_t);
    for (int y = 0; y < ht; y++) for (int x = 0; x < wd; x++) out_f16[4*(y * wd + x)+3] = one; // opaque alpha
  }
  if(exr->hdr.pixel_types[0] == TINYEXR_PIXELTYPE_FLOAT)
  {
    pxs = sizeof(float);
    for (int y = 0; y < ht; y++) for (int x = 0; x < wd; x++) out_f32[4*(y * wd + x)+3] = 1.0f;
  }
  else if(exr->hdr.pixel_types[0] == TINYEXR_PIXELTYPE_UINT)
  {
    pxs = sizeof(uint32_t);
    for (int y = 0; y < ht; y++) for (int x = 0; x < wd; x++) out_u32[4*(y * wd + x)+3] = -1u;
  }
  if(exr->img.tiles)
  {
    const EXRTile *tiles = exr->img.tiles;
    int num_tiles = exr->img.num_tiles;
    for (int tile_idx = 0; tile_idx < num_tiles; tile_idx++)
    {
      int sx = tiles[tile_idx].offset_x * exr->hdr.tile_size_x;
      int sy = tiles[tile_idx].offset_y * exr->hdr.tile_size_y;
      int ex = tiles[tile_idx].offset_x * exr->hdr.tile_size_x + tiles[tile_idx].width;
      int ey = tiles[tile_idx].offset_y * exr->hdr.tile_size_y + tiles[tile_idx].height;

      for (int c = 0; c < exr->hdr.num_channels; c++)
      {
        int co = get_cid(&exr->hdr, c);
        const uint8_t *src = (const uint8_t *)(exr->img.tiles[tile_idx].images[c]);
        for (int y = 0; y < ey - sy; y++)
          for (int x = 0; x < ex - sx; x++)
            memcpy(out_u8 + pxs*(4*((y + sy) * wd + (x + sx))+co), 
                src + pxs*(y * exr->hdr.tile_size_x + x), pxs);
      }
    }
  }
  else if(exr->img.images)
  {
    for (int c = 0; c < exr->hdr.num_channels; c++)
    {
      int co = get_cid(&exr->hdr, c);
      const uint8_t *src = (const uint8_t *)(exr->img.images[c]);
      for (int y = 0; y < ht; y++)
        for (int x = 0; x < wd; x++)
          memcpy(out_u8 + pxs*(4*(y * wd + x)+co), src + pxs*(y * wd + x), pxs);
    }
  }
  FreeEXRImage(&exr->img);
  memset(&exr->img, 0, sizeof(exr->img));
  return 0;
}

int init(dt_module_t *mod)
{
  exrinput_buf_t *dat = (exrinput_buf_t *)malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  exrinput_buf_t *exr = (exrinput_buf_t *)mod->data;
  if(exr->filename[0])
  {
    FreeEXRImage (&exr->img);
    FreeEXRHeader(&exr->hdr);
    memset(&exr->img, 0, sizeof(exr->img));
    memset(&exr->hdr, 0, sizeof(exr->hdr));
    exr->filename[0] = 0;
    exr->frame = -1;
  }
  free(exr);
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
  exrinput_buf_t *exr = (exrinput_buf_t *)mod->data;
  mod->connector[0].chan = exr->hdr.num_channels == 1 ? dt_token("y") : dt_token("rgba");
  mod->connector[0].roi.full_wd = exr->hdr.data_window.max_x - exr->hdr.data_window.min_x + 1;
  mod->connector[0].roi.full_ht = exr->hdr.data_window.max_y - exr->hdr.data_window.min_y + 1;

  int *p_prim = (int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("prim")));
  int *p_trc  = (int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("trc" )));
  // don't overwrite user choice, but if they don't know/leave at defaults we do:
  if(*p_prim == s_colour_primaries_unknown) *p_prim = mod->img_param.colour_primaries;
  if(*p_trc  == s_colour_trc_unknown)       *p_trc  = mod->img_param.colour_trc;
  mod->img_param.colour_primaries = *p_prim;
  mod->img_param.colour_trc       = *p_trc;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const int   id       = dt_module_param_int(mod, 1)[0];
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, mod->graph->frame+id, filename)) return 1;
  exrinput_buf_t *exr = (exrinput_buf_t *)mod->data;
  return read_plain(mod, exr, mapped);
}
}
