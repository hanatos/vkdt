#pragma once

typedef enum dt_lens_corr_type_t
{
  s_lens_corr_none = 0,
  s_lens_corr_dng  = 1
}
dt_lens_corr_type_t;

typedef struct dt_warp_rect_coef_t
{
  double kr0, kr1, kr2, kr3;
  double kt0, kt1;
}
dt_warp_rect_coef_t;

typedef struct dt_lens_corr_dng_t
{
  // 0: no WarpRectilinear
  // 1: warp_rect_coef[0] applies to all planes
  // 3: separate coefficients for each plane
  int                 warp_rect_planes;
  dt_warp_rect_coef_t warp_rect_coef[3];
  double              warp_rect_center_x, warp_rect_center_y;
}
dt_lens_corr_dng_t;

typedef union dt_lens_corr_params_t
{
  dt_lens_corr_dng_t dng;
}
dt_lens_corr_params_t;

typedef struct dt_lens_corr_t
{
  dt_lens_corr_type_t   type;
  dt_lens_corr_params_t params;
}
dt_lens_corr_t;
