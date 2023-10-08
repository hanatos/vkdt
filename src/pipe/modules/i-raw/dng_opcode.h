#pragma once

#include "module.h"
#include "lens_corr.h"

static uint32_t get_be_long(uint8_t **p)
{
  uint32_t int_val = ((*p)[0] << 24) | ((*p)[1] << 16) | ((*p)[2] << 8) | (*p)[3];
  *p += 4;
  return int_val;
}

static float get_be_float(uint8_t **p)
{
  uint32_t int_val = ((*p)[0] << 24) | ((*p)[1] << 16) | ((*p)[2] << 8) | (*p)[3];
  float float_val;
  for(int i = 0; i < 4; i++)
  {
    ((char *)&float_val)[i] = ((char *)&int_val)[i];
  }
  *p += 4;
  return float_val;
}

static double get_be_double(uint8_t **p)
{
  uint64_t int_val =
    ((uint64_t)(*p)[0] << 56) +
    ((uint64_t)(*p)[1] << 48) +
    ((uint64_t)(*p)[2] << 40) +
    ((uint64_t)(*p)[3] << 32) +
    ((uint64_t)(*p)[4] << 24) +
    ((uint64_t)(*p)[5] << 16) +
    ((uint64_t)(*p)[6] << 8) +
    ((uint64_t)(*p)[7]);

  double double_val;
  for(int i = 0; i < 8; i++)
  {
    ((char *)&double_val)[i] = ((char *)&int_val)[i];
  }
  *p += 8;
  return double_val;
}

void dt_dng_opcode_process_opcode_list_3(uint8_t *buf, uint32_t buf_size, dt_image_params_t *ip)
{
  dt_lens_corr_dng_t lc;

  uint8_t *p = buf;
  uint32_t count = get_be_long(&p);
  uint32_t offset = 4;
  while(count > 0)
  {
    uint32_t opcode_id = get_be_long(&p);
    uint32_t spec_ver = get_be_long(&p);
    uint32_t flags = get_be_long(&p);
    uint32_t param_size = get_be_long(&p);

    if(offset + 16 + param_size > buf_size)
    {
      return; // invalid opcode size
    }

    if(opcode_id == 1) // WarpRectilinear
    {
      uint32_t num_planes = get_be_long(&p);
      if(num_planes != 1 && num_planes != 3)
      {
        continue; // unsupported number of planes
      }
      for(int iplane = 0; iplane < num_planes; iplane++)
      {
        lc.warp_rect_coef[iplane].kr0 = get_be_double(&p);
        lc.warp_rect_coef[iplane].kr1 = get_be_double(&p);
        lc.warp_rect_coef[iplane].kr2 = get_be_double(&p);
        lc.warp_rect_coef[iplane].kr3 = get_be_double(&p);
        lc.warp_rect_coef[iplane].kt0 = get_be_double(&p);
        lc.warp_rect_coef[iplane].kt1 = get_be_double(&p);
      }
      if(num_planes == 1)
      {
        lc.warp_rect_coef[1] = lc.warp_rect_coef[0];
        lc.warp_rect_coef[2] = lc.warp_rect_coef[0];
      }
      lc.warp_rect_center_x = get_be_double(&p);
      lc.warp_rect_center_y = get_be_double(&p);

      lc.warp_rect_enabled = true;
    }

    offset += 16 + param_size;
    count--;
  }

  if(lc.warp_rect_enabled)
  {
    ip->lens_corr.type = s_lens_corr_dng;
    ip->lens_corr.params.dng = lc;
  }
}
