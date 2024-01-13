#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dng_opcode.h"

static uint16_t
get_be_short(uint8_t **p)
{
  uint16_t short_val = ((*p)[0] << 8) | (*p)[1];
  *p += 2;
  return short_val;
}

static uint32_t
get_be_long(uint8_t **p)
{
  uint32_t int_val =
      ((*p)[0] << 24) | ((*p)[1] << 16) | ((*p)[2] << 8) | (*p)[3];
  *p += 4;
  return int_val;
}

static float
get_be_float(uint8_t **p)
{
  uint32_t int_val =
      ((*p)[0] << 24) | ((*p)[1] << 16) | ((*p)[2] << 8) | (*p)[3];
  float float_val;
  for(int i = 0; i < 4; i++)
  {
    ((char *)&float_val)[i] = ((char *)&int_val)[i];
  }
  *p += 4;
  return float_val;
}

static double
get_be_double(uint8_t **p)
{
  uint64_t int_val = ((uint64_t)(*p)[0] << 56) + ((uint64_t)(*p)[1] << 48) +
                     ((uint64_t)(*p)[2] << 40) + ((uint64_t)(*p)[3] << 32) +
                     ((uint64_t)(*p)[4] << 24) + ((uint64_t)(*p)[5] << 16) +
                     ((uint64_t)(*p)[6] << 8) + ((uint64_t)(*p)[7]);

  double double_val;
  for(int i = 0; i < 8; i++)
    ((char *)&double_val)[i] = ((char *)&int_val)[i];
  *p += 8;
  return double_val;
}

static void
decode_dng_region(dt_dng_region_t *region, uint8_t **p)
{
  region->top       = get_be_long(p);
  region->left      = get_be_long(p);
  region->bottom    = get_be_long(p);
  region->right     = get_be_long(p);
  region->plane     = get_be_long(p);
  region->planes    = get_be_long(p);
  region->row_pitch = get_be_long(p);
  region->col_pitch = get_be_long(p);
}

static int
decode_warp_rectilinear(void **out, uint8_t **p, uint32_t len)
{
  uint32_t count = get_be_long(p);
  if(len != 20 + count * 48)
    return 1;
  dt_dng_warp_rectilinear_t *wr =
      calloc(1, sizeof(dt_dng_warp_rectilinear_t) +
                    count * sizeof(dt_dng_warp_rectilinear_coef_t));
  *out      = wr;
  wr->count = count;
  for(int i = 0; i < count; i++)
  {
    for(int j = 0; j < 4; j++)
      wr->coefs[i].kr[j] = get_be_double(p);
    for(int j = 0; j < 2; j++)
      wr->coefs[i].kt[j] = get_be_double(p);
  }
  wr->center_x = get_be_double(p);
  wr->center_y = get_be_double(p);
  return 0;
}

static int
decode_warp_fisheye(void **out, uint8_t **p, uint32_t len)
{
  uint32_t count = get_be_long(p);
  if(len != 20 + count * 32)
    return 1;
  dt_dng_warp_fisheye_t *wr = calloc(
      1, sizeof(dt_dng_warp_fisheye_t) + count * sizeof(dt_dng_warp_fisheye_coef_t));
  *out      = wr;
  wr->count = count;
  for(int i = 0; i < count; i++)
  {
    for(int j = 0; j < 4; j++)
      wr->coefs[i].kr[j] = get_be_double(p);
  }
  wr->center_x = get_be_double(p);
  wr->center_y = get_be_double(p);
  return 0;
}

static int
decode_gain_map(void **out, uint8_t **p, uint32_t len)
{
  dt_dng_gain_map_t gm;
  decode_dng_region(&gm.region, p);
  gm.map_points_v  = get_be_long(p);
  gm.map_points_h  = get_be_long(p);
  gm.map_spacing_v = get_be_double(p);
  gm.map_spacing_h = get_be_double(p);
  gm.map_origin_v  = get_be_double(p);
  gm.map_origin_h  = get_be_double(p);
  gm.map_planes    = get_be_long(p);

  size_t count = gm.map_points_h * gm.map_points_v * gm.map_planes;
  if(len != 76 + count * sizeof(float))
    return 1;
  dt_dng_gain_map_t *pgm =
      calloc(1, sizeof(dt_dng_gain_map_t) + count * sizeof(float));
  memcpy(pgm, &gm, sizeof(dt_dng_gain_map_t));
  *out = pgm;
  for(int i = 0; i < count; i++)
    pgm->map_gain[i] = get_be_float(p);

  return 0;
}

