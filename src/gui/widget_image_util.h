#pragma once
#include <float.h>
// widget for window-size image, portable c portion

typedef struct dt_image_widget_t
{
  dt_node_t *out;
  float look_at_x, look_at_y; // look at this position in image space
  float old_look_x, old_look_y;
  float scale;                // scale/magnification factor of image (8.0f means 800%)
  float win_x, win_y, win_w, win_h;
  float m_x, m_y;
  float wd, ht;
}
dt_image_widget_t;

static inline void
dt_image_zoom(
    dt_image_widget_t *w,
    float              x, // mouse coordinate in screen space
    float              y)
{ // zoom 1:1
  // where does the mouse look in the current image?
  if(w->m_x > 0 && w->scale > 0.0f)
  { // maybe not needed? update as if mouse cursor moved (for gamepad nav)
    int dx = x - w->m_x;
    int dy = y - w->m_y;
    w->look_at_x = w->old_look_x - dx / w->scale;
    w->look_at_y = w->old_look_y - dy / w->scale;
    w->look_at_x = CLAMP(w->look_at_x, 0.0f, w->wd);
    w->look_at_y = CLAMP(w->look_at_y, 0.0f, w->ht);
  }
  float scale = w->scale <= 0.0f ? MIN(w->win_w/w->wd, w->win_h/w->ht) : w->scale;
  float im_x = x - (w->win_x + w->win_w/2.0f);
  float im_y = y - (w->win_y + w->win_h/2.0f);
  if(w->scale < 1.0f)
  {
    w->scale = 1.0f;
    const float dscale = 1.0f/scale - 1.0f/w->scale;
    w->look_at_x += im_x * dscale;
    w->look_at_y += im_y * dscale;
  }
  else if(w->scale >= 8.0f || w->scale < 1.0f)
  {
    w->scale = -1.0f;
    w->look_at_x = w->wd/2.0f;
    w->look_at_y = w->ht/2.0f;
  }
  else if(w->scale >= 1.0f)
  {
    w->scale = MIN(w->scale * 2.0f, 8.0f);
    const float dscale = 1.0f/scale - 1.0f/w->scale;
    w->look_at_x += im_x * dscale;
    w->look_at_y += im_y * dscale;
  }
}

static inline void
dt_image_reset_zoom(
    dt_image_widget_t *w)
{
  w->look_at_x = FLT_MAX;
  w->look_at_y = FLT_MAX;
  w->scale = -1;
}

static inline void
dt_image_from_view(
    dt_image_widget_t *w,
    const float        v[2],
    float              img[2])
{
  float scale = w->scale > 0.0f ? w->scale : MIN(w->win_w/w->wd, w->win_h/w->ht);
  float cvx = w->win_x + w->win_w *.5f, cvy = w->win_y + w->win_h *.5f;
  if(w->look_at_x == FLT_MAX) w->look_at_x = w->wd/2.0f;
  if(w->look_at_y == FLT_MAX) w->look_at_y = w->ht/2.0f;
  float ox = cvx - scale * (w->look_at_x - w->wd/2.0f);
  float oy = cvy - scale * (w->look_at_y - w->ht/2.0f);
  img[0] = (v[0] - ox) / (scale * w->wd) + 0.5f;
  img[1] = (v[1] - oy) / (scale * w->ht) + 0.5f;
}

// convert normalised image coordinates to pixel coord on screen
static inline void
dt_image_to_view(
    dt_image_widget_t *w,
    const float        img[2], // image pixel coordinate in [0,1]^2
    float              v[2])   // window pixel coordinate
{
  float scale = w->scale > 0.0f ? w->scale : MIN(w->win_w/w->wd, w->win_h/w->ht);
  float cvx = w->win_x + w->win_w *.5f, cvy = w->win_y + w->win_h *.5f;
  if(w->look_at_x == FLT_MAX) w->look_at_x = w->wd/2.0f;
  if(w->look_at_y == FLT_MAX) w->look_at_y = w->ht/2.0f;
  float ox = cvx - scale * (w->look_at_x - w->wd/2.0f);
  float oy = cvy - scale * (w->look_at_y - w->ht/2.0f);
  v[0] = ox + (img[0]-0.5f) * scale * w->wd;
  v[1] = oy + (img[1]-0.5f) * scale * w->ht;
}
