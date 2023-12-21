#include "modules/api.h"
#include "core/core.h"

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
    dt_module_t *mod,
    void *buf)
{
  const char *basename = dt_module_param_string(mod, 0);
  fprintf(stderr, "[o-exr] writing '%s'\n", basename);
  uint16_t *p16 = (uint16_t *)buf;

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
  float chromaticities[] = { 0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290};
  EXRAttribute custom_attributes[] = {{
    "chromaticities",
    "Chromaticities",
    (unsigned char*)chromaticities,
    sizeof(chromaticities),
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
