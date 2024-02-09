#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  s_dngop_warp_rectilinear        = 1,
  s_dngop_warp_fisheye            = 2,
  s_dngop_fix_vignette_radial     = 3,
  s_dngop_fix_bad_pixels_constant = 4,
  s_dngop_fix_bad_pixels_list     = 5,
  s_dngop_trim_bounds_t           = 6,
  s_dngop_map_table_t             = 7,
  s_dngop_map_polynomial_t        = 8,
  s_dngop_gain_map                = 9,
  s_dngop_warp_rectilinear_2      = 14
}
dt_dng_opcode_id_t;

typedef struct
{
  dt_dng_opcode_id_t id;
  uint32_t        optional;
  uint32_t        preview_skip;
  void           *data;
}
dt_dng_opcode_t;

typedef struct
{
  int          count;
  dt_dng_opcode_t ops[];
}
dt_dng_opcode_list_t;

typedef struct
{
  uint32_t top;
  uint32_t left;
  uint32_t bottom;
  uint32_t right;
  uint32_t plane;
  uint32_t planes;
  uint32_t row_pitch;
  uint32_t col_pitch;
}
dt_dng_region_t;

typedef struct
{
  dt_dng_region_t region;
  uint32_t     map_points_v;
  uint32_t     map_points_h;
  double       map_spacing_v;
  double       map_spacing_h;
  double       map_origin_v;
  double       map_origin_h;
  uint32_t     map_planes;
  float        map_gain[];
}
dt_dng_gain_map_t;

typedef struct
{
  double kr[4];
  double kt[2];
}
dt_dng_warp_rectilinear_coef_t;

typedef struct
{
  uint32_t                    count;
  double                      center_x;
  double                      center_y;
  dt_dng_warp_rectilinear_coef_t coefs[];
}
dt_dng_warp_rectilinear_t;

typedef struct
{
  double kr[4];
}
dt_dng_warp_fisheye_coef_t;

typedef struct
{
  uint32_t                count;
  double                  center_x;
  double                  center_y;
  dt_dng_warp_fisheye_coef_t coefs[];
}
dt_dng_warp_fisheye_t;

typedef struct
{
  double k[5];
  double center_x;
  double center_y;
}
dt_dng_fix_vignette_radial_t;

typedef struct
{
  uint32_t constant;
  uint32_t bayer_phase;
}
dt_dng_fix_bad_pixels_constant_t;

typedef struct
{
  uint32_t  bayer_phase;
  uint32_t  bad_point_count;
  uint32_t  bad_rect_count;
  uint32_t *points;
  uint32_t *rects;
  uint32_t  _data[];
}
dt_dng_fix_bad_pixels_list_t;

typedef struct
{
  uint32_t top;
  uint32_t left;
  uint32_t bottom;
  uint32_t right;
}
dt_dng_trim_bounds_t;

typedef struct
{
  dt_dng_region_t region;
  uint32_t     table_size;
  uint16_t     table[];
}
dt_dng_map_table_t;

typedef struct
{
  dt_dng_region_t region;
  uint32_t     degree;
  double       coefs[];
}
dt_dng_map_polynomial_t;

typedef struct
{
  double kr[15];
  double kt[2];
  double min_valid_radius;
  double max_valid_radius;
}
dt_dng_warp_rectilinear_2_coef_t;

typedef struct
{
  uint32_t                      count;
  double                        center_x;
  double                        center_y;
  uint32_t                      reciprocal_radial;
  dt_dng_warp_rectilinear_2_coef_t coefs[];
}
dt_dng_warp_rectilinear_2_t;
