#pragma once

typedef enum dt_anim_mode_t
{
  s_anim_lerp     = 0,
  s_anim_step     = 1,
  s_anim_ease_in  = 2,
  s_anim_ease_out = 3,
  s_anim_smooth   = 4,
}
dt_anim_mode_t;


static inline float
dt_anim_ease_in(float t)
{
  return t*t*t;
}

static inline float
dt_anim_ease_out(float t)
{
  float t1 = 1.0f-t;
  return 1.0f - t1*t1*t1;
}

static inline float
dt_anim_smooth(float t)
{
  return 3.0f*t*t - 2.0f*t*t*t;
}

static inline float
dt_anim_warp(float t, dt_anim_mode_t mode)
{
  switch(mode)
  {
    default:
    case s_anim_lerp    : return t;
    case s_anim_step    : return t > 0.5f ? 1.0f : 0.0f;
    case s_anim_ease_in : return dt_anim_ease_in(t);
    case s_anim_ease_out: return dt_anim_ease_out(t);
    case s_anim_smooth  : return dt_anim_smooth(t);
  }
}
