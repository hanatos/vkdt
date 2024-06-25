#pragma once
#include "pipe/graph-history.h"
#include "gui/gui.h"
#include "gui/darkroom.h"
#include "pipe/draw.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// api functions for gui interactions, c portion.
// the c++ header api.hh includes this one too, but this here
// can be called from the non-imgui parts of the code.

static inline void
dt_gui_dr_reload_shaders()
{
  system("make reload-shaders");
  vkdt.graph_dev.runflags = s_graph_run_all;
}

static inline void
dt_gui_dr_anim_start()
{
  vkdt.state.anim_playing = 1;
  if(vkdt.graph_dev.main_img_param.snd_samplerate)
    dt_snd_init(
      &vkdt.snd, 
      vkdt.graph_dev.main_img_param.snd_samplerate,
      vkdt.graph_dev.main_img_param.snd_channels,
      vkdt.graph_dev.main_img_param.snd_format);
}

static inline void
dt_gui_dr_anim_stop()
{
  vkdt.state.anim_playing = 0;
  dt_snd_cleanup(&vkdt.snd);
}

static inline void
dt_gui_dr_anim_seek(int frame)
{
  vkdt.graph_dev.frame = vkdt.state.anim_frame = CLAMP(frame, 0, vkdt.graph_dev.frame_cnt-1);
  vkdt.state.anim_no_keyframes = 0;  // (re-)enable keyframes
  dt_graph_apply_keyframes(&vkdt.graph_dev); // rerun once
  vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
}

static inline void
dt_gui_dr_anim_seek_keyframe_fwd()
{
  int f = vkdt.graph_dev.frame;     // current frame
  int g = vkdt.graph_dev.frame_cnt; // best candidate we found
  const dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(uint32_t m=0;m<cnt;m++)
    for(uint32_t k=0;k<arr[modid[m]].keyframe_cnt;k++)
      if(arr[modid[m]].keyframe[k].frame > f && arr[modid[m]].keyframe[k].frame < g)
        g = arr[modid[m]].keyframe[k].frame;
  dt_gui_dr_anim_seek(g);
}

static inline void
dt_gui_dr_anim_seek_keyframe_bck()
{
  int f = vkdt.graph_dev.frame; // current frame
  int g = 0;                    // best candidate
  const dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(uint32_t m=0;m<cnt;m++)
    for(uint32_t k=0;k<arr[modid[m]].keyframe_cnt;k++)
      if(arr[modid[m]].keyframe[k].frame < f && arr[modid[m]].keyframe[k].frame > g)
        g = arr[modid[m]].keyframe[k].frame;
  dt_gui_dr_anim_seek(g);
}

static inline void
dt_gui_dr_anim_step_fwd()
{
  dt_gui_dr_anim_seek(vkdt.graph_dev.frame + 1);
}

static inline void
dt_gui_dr_anim_step_bck()
{
  dt_gui_dr_anim_seek(vkdt.graph_dev.frame - 1);
}

static inline void
dt_gui_dr_next()
{
  if(vkdt.graph_dev.frame_cnt != 1)
  { // start/stop animation
    if(vkdt.state.anim_playing == 0)
      dt_gui_dr_anim_start();
    else
      dt_gui_dr_anim_stop();
  }
  else
  { // advance to next image in lighttable collection
    uint32_t next = dt_db_current_colid(&vkdt.db) + 1;
    if(next < vkdt.db.collection_cnt)
    {
      int err;
      err = darkroom_leave(); // writes back thumbnails. maybe there'd be a cheaper way to invalidate.
      if(err) return;
      dt_db_selection_clear(&vkdt.db);
      dt_db_selection_add(&vkdt.db, next);
      err = darkroom_enter();
      if(err)
      { // roll back
        dt_db_selection_clear(&vkdt.db);
        dt_db_selection_add(&vkdt.db, next-1);
        darkroom_enter(); // hope they take us back
      }
    }
  }
}

static inline void
dt_gui_dr_prev()
{
  if(vkdt.graph_dev.frame_cnt != 1)
  {
    vkdt.graph_dev.frame = vkdt.state.anim_frame = 0; // reset to beginning
    vkdt.graph_dev.runflags = s_graph_run_all; // reprocess first frame
    vkdt.state.anim_no_keyframes = 0;  // (re-)enable keyframes
    dt_graph_apply_keyframes(&vkdt.graph_dev); // rerun once
    dt_gui_dr_anim_stop();
  }
  else
  { // backtrack to last image in lighttable collection
    int32_t next = dt_db_current_colid(&vkdt.db) - 1;
    if(next >= 0)
    {
      int err;
      err = darkroom_leave(); // writes back thumbnails. maybe there'd be a cheaper way to invalidate.
      if(err) return;
      dt_db_selection_clear(&vkdt.db);
      dt_db_selection_add(&vkdt.db, next);
      err = darkroom_enter();
      if(err)
      { // roll back
        dt_db_selection_clear(&vkdt.db);
        dt_db_selection_add(&vkdt.db, next+1);
        darkroom_enter(); // hope they take us back
      }
    }
  }
}

