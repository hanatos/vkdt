#pragma once
#include <stdint.h>

typedef struct rawimage_t
{
  char     maker[32];
  char     model[32];
  char     clean_maker[32];
  char     clean_model[32];
  uint64_t width;
  uint64_t height;
  uint64_t cpp;
  float    wb_coeffs[4];
  float    whitelevels[4];
  float    blacklevels[4];
  float    xyz_to_cam[4][3];
  uint32_t illuminant; // D65 D50 D55 D75 A or we don't know
  uint32_t filters;
  uint64_t crop_aabb[4];
  uint32_t orientation;
  float    iso;
  float    exposure;
  float    aperture;
  float    focal_length;
  char     datetime[32];
  uint8_t *dng_opcode_lists[3];
  uint32_t dng_opcode_lists_len[3];
  uint32_t data_type; // 0 means u16, 1 means f32
  uint32_t cfa_off_x;
  uint32_t cfa_off_y;
  uint32_t stride;
  void    *data;
} rawimage_t;

uint64_t rl_decode_file(
    const char *filename,
    rawimage_t *rawimg);

void rl_deallocate(
    void    *data,
    uint64_t size);
