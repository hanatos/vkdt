#pragma once
// drag-to-adjust: select a parameter via the chord menu (or press its key),
// then move the mouse horizontally to adjust it.  click or any key confirms;
// esc restores to the value at arm time.  double-click resets to the module
// default.  hold shift while dragging for 5x increased sensitivity.
// config files live in bin/dragkeys/ and ~/.config/vkdt/dragkeys/.
// format:
//   # comment
//   key:<keyname>
//   <module>:<instance>:<param>:<component>:<sensitivity>
//   ...
// multiple component lines per file are supported (e.g. for white balance).
// all drags use exponential delta scaling; sensitivity is the base scale
// (parameter change per pixel in the linear regime).
// negative sensitivity inverts drag direction for that component
// (used in white balance to move red and blue in opposite directions from one drag).
//
// return value conventions used in this file:
//   operation functions (load, resolve, activate_*): 0 = success, 1 = failure
//   event handler functions (keyboard, mouse_*):     1 = event consumed, 0 = not consumed

#include "core/log.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/hotkey.h"
#include "pipe/graph.h"
#include "pipe/graph-history.h"
#include "pipe/modules/api.h"
#include "pipe/res.h"

#include <dirent.h>
#include <limits.h>

#define DT_DRAGKEY_MAX            16
#define DT_DRAGKEY_MAX_COMP        8
#define DT_DRAGKEY_MAX_COMP_IDX   64  // sanity cap on component index at parse time (resolve checks actual param->cnt)
#define DT_DRAGKEY_DBLCLICK_WIN  0.4  // seconds: window for double-click reset after commit
#define DT_DRAGKEY_ARROW_STEP   10.0f // pixels equivalent per arrow key press
#define DT_DRAGKEY_SHIFT_MULT   5.0f  // sensitivity multiplier while shift is held
#define DT_DRAGKEY_2D_SCALE     4.0f  // sensitivity reduction for 2D (multi-axis) drags

typedef struct dt_dragkey_comp_t
{
  dt_token_t module;
  dt_token_t instance;
  dt_token_t param;
  int        component;    // index into parameter array
  float      sensitivity;  // base scale (parameter change per pixel in linear regime)
  int        axis;        // 0 = x (left/right/h/l), 1 = y (up/down/k/j)
  // resolved at arm time:
  int        modid;
  int        parid;
  float      arm_val;     // value when armed (for esc restore), never updated mid-session
  float      min_val;     // slider min (for clamping)
  float      max_val;     // slider max (for clamping)
}
dt_dragkey_comp_t;

typedef struct dt_dragkey_t
{
  int    key;
  char   name[64];         // display name (from filename)
  dt_dragkey_comp_t comp[DT_DRAGKEY_MAX_COMP];
  int    comp_cnt;
  int    use_exp;          // 1 = exponential delta scaling
}
dt_dragkey_t;

typedef struct dt_dragkeys_t
{
  dt_dragkey_t dk[DT_DRAGKEY_MAX];
  dt_dragkey_t menu_dk;    // active dragkey, copied here on each activation
  int    cnt;
  int    latched;           // 1 = armed: mouse-move adjusts, release confirms, esc cancels
  double start_x;           // mouse x at arm time
  double start_y;           // mouse y at arm time (for axis=1 components)
  double click_down_time;   // time of first left-press while latched; 0 = no pending click
  int    shift;             // 1 = shift held, coarse adjustment active
  int    key_xdec[2];      // x-axis step-decrease: [0]=primary (LEFT),  [1]=alt (H)
  int    key_xinc[2];      // x-axis step-increase: [0]=primary (RIGHT), [1]=alt (L)
  int    key_ydec[2];      // y-axis step-decrease: [0]=primary (DOWN),  [1]=alt (J)
  int    key_yinc[2];      // y-axis step-increase: [0]=primary (UP),    [1]=alt (K)
  dt_graph_run_t pending_runflags; // follow-up run scheduled after double-click reset (double-buffer lag compensation)
}
dt_dragkeys_t;