// assumes the crop start button has been pressed and we're in crop mode in the ui
static inline void
dt_gui_dr_crop_adjust(
    float edge,   // value to set vkdt.wtstate.selected edge to
    int   inc)    // if nonzero, increment edge instead of setting
{
  if(vkdt.wstate.selected >= 0 && vkdt.wstate.selected < 4)
  {
    if(inc) vkdt.wstate.state[vkdt.wstate.selected] += edge;
    else    vkdt.wstate.state[vkdt.wstate.selected]  = edge;
    if(vkdt.wstate.aspect > 0.0f)
    { // fix up aspect ratio
      float target_aspect = vkdt.wstate.aspect;
      if(vkdt.wstate.selected == 0)
      { // left, move right side along:
        vkdt.wstate.state[1] = vkdt.wstate.state[0] + target_aspect * (vkdt.wstate.state[3]-vkdt.wstate.state[2]);
      }
      else if(vkdt.wstate.selected == 2)
      { // top, move bottom side along:
        vkdt.wstate.state[3] = vkdt.wstate.state[2] + 1.0f/target_aspect * (vkdt.wstate.state[1]-vkdt.wstate.state[0]);
      }
      else if(vkdt.wstate.selected == 1)
      { // right, move top and bottom simultaneously to keep center:
        float c = (vkdt.wstate.state[3] + vkdt.wstate.state[2])*0.5f;
        float w =  vkdt.wstate.state[1] - vkdt.wstate.state[0];
        vkdt.wstate.state[3] = c + 0.5f / target_aspect * w;
        vkdt.wstate.state[2] = c - 0.5f / target_aspect * w;
      }
      else if(vkdt.wstate.selected == 3)
      { // bottom, move left and right simultaneously to keep center:
        float c = (vkdt.wstate.state[1] + vkdt.wstate.state[0])*0.5f;
        float w =  vkdt.wstate.state[3] - vkdt.wstate.state[2];
        vkdt.wstate.state[1] = c + 0.5f * target_aspect * w;
        vkdt.wstate.state[0] = c - 0.5f * target_aspect * w;
      }
    }
    // make sure min <= max
    if(vkdt.wstate.state[1] < vkdt.wstate.state[0]) { float tmp = vkdt.wstate.state[1]; vkdt.wstate.state[1] = vkdt.wstate.state[0]; vkdt.wstate.state[0] = tmp; }
    if(vkdt.wstate.state[3] < vkdt.wstate.state[2]) { float tmp = vkdt.wstate.state[3]; vkdt.wstate.state[3] = vkdt.wstate.state[2]; vkdt.wstate.state[2] = tmp; }
  }
}

static inline void
dt_gui_dr_pers_adjust(
    float *n,   // corner coordinates in image space
    int    inc) // if set increment instead of set
{
  if(vkdt.wstate.selected < 0 || vkdt.wstate.selected >= 4) return;
  // copy to quad state at corner c
  if(inc)
  {
    vkdt.wstate.state[2*vkdt.wstate.selected+0] += n[0];
    vkdt.wstate.state[2*vkdt.wstate.selected+1] += n[1];
  }
  else
  {
    vkdt.wstate.state[2*vkdt.wstate.selected+0] = n[0];
    vkdt.wstate.state[2*vkdt.wstate.selected+1] = n[1];
  }
}

static inline void
dt_gui_dr_toggle_fullscreen_view()
{
  vkdt.wstate.fullscreen_view ^= 1;
  if(vkdt.wstate.fullscreen_view)
  {
    vkdt.state.center_x = 0;
    vkdt.state.center_y = 0;
    vkdt.state.center_wd = qvk.win_width;
    vkdt.state.center_ht = qvk.win_height;
  }
  else
  {
    vkdt.state.center_x = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_y = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - vkdt.state.panel_wd;
    vkdt.state.center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width;
  }
}

static inline void
dt_gui_dr_set_fullscreen_view()
{
  if(!vkdt.wstate.fullscreen_view) dt_gui_dr_toggle_fullscreen_view();
}

