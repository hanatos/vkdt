#pragma once
// some stuff useful for recording screencasts.
// on wayland, it's not always easy to keep generic show-keys
// applications always on top, depending on compositor. this provides
// some unified means for this functionality.

#define HIST_SIZE 20
typedef enum dt_screencast_device_t
{
  s_screencast_keyboard   = 0,
  s_screencast_mouse      = 1,
  s_screencast_gamepad    = 2,
  s_screencast_device_cnt = 3,
}
dt_screencast_device_t;

typedef struct dt_screencast_t
{ // will be zero inited on vkdt struct
  char event[HIST_SIZE][10]; // ringbuffer
  double ts [HIST_SIZE]; // time stamps
  dt_screencast_device_t device[HIST_SIZE];
  int curr; // pointer to head of ringbuffer
}
dt_screencast_t;

void dt_screencast_event(
    dt_screencast_device_t dev, 
    int key, int action, int mods);

void dt_screencast_render();
#undef HIST_SIZE