// parse a dragkey config file into dk. returns 0 on success, 1 if unusable (missing key or components).
static inline int
dt_dragkey_load_file(dt_dragkey_t *dk, const char *path, const char *basename)
{
  FILE *f = fopen(path, "r");
  if(!f) return 1;
  dk->key = 0;
  dk->comp_cnt = 0;
  dk->use_exp = 1; // default; config may override with use_exp:0
  snprintf(dk->name, sizeof(dk->name), "%s", basename);
  char line[256];
  while(fgets(line, sizeof(line), f))
  {
    char *nl = strchr(line, '\n');
    if(nl) *nl = 0;
    if(line[0] == '#' || line[0] == 0) continue;
    if(!strncmp(line, "key:", 4))
    {
      dk->key = hk_name_to_glfw(line + 4);
    }
    else if(!strncmp(line, "use_exp:", 8))
    {
      dk->use_exp = (int)strtol(line + 8, NULL, 10);
    }
    else
    {
      if(dk->comp_cnt >= DT_DRAGKEY_MAX_COMP) continue;
      dt_dragkey_comp_t *c = dk->comp + dk->comp_cnt;
      char mod[9] = {0}, inst[9] = {0}, par[9] = {0};
      int comp = 0, axis = 0;
      float sens = 0.002f;
      if(sscanf(line, "%8[^:]:%8[^:]:%8[^:]:%d:%f:%d", mod, inst, par, &comp, &sens, &axis) >= 4)
      {
        if(comp < 0 || comp >= DT_DRAGKEY_MAX_COMP_IDX)
        {
          dt_log(s_log_err, "[dragkey] component index %d out of range, skipping line", comp);
          continue;
        }
        c->module      = dt_token(mod);
        c->instance    = dt_token(inst);
        c->param       = dt_token(par);
        c->component   = comp;
        c->sensitivity = sens;
        c->axis        = (axis == 1) ? 1 : 0;
        dk->comp_cnt++;
      }
    }
  }
  fclose(f);
  if(dk->key == 0 || dk->comp_cnt == 0) return 1;
  return 0;
}

// resolve module and parameter IDs on all components. returns 1 if no component resolved.
static inline int
dt_dragkey_resolve(dt_dragkey_t *d)
{
  int ok = 0;
  for(int j = 0; j < d->comp_cnt; j++)
  {
    dt_dragkey_comp_t *c = d->comp + j;
    c->modid = dt_module_get(&vkdt.graph_dev, c->module, c->instance);
    if(c->modid < 0) { c->parid = -1; continue; }
    c->parid = dt_module_get_param(vkdt.graph_dev.module[c->modid].so, c->param);
    if(c->parid >= 0)
    {
      int cnt = vkdt.graph_dev.module[c->modid].so->param[c->parid]->cnt;
      if(c->component >= cnt)
      {
        dt_log(s_log_err, "[dragkey] component %d >= param count %d, skipping", c->component, cnt);
        c->parid = -1;
        continue;
      }
      ok = 1;
    }
  }
  return ok ? 0 : 1;
}