static int
decode_fix_vignette_radial(void **out, uint8_t **p, uint32_t len)
{
  if(len != 56)
    return 1;
  dt_dng_fix_vignette_radial_t *fvr = calloc(1, sizeof(dt_dng_fix_vignette_radial_t));
  *out                           = fvr;
  for(int j = 0; j < 5; j++)
    fvr->k[j] = get_be_double(p);
  fvr->center_x = get_be_double(p);
  fvr->center_y = get_be_double(p);
  return 0;
}

static int
decode_fix_bad_pixels_constant(void **out, uint8_t **p, uint32_t len)
{
  if(len != 8)
    return 1;
  dt_dng_fix_bad_pixels_constant_t *fbpc =
      calloc(1, sizeof(dt_dng_fix_bad_pixels_constant_t));
  *out              = fbpc;
  fbpc->constant    = get_be_long(p);
  fbpc->bayer_phase = get_be_long(p);
  return 0;
}

static int
decode_fix_bad_pixels_list(void **out, uint8_t **p, uint32_t len)
{
  uint32_t bayer_phase     = get_be_long(p);
  uint32_t bad_point_count = get_be_long(p);
  uint32_t bad_rect_count  = get_be_long(p);
  if(len != 12 + bad_point_count * 8 + bad_rect_count * 16)
    return 1;
  dt_dng_fix_bad_pixels_list_t *fbpl = calloc(
      1, sizeof(dt_dng_fix_bad_pixels_list_t) +
             (bad_point_count * 2 + bad_rect_count * 4) * sizeof(uint32_t));
  *out                  = fbpl;
  fbpl->bayer_phase     = bayer_phase;
  fbpl->bad_point_count = bad_point_count;
  fbpl->bad_rect_count  = bad_rect_count;
  fbpl->points          = &fbpl->_data[0];
  fbpl->rects           = &fbpl->_data[bad_point_count * 2];
  for(int i = 0; i < 2 * bad_point_count + 4 * bad_rect_count; i++)
    fbpl->_data[i] = get_be_long(p);
  return 0;
}

static int
decode_trim_bounds(void **out, uint8_t **p, uint32_t len)
{
  if(len != 16)
    return 1;
  dt_dng_trim_bounds_t *tb = calloc(1, sizeof(dt_dng_trim_bounds_t));
  *out                  = tb;
  tb->top               = get_be_long(p);
  tb->left              = get_be_long(p);
  tb->bottom            = get_be_long(p);
  tb->right             = get_be_long(p);
  return 0;
}

static int
decode_map_table(void **out, uint8_t **p, uint32_t len)
{
  dt_dng_map_table_t mt;
  decode_dng_region(&mt.region, p);
  mt.table_size = get_be_long(p);
  if(len != 36 + 2 * mt.table_size)
    return 1;
  dt_dng_map_table_t *pmt =
      calloc(1, sizeof(dt_dng_map_table_t) + mt.table_size * sizeof(uint16_t));
  *out = pmt;
  memcpy(pmt, &mt, sizeof(dt_dng_map_table_t));
  for(int i = 0; i < mt.table_size; i++)
    pmt->table[i] = get_be_short(p);
  return 0;
}

static int
decode_map_polynomial(void **out, uint8_t **p, uint32_t len)
{
  dt_dng_map_polynomial_t mp;
  decode_dng_region(&mp.region, p);
  mp.degree = get_be_long(p);
  if(len != 36 + 8 * mp.degree)
    return 1;
  dt_dng_map_polynomial_t *pmp =
      calloc(1, sizeof(dt_dng_map_polynomial_t) + mp.degree * sizeof(double));
  *out = pmp;
  memcpy(pmp, &mp, sizeof(dt_dng_map_polynomial_t));
  for(int i = 0; i < mp.degree; i++)
    pmp->coefs[i] = get_be_double(p);
  return 0;
}

