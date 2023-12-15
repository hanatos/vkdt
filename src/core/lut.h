#pragma once
#include <stdint.h>

typedef struct dt_lut_header_t
{ // header of .lut files
  uint32_t magic;     // = 1234, very creative
  uint16_t version;   // = dt_lut_header_version, see below
  uint8_t  channels;  // number of channels
  uint8_t  datatype;  // 0 is half, 1 is 32 bit float
  uint32_t wd;        // size of the image
  uint32_t ht;
} // data pointer follows right after this in the file.
dt_lut_header_t;

static const int dt_lut_header_magic    = 1234;
static const int dt_lut_header_version  = 2;
static const int dt_lut_header_f16      = 0;
static const int dt_lut_header_f32      = 1;
static const int dt_lut_header_ssbo_f16 = 16;
static const int dt_lut_header_ssbo_f32 = 17;