static inline void
dt_dragkeys_init(dt_dragkeys_t *dk)
{
  memset(dk, 0, sizeof(*dk));
  // default step keys (rebindable via hotkey system, see render_darkroom.c)
  dk->key_xdec[0] = GLFW_KEY_LEFT;
  dk->key_xdec[1] = GLFW_KEY_H;
  dk->key_xinc[0] = GLFW_KEY_RIGHT;
  dk->key_xinc[1] = GLFW_KEY_L;
  dk->key_ydec[0] = GLFW_KEY_DOWN;
  dk->key_ydec[1] = GLFW_KEY_J;
  dk->key_yinc[0] = GLFW_KEY_UP;
  dk->key_yinc[1] = GLFW_KEY_K;
  // load from home dir first (user overrides), then basedir (shipped defaults)
  for(int inbase = 0; inbase < 2; inbase++)
  {
    void *dirp = dt_res_opendir("dragkeys", inbase);
    if(!dirp) continue;
    const char *basename = 0;
    while((basename = dt_res_next_basename(dirp, inbase)))
    {
      if(basename[0] == '.') continue;
      if(dk->cnt >= DT_DRAGKEY_MAX) break;
      int dup = 0;
      for(int i = 0; i < dk->cnt; i++)
        if(!strcmp(dk->dk[i].name, basename)) { dup = 1; break; }
      if(dup) continue; // home overrides base
      char path[PATH_MAX];
      snprintf(path, sizeof(path), "%s/dragkeys/%s",
          inbase ? dt_pipe.basedir : dt_pipe.homedir, basename);
      if(!dt_dragkey_load_file(dk->dk + dk->cnt, path, basename))
      {
        dk->cnt++;
      }
    }
    dt_res_closedir(dirp, inbase);
  }
}

static inline void
dt_dragkey_commit(dt_dragkey_t *d)
{
  // deduplicate by (modid, parid) — same param may appear on multiple components (e.g. white balance)
  for(int j = 0; j < d->comp_cnt; j++)
  {
    dt_dragkey_comp_t *c = d->comp + j;
    if(c->modid < 0 || c->parid < 0) continue;
    int dup = 0;
    for(int k = 0; k < j; k++)
      if(d->comp[k].modid == c->modid && d->comp[k].parid == c->parid) { dup = 1; break; }
    if(dup) continue;
    vkdt.graph_dev.active_module = c->modid;
    dt_graph_history_append(&vkdt.graph_dev, c->modid, c->parid, 0.0);
  }
}

// restore all params to arm-time values (esc cancel)
static inline void
dt_dragkey_restore(dt_dragkey_t *d)
{
  for(int j = 0; j < d->comp_cnt; j++)
  {
    dt_dragkey_comp_t *c = d->comp + j;
    if(c->modid < 0 || c->parid < 0) continue;
    float *val = (float *)dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
    if(!val) continue;
    val[c->component] = c->arm_val;
  }
  vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
}

static inline void
dt_dragkey_reset(dt_dragkey_t *d)
{
  dt_graph_run_t runflags = s_graph_run_all;
  // reset value and call check_params once per (modid, parid) pair
  for(int j = 0; j < d->comp_cnt; j++)
  {
    dt_dragkey_comp_t *c = d->comp + j;
    if(c->modid < 0 || c->parid < 0) continue;
    int dup = 0;
    for(int k = 0; k < j; k++)
      if(d->comp[k].modid == c->modid && d->comp[k].parid == c->parid) { dup = 1; break; }
    if(dup) continue;
    const dt_ui_param_t *pp = vkdt.graph_dev.module[c->modid].so->param[c->parid];
    const float *rp = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
    float oldval = rp ? rp[c->component] : 0.0f;
    memcpy(vkdt.graph_dev.module[c->modid].param + pp->offset, pp->val, dt_ui_param_size(pp->type, pp->cnt));
    if(vkdt.graph_dev.module[c->modid].so->check_params)
      runflags |= vkdt.graph_dev.module[c->modid].so->check_params(
          vkdt.graph_dev.module + c->modid, c->parid, c->component, &oldval);
    vkdt.graph_dev.active_module = c->modid;
    dt_graph_history_append(&vkdt.graph_dev, c->modid, c->parid, 0.0);
  }
  vkdt.graph_dev.runflags = runflags;
}

