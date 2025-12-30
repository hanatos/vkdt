#pragma once
#include <float.h>
// widget for window-size image, portable c portion

typedef struct dt_image_widget_t
{
  dt_node_t *out;
  float look_at_x, look_at_y;       // look at this position in normalised image space (-0.5,0.5)x(-a,a)
  float old_look_x, old_look_y;     // where a is 0.5*ht/wd
  float scale;                      // scale/magnification factor of image (1.0f means scale to fit)
  float win_x, win_y, win_w, win_h; // dimensions / position if the widget, in pixels on screen
  float m_x, m_y;
  float wd, ht;                     // dimensions of the image in texels
}
dt_image_widget_t;

static inline void
dt_image_reset_zoom(
    dt_image_widget_t *w)
{ // zoom to fit, look at center
  w->look_at_x = 0.f;
  w->look_at_y = 0.f;
  w->scale = 1.0f;
}

static inline void
dt_image_from_view(
    dt_image_widget_t *w,
    const float        v[2],
    float              img[2])
{
  float scale = w->scale * MIN(w->win_w/w->wd, w->win_h/w->ht);
  float cvx = w->win_x + w->win_w *.5f, cvy = w->win_y + w->win_h *.5f;
  img[0] = (v[0] - cvx) / (scale * w->wd) + 0.5f + w->look_at_x;
  img[1] = (v[1] - cvy) / (scale * w->ht) + 0.5f + w->look_at_y;
}

// convert normalised image coordinates to pixel coord on screen
static inline void
dt_image_to_view(
    dt_image_widget_t *w,
    const float        img[2], // image pixel coordinate in [0,1]^2
    float              v[2])   // window pixel coordinate
{
  float scale = w->scale * MIN(w->win_w/w->wd, w->win_h/w->ht);
  float cvx = w->win_x + w->win_w *.5f, cvy = w->win_y + w->win_h *.5f;
  v[0] = cvx + (img[0]-0.5f - w->look_at_x) * scale * w->wd;
  v[1] = cvy + (img[1]-0.5f - w->look_at_y) * scale * w->ht;
}

static inline void
dt_image_zoom(
    dt_image_widget_t *w,
    float              x, // mouse coordinate in screen space
    float              y)
{ // zoom 1:1
  float view[2] = {x,y}, img[2];
  dt_image_from_view(w, view, img);

  float scale = MIN(w->win_w/w->wd, w->win_h/w->ht);
  float scale1 = 1.0f/scale;
  scale *= w->scale;
  if(w->scale < scale1)
  { // scale 100%
    w->scale = scale1;
    w->look_at_x = img[0]-0.5f;
    w->look_at_y = img[1]-0.5f;
  }
  else if(w->scale >= 8.0f*scale1 || w->scale < 1.0f)
  { // revert to zoom to fit
    w->scale = 1.0f;
    w->look_at_x = 0.0f;
    w->look_at_y = 0.0f;
  }
  else if(w->scale >= scale1)
  { // double up
    w->scale = MIN(w->scale * 2.0f, scale1*8.0f);
  }
  float img1[2];
  dt_image_from_view(w, view, img1);
  w->look_at_x += -img1[0]+img[0];
  w->look_at_y += -img1[1]+img[1];
}