static inline void
dt_gui_dr_unset_fullscreen_view()
{
  if(vkdt.wstate.fullscreen_view) dt_gui_dr_toggle_fullscreen_view();
}

static inline void
dt_gui_dr_toggle_history()
{
  vkdt.wstate.history_view ^= 1;
  if(vkdt.wstate.history_view)
  {
    vkdt.state.center_x = vkdt.style.border_frac * qvk.win_width + vkdt.state.panel_wd;
    vkdt.state.center_y = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - 2*vkdt.state.panel_wd;
    vkdt.state.center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width;
  }
  else
  {
    vkdt.state.center_x = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_y = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - vkdt.state.panel_wd;
    vkdt.state.center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width;
  }
}

static inline void
dt_gui_dr_show_history()
{
  if(vkdt.wstate.history_view) return;
  dt_gui_dr_toggle_history();
}

static inline void
dt_gui_dr_history_undo()
{ // "cur" means "current end", item is just before that.
  dt_graph_history_set(&vkdt.graph_dev, vkdt.graph_dev.history_item_cur-2);
  vkdt.graph_dev.runflags = s_graph_run_all;
}

static inline void
dt_gui_dr_history_redo()
{
  dt_graph_history_set(&vkdt.graph_dev, vkdt.graph_dev.history_item_cur);
  vkdt.graph_dev.runflags = s_graph_run_all;
}

static inline void
dt_gui_dr_enter_fullscreen_view()
{
  if(!vkdt.wstate.fullscreen_view)
    dt_gui_dr_toggle_fullscreen_view();
}

static inline void
dt_gui_dr_leave_fullscreen_view()
{
  if(vkdt.wstate.fullscreen_view)
    dt_gui_dr_toggle_fullscreen_view();
}

static inline void
dt_gui_dr_show_dopesheet()
{
  vkdt.wstate.dopesheet_view = 1.0;
}

static inline void
dt_gui_dr_hide_dopesheet()
{
  vkdt.wstate.dopesheet_view = 0.0;
}

static inline void
dt_gui_dr_toggle_dopesheet()
{
  if(vkdt.wstate.dopesheet_view > 0) vkdt.wstate.dopesheet_view = 0.0;
  else vkdt.wstate.dopesheet_view = 1.0;
}

static inline void
dt_gui_lt_duplicate()
{
  if(!vkdt.db.selection_cnt) return; // no images selected
  dt_db_duplicate_selected_images(&vkdt.db); // just create .cfg files, don't update db
  char dir[PATH_MAX]; // reload directory:
  snprintf(dir, sizeof(dir), "%s", vkdt.db.dirname);
  dt_gui_switch_collection(dir);
}

// this only takes care of in/out chain reconnections when a module is removed.
// the module removal will take care of the rest.
static inline int
dt_gui_dr_disconnect_module(int m)
{
  dt_graph_t *graph = &vkdt.graph_dev;
  int id_dspy = dt_module_get(graph, dt_token("display"), dt_token("dspy"));
  if(id_dspy >= 0) dt_module_connect(graph, -1, -1, id_dspy, 0); // no history
  graph->runflags = s_graph_run_all;

  int ret = 0;
  dt_module_t *mod = graph->module + m;
  if(mod->so->has_inout_chain)
  {
    int m_after[DT_MAX_CONNECTORS], c_after[DT_MAX_CONNECTORS], max_after = DT_MAX_CONNECTORS;
    int c_prev, m_prev = dt_module_get_module_before(graph, mod, &c_prev);
    int cnt = dt_module_get_module_after(graph, mod, m_after, c_after, max_after);
    if(m_prev != -1 && cnt > 0)
    {
      int c_our = dt_module_get_connector(mod, dt_token("input"));
      ret = dt_module_connect_with_history(graph, -1, -1, m, c_our);
      for(int k=0;k<cnt;k++)
        if(!ret)
          ret = dt_module_connect_with_history(graph, m_prev, c_prev, m_after[k], c_after[k]);
    }
  }
  return ret;
}

static inline void
dt_gui_dr_remove_module(int m)
{
  dt_gui_dr_disconnect_module(m);
  dt_module_remove_with_history(&vkdt.graph_dev, vkdt.graph_dev.module[m].name, vkdt.graph_dev.module[m].inst);
}

static inline void
dt_gui_rate(int rate)
{
  if(vkdt.view_mode == s_view_lighttable)
  {
    const uint32_t *sel = dt_db_selection_get(&vkdt.db);
    for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      vkdt.db.image[sel[i]].rating = rate;
  }
  else if(vkdt.view_mode == s_view_darkroom)
  {
    const uint32_t ci = dt_db_current_imgid(&vkdt.db);
    if(ci != -1u) vkdt.db.image[ci].rating = rate;
  }
}