// shared teardown for all latched-mode exit paths.
// commit=1: append history, enable post-confirm double-click reset.
// commit=0: restore arm-time values, disable post-confirm double-click.
static inline void
dt_dragkeys_exit(dt_dragkeys_t *dk, int commit)
{
  if(commit)
  {
    dt_dragkey_commit(&dk->menu_dk);
    dt_gui_notification(""); // clear info bar immediately on confirm
    glfwPostEmptyEvent();   // wake the render loop so the bar disappears without waiting for mouse input
  }
  else
    dt_dragkey_restore(&dk->menu_dk);
  glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  dk->latched          = 0;
  dk->click_down_time  = 0.0;
  dk->pending_runflags = 0;
  vkdt.wstate.dragkey_latched = 0;
}

static inline void
dt_dragkeys_cancel(dt_dragkeys_t *dk)
{
  if(dk->latched) dt_dragkeys_exit(dk, 0);
}

static inline void
dt_dragkeys_cleanup(dt_dragkeys_t *dk)
{
  dt_dragkeys_cancel(dk);
  dk->cnt = 0;
}

// re-anchor start_x and arm values to current state.
// called when shift is pressed/released so the sensitivity change doesn't cause a jump.
static inline void
dt_dragkeys_reanchor(dt_dragkeys_t *dk)
{
  double mx, my;
  dt_view_get_cursor_pos(vkdt.win.window, &mx, &my);
  dk->start_x = mx;
  dk->start_y = my;
  for(int j = 0; j < dk->menu_dk.comp_cnt; j++)
  {
    dt_dragkey_comp_t *c = dk->menu_dk.comp + j;
    if(c->modid < 0 || c->parid < 0) continue;
    const float *val = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
    if(val) c->arm_val = val[c->component];
  }
}

