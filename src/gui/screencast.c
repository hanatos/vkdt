#include "screencast.h"
#include "gui.h"
void dt_screencast_init()
{
  vkdt.screencast.enable = dt_rc_get_int(&vkdt.rc, "gui/screencast", 0);
}

void dt_screencast_cleanup()
{
  dt_rc_set_int(&vkdt.rc, "gui/screencast", vkdt.screencast.enable);
}

void dt_screencast_event(
    dt_screencast_device_t dev, 
    int key, int action, int mods)
{
  if(!vkdt.screencast.enable) return;
  int hist_size = LENGTH(vkdt.screencast.ts);
  int i = (vkdt.screencast.curr+1) % hist_size;
  if(dev == s_screencast_mouse)
  {
    if(action != GLFW_PRESS) return;
    if(key == 0) snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "\\||");
    if(key == 1) snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "||\\");
    if(key == 2) snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "|\\|");
  }
  else if(dev == s_screencast_keyboard)
  {
    if(action != GLFW_PRESS) return;
    if     (key == GLFW_KEY_ESCAPE)
      snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "esc");
    else if(key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
      snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "shf");
    else if(key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
      snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "ctl");
    else
      snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "%c", CLAMP(key, ' ', '~'));
  }
  else if(dev == s_screencast_gamepad)
  {
    const char *val = ".";
    if     (key == GLFW_GAMEPAD_BUTTON_A) val = "a";
    else if(key == GLFW_GAMEPAD_BUTTON_B) val = "b";
    else if(key == GLFW_GAMEPAD_BUTTON_X) val = "x";
    else if(key == GLFW_GAMEPAD_BUTTON_Y) val = "y";
    else if(key == GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)  val = "l1";
    else if(key == GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER) val = "r1";
    else if(key == GLFW_GAMEPAD_BUTTON_BACK)         val = "bck";
    else if(key == GLFW_GAMEPAD_BUTTON_START)        val = "srt";
    else if(key == GLFW_GAMEPAD_BUTTON_GUIDE)        val = "gde";
    else if(key == GLFW_GAMEPAD_BUTTON_LEFT_THUMB)   val = "l3";
    else if(key == GLFW_GAMEPAD_BUTTON_RIGHT_THUMB)  val = "r3";
    else if(key == GLFW_GAMEPAD_BUTTON_DPAD_UP)      val = "up";
    else if(key == GLFW_GAMEPAD_BUTTON_DPAD_DOWN)    val = "dn";
    else if(key == GLFW_GAMEPAD_BUTTON_DPAD_LEFT)    val = "lf";
    else if(key == GLFW_GAMEPAD_BUTTON_DPAD_RIGHT)   val = "rg";
    snprintf(vkdt.screencast.event[i], sizeof(vkdt.screencast.event[i]), "%s", val);
  }
  else return;
  vkdt.screencast.ts[i] = dt_time();
  vkdt.screencast.device[i] = dev;
  vkdt.screencast.curr = i;
}

void dt_screencast_render()
{
  if(!vkdt.screencast.enable) return;
  int hist_size = LENGTH(vkdt.screencast.ts);
  int off_y = vkdt.state.center_y + vkdt.state.center_ht * 0.6;
  int off_x = vkdt.state.center_x + vkdt.state.center_wd * 0.6;
  int lineht = vkdt.state.center_ht * 0.1;
  static struct nk_user_font font; // memory needs to stay valid during cmd exec
  font = *nk_glfw3_font(0);
  font.height = lineht/2;
  const double max_t = 6.0; // max temporal history in seconds
  double off_now = dt_time();
  double off_ts = vkdt.screencast.ts[vkdt.screencast.curr];
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  for(int ii=0;ii<hist_size;ii++)
  {
    int i = (hist_size+vkdt.screencast.curr-ii)%hist_size;
    double dt = off_ts - vkdt.screencast.ts[i];
    double age = off_now - vkdt.screencast.ts[i];

    vkdt.wstate.busy = 3; // we draw some stuff, it needs constant updating
    int linewd = lineht;
    float off = 0.9*dt/max_t;
    int x = off_x - linewd - vkdt.state.center_wd * off;
    int y = off_y + vkdt.screencast.device[i] * lineht;

    float fade = expf(-age/max_t * 5.545f); // drops below 1/256 after max_t seconds
    uint8_t alpha = CLAMP(256.0*fade, 0, 255);
    struct nk_color fg = ii ? vkdt.style.colour[NK_COLOR_TEXT] : vkdt.style.colour[NK_COLOR_DT_ACCENT];
    struct nk_color bg = vkdt.style.colour[NK_COLOR_DT_BACKGROUND];
    fg.a = alpha;
    bg.a = alpha;
    struct nk_rect rect = { x, y, linewd, lineht };
    nk_fill_rect(cmd, rect, lineht*0.2, bg);
    nk_stroke_rect(cmd, rect, lineht*0.2, lineht*0.02, fg);
    int len = strlen(vkdt.screencast.event[i]);
    rect.x += rect.w/2-(font.height/2*len)/2; rect.y += rect.h/2-font.height/2;
    nk_draw_text(cmd, rect,
        vkdt.screencast.event[i], len,
        &font,
        (struct nk_color){0,0,0,0xff},
        fg);
    off_x = x;
    off_ts = vkdt.screencast.ts[i];
  }
}
