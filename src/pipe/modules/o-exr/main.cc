#include "modules/api.h"
#include "core/core.h"
#include "core/colour.h"

#include <zlib.h>
#define TINYEXR_USE_THREAD (1)
#define TINYEXR_USE_OPENMP (0)
#define TINYEXR_USE_MINIZ (0)
#define TINYEXR_IMPLEMENTATION
#include "../i-exr/tinyexr.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
extern "C" {

void write_sink(
    dt_module_t            *mod,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  const char *basename = dt_module_param_string(mod, 0);
  fprintf(stderr, "[o-exr] writing '%s'\n", basename);
  uint16_t *p16 = (uint16_t *)buf;
  const dt_image_params_t *img_param = dt_module_get_input_img_param(mod->graph, mod, dt_token("input"));
  if(!img_param) return;

  const int wd = mod->connector[0].roi.wd;
  const int ht = mod->connector[0].roi.ht;
  char filename[512];
  snprintf(filename, sizeof(filename), "%s.exr", basename);

  EXRImage img;
  EXRHeader hdr;
  InitEXRHeader(&hdr);
  InitEXRImage(&img);

  img.num_channels = 3;
  std::vector<uint16_t> images[3];
  images[0].resize(wd * ht);
  images[1].resize(wd * ht);
  images[2].resize(wd * ht);

  for (int i = 0; i < wd * ht; i++)
  {
    images[0][i] = p16[4*i+0];
    images[1][i] = p16[4*i+1];
    images[2][i] = p16[4*i+2];
  }

  uint16_t* image_ptr[3];
  image_ptr[0] = &(images[2].at(0));
  image_ptr[1] = &(images[1].at(0));
  image_ptr[2] = &(images[0].at(0));

  img.images = (unsigned char**)image_ptr;
  img.width  = wd;
  img.height = ht;

  hdr.num_channels = 3;
  hdr.channels = (EXRChannelInfo *)malloc(sizeof(EXRChannelInfo) * hdr.num_channels);
  strncpy(hdr.channels[0].name, "B", 255); hdr.channels[0].name[strlen("B")] = '\0';
  strncpy(hdr.channels[1].name, "G", 255); hdr.channels[1].name[strlen("G")] = '\0';
  strncpy(hdr.channels[2].name, "R", 255); hdr.channels[2].name[strlen("R")] = '\0';

  hdr.pixel_types = (int *)malloc(sizeof(int) * hdr.num_channels);
  hdr.requested_pixel_types = (int *)malloc(sizeof(int) * hdr.num_channels);
  for (int i = 0; i < hdr.num_channels; i++)
  {
    hdr.pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
    hdr.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
  }

  // rr gg bb ww assuming rec2020 D65
  float chromaticities[][8] = {
    { 0, }, // custom matrix, need to multiply rec2020_to_XYZ * cam_to_rec2020 * (1,0,0) and then convert to xy chromaticities etc
    { 0.6400, 0.3300, 0.3000, 0.6000, 0.1500, 0.0600, 0.3127, 0.3290}, // sRGB
    { 0.708,  0.292,  0.170,  0.797,  0.131,  0.046,  0.3127, 0.3290}, // rec2020
    { 0.6400, 0.3300, 0.2100, 0.7100, 0.1500, 0.0600, 0.3127, 0.3290}, // adobe rgb
    { 0.680,  0.320,  0.265,  0.690,  0.150,  0.060,  0.3127, 0.3290}, // P3 D65 (display)
    { 1.0,    0.0,    0.0,    1.0,    0.0,    0.0,    0.3333, 0.3333}, // XYZ illum E
    { 0.708,  0.292,  0.170,  0.797,  0.131,  0.046,  0.3127, 0.3290}, // unknown, assume rec2020
  };
  matrix_to_chromaticities(img_param->cam_to_rec2020,
      &chromaticities[0][0], &chromaticities[0][2], &chromaticities[0][4], &chromaticities[0][6]);
  const char *trc[] = {"linear", "bt709", "sRGB", "PQ", "DCI", "HLG", "gamma"};
  EXRAttribute custom_attributes[] = {{
    "chromaticities",
    "chromaticities",
    (unsigned char*)(chromaticities[CLAMP(img_param->colour_primaries, 0, 7)]),
    sizeof(chromaticities[0]),
  },{
    "trc", "char", (unsigned char*)trc[CLAMP(img_param->colour_trc, 0, 6)], sizeof(trc[CLAMP(img_param->colour_trc, 0, 6)]),
  }};
  hdr.custom_attributes = custom_attributes;
  hdr.num_custom_attributes = sizeof(custom_attributes)/sizeof(custom_attributes[0]);

  const char* err;
  int ret = SaveEXRImageToFile(&img, &hdr, filename, &err);
  if (ret != TINYEXR_SUCCESS)
    fprintf(stderr, "[o-exr] ERR: %s\n", err);
  free(hdr.channels);
  free(hdr.pixel_types);
  free(hdr.requested_pixel_types);
}

}