// copy d to menu_dk, resolve module/param IDs, snapshot arm values, and arm latched drag.
// returns 0 on success, 1 if no eligible component found.
static inline int
dt_dragkey_activate_dk(dt_dragkeys_t *dk, dt_dragkey_t *d)
{
  dk->menu_dk = *d;
  dt_dragkey_t *md = &dk->menu_dk;
  if(dt_dragkey_resolve(md)) return 1;
  int ok = 0;
  vkdt.wstate.pending_modid = -1;
  for(int j = 0; j < md->comp_cnt; j++)
  {
    dt_dragkey_comp_t *c = md->comp + j;
    if(c->modid < 0 || c->parid < 0) continue;
    const float *val = dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
    if(!val) continue;
    c->arm_val = val[c->component];
    const dt_ui_param_t *pp = vkdt.graph_dev.module[c->modid].so->param[c->parid];
    c->min_val = pp->widget.min;
    c->max_val = pp->widget.max;
    if(!ok) vkdt.wstate.pending_modid = c->modid;
    ok = 1;
  }
  if(!ok) return 1;
  double mx, my;
  dt_view_get_cursor_pos(vkdt.win.window, &mx, &my);
  dk->start_x = mx;
  dk->start_y = my;
  dk->latched          = 1;
  dk->shift            = 0;
  dk->click_down_time  = 0.0;
  glfwSetInputMode(vkdt.win.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  vkdt.wstate.dragkey_latched = 1;
  return 0;
}

// activate a loaded dragkey by name. returns 0 on success, 1 if not found.
static inline int
dt_dragkey_activate_named(dt_dragkeys_t *dk, const char *name)
{
  for(int i = 0; i < dk->cnt; i++)
    if(!strcmp(dk->dk[i].name, name))
      return dt_dragkey_activate_dk(dk, dk->dk + i);
  return 1;
}

// called from darkroom_keyboard. returns 1 if the event was consumed.
static inline int
dt_dragkey_keyboard(dt_dragkeys_t *dk, int key, int action, int mods)
{
  (void)mods;
  if(action == GLFW_PRESS && !dk->latched)
  {
    for(int i = 0; i < dk->cnt; i++)
    {
      if(dk->dk[i].key != key) continue;
      dt_dragkey_t *d = dk->dk + i;
      if(!dt_dragkey_activate_dk(dk, d))
      {
        dt_gui_notification("%s: drag to adjust -- click or any key to confirm, esc to cancel", dk->menu_dk.name);
        return 1;
      }
      return 0;
    }
  }
  else if(dk->latched)
  { // armed: step keys adjust, any other key-press confirms (esc cancels)
    int is_xdec = (key == dk->key_xdec[0] || key == dk->key_xdec[1]);
    int is_xinc = (key == dk->key_xinc[0] || key == dk->key_xinc[1]);
    int is_ydec = (key == dk->key_ydec[0] || key == dk->key_ydec[1]);
    int is_yinc = (key == dk->key_yinc[0] || key == dk->key_yinc[1]);
    if((action == GLFW_PRESS || action == GLFW_REPEAT) && (is_xdec || is_xinc || is_ydec || is_yinc))
    { // step the parameter and stay armed (hold for repeat)
      int step_axis = (is_ydec || is_yinc) ? 1 : 0;
      float dir = (is_xinc || is_yinc) ? 1.0f : -1.0f;
      dt_dragkey_t *d = &dk->menu_dk;
      char msg[128];
      int off = snprintf(msg, sizeof(msg), "%s:", d->name);
      dt_graph_run_t runflags = s_graph_run_record_cmd_buf;
      for(int j = 0; j < d->comp_cnt; j++)
      {
        dt_dragkey_comp_t *c = d->comp + j;
        if(c->modid < 0 || c->parid < 0) continue;
        if(c->axis != step_axis) continue;
        float *val = (float *)dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
        if(!val) continue;
        float new_val = val[c->component] + dir * c->sensitivity * DT_DRAGKEY_ARROW_STEP * (dk->shift ? DT_DRAGKEY_SHIFT_MULT : 1.0f);
        if(c->min_val < c->max_val)
        {
          if(new_val < c->min_val) new_val = c->min_val;
          if(new_val > c->max_val) new_val = c->max_val;
        }
        float oldval = val[c->component];
        val[c->component] = new_val;
        if(vkdt.graph_dev.module[c->modid].so->check_params)
          runflags |= vkdt.graph_dev.module[c->modid].so->check_params(
              vkdt.graph_dev.module + c->modid, c->parid, c->component, &oldval);
        if(off < (int)sizeof(msg) - 20)
          off += snprintf(msg + off, sizeof(msg) - off, " %.3f", new_val);
      }
      vkdt.graph_dev.runflags = runflags;
      dt_dragkeys_reanchor(dk); // re-anchor so subsequent mouse movement starts from the stepped value
      dt_gui_notification("%s", msg);
      return 1;
    }
    if(key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
    { // shift toggles coarse multiplier; track via event so mouse_move doesn't need to poll
      if(action != GLFW_REPEAT)
      {
        int new_shift = (action == GLFW_PRESS);
        if(new_shift != dk->shift) { dk->shift = new_shift; dt_dragkeys_reanchor(dk); }
      }
      return 1;
    }
    if(key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL ||
       key == GLFW_KEY_LEFT_ALT     || key == GLFW_KEY_RIGHT_ALT    ||
       key == GLFW_KEY_LEFT_SUPER   || key == GLFW_KEY_RIGHT_SUPER  ||
       key == GLFW_KEY_CAPS_LOCK)
      return 1; // other modifier keys don't commit
    if(action == GLFW_PRESS)
    { // any other key: confirm (esc = cancel/restore)
      dt_dragkeys_exit(dk, key != GLFW_KEY_ESCAPE);
      return 1;
    }
    return 1; // consume all key events while latched
  }
  return 0;
}

// called from darkroom_mouse_position. returns 1 if armed (consuming mouse events).
static inline int
dt_dragkey_mouse_move(dt_dragkeys_t *dk, double x, double y)
{
  if(!dk->latched) return 0;
  // if a click is pending, we only check for timeout and do not move.
  // mouse_move is the reliable flush path for single clicks because GLFW_CURSOR_DISABLED
  // delivers move events for any mouse micro-movement.
  if(dk->click_down_time > 0.0)
  {
    if(glfwGetTime() - dk->click_down_time >= DT_DRAGKEY_DBLCLICK_WIN)
    {
      dk->click_down_time = 0.0;
      dt_dragkeys_exit(dk, 1);
    }
    return 1;
  }
  dt_dragkey_t *d = &dk->menu_dk;
  float dx = (float)(x - dk->start_x);
  float dy = (float)(y - dk->start_y);
  if(dk->shift) { dx *= DT_DRAGKEY_SHIFT_MULT; dy *= DT_DRAGKEY_SHIFT_MULT; }
  char msg[128];
  int off = 0;
  off += snprintf(msg + off, sizeof(msg) - off, "%s:", d->name);
  dt_graph_run_t runflags = s_graph_run_record_cmd_buf;
  float scale = vkdt.state.center_wd > 0 ? vkdt.state.center_wd * 0.5f : 500.0f;
  float delta_x, delta_y;
  if(d->use_exp)
  {
    float adx = fminf(fabsf(dx), 20.0f * scale);
    float ady = fminf(fabsf(dy), 20.0f * scale);
    delta_x = (dx >= 0 ? 1.0f : -1.0f) * scale * (expf(adx / scale) - 1.0f);
    delta_y = (dy >= 0 ? 1.0f : -1.0f) * scale * (expf(ady / scale) - 1.0f);
  }
  else { delta_x = dx; delta_y = dy; }

  // check if this is a colwheel (2D widget with special barycentric math)
  int is_colwheel = 0;
  if(d->comp_cnt >= 2)
  {
    const dt_ui_param_t *pp = vkdt.graph_dev.module[d->comp[0].modid].so->param[d->comp[0].parid];
    is_colwheel = (pp && pp->widget.type == dt_token("colwheel"));
  }

  if(is_colwheel && d->comp_cnt >= 2 && d->comp[0].modid >= 0 && d->comp[0].parid >= 0)
  {
    // apply barycentric math like mouse drag does for correct color mapping
    float sx = delta_x * d->comp[0].sensitivity;
    float sy = delta_y * d->comp[1].sensitivity;

    // barycentric projection (same as in colwheel widget rendering)
    const float inv_rt3 = 0.5773503f; // 1/sqrt(3)
    float disp[3];
    disp[0] = -2.0f/3.0f * sy;
    disp[1] = 1.0f/3.0f * sy - inv_rt3 * sx;
    disp[2] = 1.0f/3.0f * sy + inv_rt3 * sx;

    // clamp displacement to enforce limits, matching mouse drag behavior exactly
    float lo = d->comp[0].min_val, hi = d->comp[0].max_val;
    if(lo < hi) for(int k = 0; k < 3; k++)
      disp[k] = CLAMP(d->comp[k].arm_val + disp[k], lo, hi) - d->comp[k].arm_val;

    // constrain displacement to keep visual dot within wheel boundary
    float rt32 = 0.8660254f; // sqrt(3)/2
    float wx = rt32 * (disp[2] - disp[1]);
    float wy = -disp[0] + 0.5f * (disp[1] + disp[2]);
    float dist_sq = wx * wx + wy * wy;
    float max_dist_sq = (1.0f / 2.1f) * (1.0f / 2.1f); // matches wheel boundary: sqrt(wx^2+wy^2) <= 1/2.1
    if(dist_sq > max_dist_sq)
    {
      float scale = sqrtf(max_dist_sq / dist_sq);
      disp[0] *= scale;
      disp[1] *= scale;
      disp[2] *= scale;
    }

    // apply to R, G, B (first 3 components)
    float *val = (float *)dt_module_param_float(vkdt.graph_dev.module + d->comp[0].modid, d->comp[0].parid);
    if(val)
    {
      for(int k = 0; k < 3; k++)
      {
        float new_val = d->comp[k].arm_val + disp[k];
        float oldval = val[k];
        val[k] = new_val;
        if(vkdt.graph_dev.module[d->comp[0].modid].so->check_params)
          runflags |= vkdt.graph_dev.module[d->comp[0].modid].so->check_params(
              vkdt.graph_dev.module + d->comp[0].modid, d->comp[0].parid, k, &oldval);
        if(off < (int)sizeof(msg) - 20)
          off += snprintf(msg + off, sizeof(msg) - off, " %.3f", new_val);
      }
    }
  }
  else
  {
    // standard component-wise delta application for other widgets
    for(int j = 0; j < d->comp_cnt; j++)
    {
      dt_dragkey_comp_t *c = d->comp + j;
      if(c->modid < 0 || c->parid < 0) continue;
      float delta = (c->axis == 1) ? delta_y : delta_x;
      float new_val = c->arm_val + delta * c->sensitivity;
      if(c->min_val < c->max_val)
      {
        if(new_val < c->min_val) new_val = c->min_val;
        if(new_val > c->max_val) new_val = c->max_val;
      }
      float *val = (float *)dt_module_param_float(vkdt.graph_dev.module + c->modid, c->parid);
      if(!val) continue;
      float oldval = val[c->component];
      val[c->component] = new_val;
      if(vkdt.graph_dev.module[c->modid].so->check_params)
        runflags |= vkdt.graph_dev.module[c->modid].so->check_params(
            vkdt.graph_dev.module + c->modid, c->parid, c->component, &oldval);
      if(off < (int)sizeof(msg) - 20)
        off += snprintf(msg + off, sizeof(msg) - off, " %.3f", new_val);
    }
  }
  vkdt.graph_dev.runflags |= runflags;
  dt_gui_notification("%s", msg);
  return 1;
}

// called from darkroom_mouse_button. returns 1 if event was consumed.
static inline int
dt_dragkey_mouse_button(dt_dragkeys_t *dk, int button, int action, int mods)
{
  if(!dk->latched) return 0;
  if(button == 1 && action == GLFW_PRESS)
  { // right click: cancel/restore
    dt_dragkeys_exit(dk, 0);
    return 1;
  }
  if(button == 0 && !(mods & GLFW_MOD_SHIFT))
  {
    if(action == GLFW_PRESS)
    {
      double now = glfwGetTime();
      if(dk->click_down_time > 0.0 && (now - dk->click_down_time) < DT_DRAGKEY_DBLCLICK_WIN)
      { // second press within window: double-click → reset to default and re-anchor
        dk->click_down_time = 0.0;
        dt_dragkey_reset(&dk->menu_dk);
        dk->pending_runflags = s_graph_run_record_cmd_buf; // schedule follow-up to flush double-buffer lag
        dt_dragkeys_reanchor(dk);
        dt_gui_notification("%s: reset to default -- drag to adjust", dk->menu_dk.name);
      }
      else
      {
        dk->click_down_time = now; // record first press time and wait for release or second press
      }
      return 1;
    }
    if(action == GLFW_RELEASE)
    { // don't commit on release — wait out the double-click window first
      dt_gui_notification(""); // clear info bar immediately; commit fires on next mouse move
      return 1;
    }
  }
  return 1; // consume all other mouse events while latched
}

// programmatically activate a drag for ncomps components of a param.
// ncomps=1: single 1D drag on the x-axis (component given by first_comp).
// ncomps=2: 2D drag; component first_comp on x-axis, first_comp+1 on y-axis.
// sensitivity is auto-computed from the slider range and current image width.
// note: modid/parid are looked up here to compute sensitivity, then looked up again
// inside dt_dragkey_activate_dk via dt_dragkey_resolve. the double lookup is cheap
// and avoids threading resolved ids through the dt_dragkey_t config-level interface.
static inline int
dt_dragkey_activate_n(
    dt_dragkeys_t *dk,
    dt_token_t     mod,
    dt_token_t     inst,
    dt_token_t     param,
    int            first_comp,
    int            ncomps)
{
  int modid = dt_module_get(&vkdt.graph_dev, mod, inst);
  if(modid < 0) return 1;
  int parid = dt_module_get_param(vkdt.graph_dev.module[modid].so, param);
  if(parid < 0) return 1;
  int cnt = vkdt.graph_dev.module[modid].so->param[parid]->cnt;
  if(first_comp < 0 || first_comp + ncomps > cnt) return 1;
  const dt_ui_param_t *pp = vkdt.graph_dev.module[modid].so->param[parid];
  float range = pp->widget.max - pp->widget.min;
  if(range <= 0) range = 1.0f;
  float width = vkdt.state.center_wd > 0 ? vkdt.state.center_wd : 1000.0f;
  float sens = range / width / (ncomps > 1 ? DT_DRAGKEY_2D_SCALE : 1.0f);
  dt_dragkey_t d = {0};
  d.use_exp  = 1;
  d.comp_cnt = ncomps;
  for(int i = 0; i < ncomps; i++)
  {
    d.comp[i].module      = mod;
    d.comp[i].instance    = inst;
    d.comp[i].param       = param;
    d.comp[i].component   = first_comp + i;
    d.comp[i].axis        = i; // comp 0 → x-axis, comp 1 → y-axis
    d.comp[i].sensitivity = sens;
  }
  snprintf(d.name, sizeof(d.name), "%.8s:%.8s", dt_token_str(mod), dt_token_str(param));
  return dt_dragkey_activate_dk(dk, &d);
}

// format a dragkey action string (used by menu provider)
static inline void
dt_dragkey_format_action(char *buf, int bufsz,
    dt_token_t mod, dt_token_t inst, dt_token_t param, int component)
{
  snprintf(buf, bufsz, "dragkey:%.8s:%.8s:%.8s:%d",
      dt_token_str(mod), dt_token_str(inst),
      dt_token_str(param), component);
}

// parse a dragkey action string and activate. returns 0 on success.
static inline int
dt_dragkey_parse_and_activate(dt_dragkeys_t *dk, const char *action)
{
  char mod[9] = {0}, inst[9] = {0}, par[9] = {0};
  int comp = 0;
  if(sscanf(action + sizeof("dragkey:") - 1, "%8[^:]:%8[^:]:%8[^:]:%d", mod, inst, par, &comp) < 4)
    return 1;
  return dt_dragkey_activate_n(dk, dt_token(mod), dt_token(inst), dt_token(par), comp, 1);
}

// parse "dragkey2d:mod:inst:param" and activate a 2D drag (comp 0 on x, comp 1 on y).
// for colwheel widgets, activate all 3 RGB components since barycentric math modifies all 3.
static inline int
dt_dragkey_parse_and_activate_2d(dt_dragkeys_t *dk, const char *action)
{
  char mod[9] = {0}, inst[9] = {0}, par[9] = {0};
  if(sscanf(action + sizeof("dragkey2d:") - 1, "%8[^:]:%8[^:]:%8[^:]", mod, inst, par) < 3)
    return 1;
  // check if this is a colwheel parameter
  int modid = dt_module_get(&vkdt.graph_dev, dt_token(mod), dt_token(inst));
  int parid = modid >= 0 ? dt_module_get_param(vkdt.graph_dev.module[modid].so, dt_token(par)) : -1;
  int is_colwheel = 0;
  if(modid >= 0 && parid >= 0)
  {
    const dt_ui_param_t *pp = vkdt.graph_dev.module[modid].so->param[parid];
    is_colwheel = (pp && pp->widget.type == dt_token("colwheel"));
  }
  // colwheel uses barycentric math on all 3 RGB components, so we need 3 components.
  // normal 2D parameters only need 2.
  int ncomps = is_colwheel ? 3 : 2;
  return dt_dragkey_activate_n(dk, dt_token(mod), dt_token(inst), dt_token(par), 0, ncomps);
}