static int
decode_warp_rectilinear_2(void **out, uint8_t **p, uint32_t len)
{
  uint32_t count = get_be_long(p);
  if(len != 24 + count * 76)
    return 1;
  dt_dng_warp_rectilinear_2_t *wr =
      calloc(1, sizeof(dt_dng_warp_rectilinear_2_t) +
                    count * sizeof(dt_dng_warp_rectilinear_2_coef_t));
  *out      = wr;
  wr->count = count;
  for(int i = 0; i < count; i++)
  {
    for(int j = 0; j < 15; j++)
      wr->coefs[i].kr[j] = get_be_double(p);
    for(int j = 0; j < 2; j++)
      wr->coefs[i].kt[j] = get_be_double(p);
    wr->coefs[i].min_valid_radius = get_be_double(p);
    wr->coefs[i].max_valid_radius = get_be_double(p);
  }
  wr->center_x          = get_be_double(p);
  wr->center_y          = get_be_double(p);
  wr->reciprocal_radial = get_be_long(p);
  return 0;
}

static int
decode_opcode(dt_dng_opcode_t *op, uint8_t **p)
{
  uint32_t op_id = get_be_long(p);
  get_be_long(p);
  uint32_t flags      = get_be_long(p);
  uint32_t param_size = get_be_long(p);

  op->id           = op_id;
  op->optional     = (flags & 1) > 0;
  op->preview_skip = (flags & 2) > 0;

  switch(op_id)
  {
  case s_dngop_warp_rectilinear:
    return decode_warp_rectilinear(&op->data, p, param_size);
  case s_dngop_warp_fisheye:
    return decode_warp_fisheye(&op->data, p, param_size);
  case s_dngop_fix_vignette_radial:
    return decode_fix_vignette_radial(&op->data, p, param_size);
  case s_dngop_fix_bad_pixels_constant:
    return decode_fix_bad_pixels_constant(&op->data, p, param_size);
  case s_dngop_fix_bad_pixels_list:
    return decode_fix_bad_pixels_list(&op->data, p, param_size);
  case s_dngop_trim_bounds_t:
    return decode_trim_bounds(&op->data, p, param_size);
  case s_dngop_map_table_t:
    return decode_map_table(&op->data, p, param_size);
  case s_dngop_map_polynomial_t:
    return decode_map_polynomial(&op->data, p, param_size);
  case s_dngop_gain_map:
    return decode_gain_map(&op->data, p, param_size);
  case s_dngop_warp_rectilinear_2:
    return decode_warp_rectilinear_2(&op->data, p, param_size);
  default:
    // Unknown or unsupported opcode; skip it.
    *p += param_size;
    return 0;
  }
}

dt_dng_opcode_list_t *
dt_dng_opcode_list_decode(uint8_t *data, size_t len)
{
  uint8_t *p     = data;
  uint32_t count = get_be_long(&p);
  if(count == 0)
    return NULL;

  // Validate size of opcode list and of each opcode
  for(int i = 0; i < count; i++)
  {
    p += 12;
    uint32_t param_size = get_be_long(&p);
    p += param_size;
    if(p > &data[len])
      return NULL;
  }
  if(p != &data[len])
    return NULL;

  dt_dng_opcode_list_t *ops =
      calloc(1, sizeof(dt_dng_opcode_list_t) + count * sizeof(dt_dng_opcode_t));
  ops->count = count;

  p = &data[4];
  for(int i = 0; i < count; i++)
  {
    if(decode_opcode(&ops->ops[i], &p) != 0)
    {
      dt_dng_opcode_list_free(ops);
      return NULL;
    }
  }

  return ops;
}

void
dt_dng_opcode_list_free(dt_dng_opcode_list_t *ops)
{
  if(ops == NULL)
    return;
  for(int i = 0; i < ops->count; i++)
    free(ops->ops[i].data);
  free(ops);
}