static inline void
dt_gui_label_unset(dt_image_label_t l)
{ // remove label
  if(vkdt.view_mode == s_view_lighttable)
  {
    const uint32_t *sel = dt_db_selection_get(&vkdt.db);
    for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      vkdt.db.image[sel[i]].labels &= ~l;
  }
  else if(vkdt.view_mode == s_view_darkroom)
  {
    const uint32_t ci = dt_db_current_imgid(&vkdt.db);
    if(ci != -1u) vkdt.db.image[ci].labels &= ~l;
  }
}

static inline void
dt_gui_label_set(dt_image_label_t l)
{ // set label
  if(vkdt.view_mode == s_view_lighttable)
  {
    const uint32_t *sel = dt_db_selection_get(&vkdt.db);
    for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      vkdt.db.image[sel[i]].labels |= l;
  }
  else if(vkdt.view_mode == s_view_darkroom)
  {
    const uint32_t ci = dt_db_current_imgid(&vkdt.db);
    if(ci != -1u) vkdt.db.image[ci].labels |= l;
  }
}

static inline void
dt_gui_label(int label)
{ // toggle label
  if(vkdt.view_mode == s_view_lighttable)
  {
    const uint32_t *sel = dt_db_selection_get(&vkdt.db);
    for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      vkdt.db.image[sel[i]].labels ^= 1<<(label-1);
  }
  else if(vkdt.view_mode == s_view_darkroom)
  {
    const uint32_t ci = dt_db_current_imgid(&vkdt.db);
    if(ci != -1u) vkdt.db.image[ci].labels ^= 1<<(label-1);
  }
}

static inline void dt_gui_rate_0() { dt_gui_rate(0); }
static inline void dt_gui_rate_1() { dt_gui_rate(1); }
static inline void dt_gui_rate_2() { dt_gui_rate(2); }
static inline void dt_gui_rate_3() { dt_gui_rate(3); }
static inline void dt_gui_rate_4() { dt_gui_rate(4); }
static inline void dt_gui_rate_5() { dt_gui_rate(5); }

static inline void dt_gui_label_1() { dt_gui_label(1); }
static inline void dt_gui_label_2() { dt_gui_label(2); }
static inline void dt_gui_label_3() { dt_gui_label(3); }
static inline void dt_gui_label_4() { dt_gui_label(4); }
static inline void dt_gui_label_5() { dt_gui_label(5); }

static inline void
dt_gui_dr_draw_position(
    float *n,          // image space coordinate
    float  pressure)   // pressure in [0, 1]
{
  float radius   = vkdt.wstate.state[0];
  float opacity  = vkdt.wstate.state[1];
  float hardness = vkdt.wstate.state[2];
  uint32_t *dat = (uint32_t *)vkdt.wstate.mapped;
  if(!dat) return; // paranoid segfault shield
  dt_draw_vert_t *vx = (dt_draw_vert_t *)(dat+1);
  if(pressure > 0.0f)
  {
    static struct timespec beg = {0};
    float xi = 2.0f*n[0] - 1.0f, yi = 2.0f*n[1] - 1.0f;
    if(dat[0])
    { // already have a vertex in the list
      if(beg.tv_sec)
      { // avoid spam
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);
        double dt = (double)(end.tv_sec - beg.tv_sec) + 1e-9*(end.tv_nsec - beg.tv_nsec);
        if(dt < 1.0/60.0) return; // draw at low frame rates
        beg = end;
      }
      else clock_gettime(CLOCK_REALTIME, &beg);
      // this cuts off at steps < ~0.005 of the image width
      const dt_draw_vert_t vo = vx[dat[0]-1];
      if(vo.x != 0 && vo.y != 0 && fabsf(vo.x - xi) < 0.005 && fabsf(vo.y - yi) < 0.005) return;
    }
    if(2*dat[0]+2 < vkdt.wstate.mapped_size/sizeof(uint32_t))
    { // add vertex
      int v = dat[0]++;
      vx[v] = dt_draw_vertex(xi, yi, pressure * radius, opacity, hardness);
    }
    // trigger draw list upload and recomputation:
    vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_upload_source | s_graph_run_wait_done;
    vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].flags = s_module_request_read_source;
  }
  else
  { // write endmarker
    if(dat[0] && dt_draw_vert_is_endmarker(vx[dat[0]-1])) return; // already have an endmarker
    if(2*dat[0]+2 < vkdt.wstate.mapped_size/sizeof(uint32_t))
      vx[dat[0]++] = dt_draw_endmarker();
  }
}
