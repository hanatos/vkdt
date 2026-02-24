#pragma once
#include "render.h"

typedef enum dt_radial_menu_dr_state_t
{
  s_radial_menu_dr_closed = 0,
  s_radial_menu_dr_main,
  s_radial_menu_dr_widget,
}
dt_radial_menu_dr_state_t;

// store state of darkroom mode radial menu
typedef struct dt_radial_menu_dr_t
{
  dt_radial_menu_dr_state_t state;
  int modid;
  int parid;

  // dynamic main menu
  int  mcnt;
  char mtxt[20][50];
  const char *mtxtptr[20];
  int  mmodid[20];
  int  mparid[20];
  
  int mouse;    // activated by mouse, not gamepad
  int selected; // item that was last selected via gamepad
  int left;     // whether `selected` was left or right of the circle
}
dt_radial_menu_dr_t;

static inline int
dt_radial_menu_dr_active(dt_radial_menu_dr_t *m)
{
  return m->state != s_radial_menu_dr_closed;
}

static inline void // also use this to init
dt_radial_menu_dr_close(dt_radial_menu_dr_t *m)
{
  if(m->state == s_radial_menu_dr_widget)
    dt_gamepadhelp_pop();
  m->state = s_radial_menu_dr_closed;
  m->mcnt  =  0;
  m->modid = -1;
  m->parid = -1;
}

