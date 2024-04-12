#include "core/log.h"
#include "core/fs.h"
#include "core/core.h"
#include "qvk/qvk.h"
#include "gui/api.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/render.h"
#include "gui/darkroom.h"
#include "gui/widget_image.h"
#include "pipe/draw.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-defaults.h"
#include "pipe/modules/api.h"
#include "pipe/graph-history.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <libgen.h>
#include <limits.h>

void
darkroom_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  double x, y;
  glfwGetCursorPos(qvk.window, &x, &y);

  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type = 1,
      .x = x,
      .y = y,
      .mbutton = button,
      .action  = action,
      .mods    = mods,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(vkdt.wstate.active_widget_modid >= 0)
      if(mod->so->input) mod->so->input(mod, &p);
    return;
  }
}

void
darkroom_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  double x, y;
  glfwGetCursorPos(qvk.window, &x, &y);

  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type = 3,
      .dx = xoff,
      .dy = yoff,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(vkdt.wstate.active_widget_modid >= 0)
      if(mod->so->input) mod->so->input(mod, &p);
    return;
  }
}

void
darkroom_mouse_position(GLFWwindow* window, double x, double y)
{
  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type = 2,
      .x = x,
      .y = y,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(vkdt.wstate.active_widget_modid >= 0)
      if(mod->so->input) mod->so->input(mod, &p);
  }
}

void
darkroom_process()
{
  static int    start_frame = 0;           // frame we were when play was issued
  static struct timespec start_time = {0}; // start time of the same event

  struct timespec beg;
  clock_gettime(CLOCK_REALTIME, &beg);

  int advance = 0;
  if(vkdt.state.anim_playing)
  {
    if(vkdt.graph_dev.frame_rate == 0.0)
    {
      advance = 1; // no frame rate set, run as fast as we can
      vkdt.state.anim_frame = CLAMP(vkdt.graph_dev.frame + 1, 0, (uint32_t)vkdt.graph_dev.frame_cnt-1);
    }
    else
    { // just started replay, record timestamp:
      if(start_time.tv_nsec == 0)
      {
        start_time  = beg;
        start_frame = vkdt.state.anim_frame;
      }
      // compute current animation frame by time:
      double dt = (double)(beg.tv_sec - start_time.tv_sec) + 1e-9*(beg.tv_nsec - start_time.tv_nsec);
      vkdt.state.anim_frame = CLAMP(
          start_frame + MAX(0, vkdt.graph_dev.frame_rate * dt),
          0, (uint32_t)vkdt.graph_dev.frame_cnt-1);
      if(vkdt.graph_dev.frame > start_frame &&
         vkdt.graph_dev.frame == vkdt.state.anim_frame)
        vkdt.graph_dev.runflags = 0; // no need to re-render
      else advance = 1;
    }
    if(advance)
    {
      if(vkdt.state.anim_frame > vkdt.graph_dev.frame + 1)
        dt_log(s_log_snd, "frame drop warning, audio may stutter!");
      vkdt.graph_dev.frame = vkdt.state.anim_frame;
      if(!vkdt.state.anim_no_keyframes)
        dt_graph_apply_keyframes(&vkdt.graph_dev);
      if(vkdt.graph_dev.frame_cnt == 0 || vkdt.state.anim_frame < vkdt.state.anim_max_frame+1)
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    }
    if(vkdt.state.anim_frame == vkdt.graph_dev.frame_cnt - 1)
      dt_gui_dr_anim_stop(); // reached the end, stop.
  }
  else
  { // if no animation, reset time stamp
    start_time = (struct timespec){0};
  }

  int reset_view = 0;
  dt_roi_t old_roi;
  if(vkdt.graph_dev.runflags & s_graph_run_roi)
  {
    reset_view = 1;
    dt_node_t *md = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(md) old_roi = md->connector[0].roi;
  }
  if(vkdt.graph_dev.runflags && vkdt.state.anim_playing && advance)
    vkdt.graph_res = dt_graph_run(&vkdt.graph_dev, vkdt.graph_dev.runflags & ~s_graph_run_wait_done); // interleave cpu and gpu
  else if(vkdt.graph_dev.runflags)
    vkdt.graph_res = dt_graph_run(&vkdt.graph_dev, vkdt.graph_dev.runflags | s_graph_run_wait_done);  // wait
  if(reset_view)
  {
    dt_node_t *md = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(md && memcmp(&old_roi, &md->connector[0].roi, sizeof(dt_roi_t))) // did the output roi change?
      dt_image_reset_zoom(&vkdt.wstate.img_widget);
  }

  if(vkdt.state.anim_playing && advance)
  { // new frame for animations need new audio, too
    dt_graph_t *g = &vkdt.graph_dev;
    for(int i=0;i<g->num_modules;i++)
    { // find first audio module, if any
      if(g->module[i].name == 0) continue;
      if(g->module[i].so->audio)
      {
        uint16_t *samples;
        int cnt = g->module[i].so->audio(g->module+i, g->frame, &samples);
        if(cnt > 0) dt_snd_play(&vkdt.snd, samples, cnt);
        break;
      }
    }
  }

  // struct timespec end;
  // clock_gettime(CLOCK_REALTIME, &end);
  // double dt = (double)(end.tv_sec - beg.tv_sec) + 1e-9*(end.tv_nsec - beg.tv_nsec);
  // dt_log(s_log_perf, "frame time %2.3fs", dt);
}

int
darkroom_enter()
{
  vkdt.state.anim_frame = 0;
  dt_gui_dr_anim_stop();
  dt_image_reset_zoom(&vkdt.wstate.img_widget);
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.active_widget_parid = -1;
  vkdt.wstate.mapped = 0;
  vkdt.wstate.selected = -1;
  uint32_t imgid = dt_db_current_imgid(&vkdt.db);
  if(imgid == -1u) return 1;
  char graph_cfg[PATH_MAX+100];
  dt_db_image_path(&vkdt.db, imgid, graph_cfg, sizeof(graph_cfg));

  // stat, if doesn't exist, load default
  // always set filename param? (definitely do that for default cfg)
  int load_default = 0;
  char realimg[PATH_MAX];
  dt_token_t input_module = dt_token("i-raw");
  struct stat statbuf;
  if(stat(graph_cfg, &statbuf))
  {
    fs_realpath(graph_cfg, realimg); // depend on GNU extension in case of ENOENT (to cut out /../ and so on)
    int len = strlen(realimg);
    assert(len > 4);
    realimg[len-4] = 0; // cut away ".cfg"
    input_module = dt_graph_default_input_module(realimg);
    snprintf(graph_cfg, sizeof(graph_cfg), "default-darkroom.%"PRItkn, dt_token_str(input_module));
    load_default = 1;
  }

  dt_graph_init(&vkdt.graph_dev);
  vkdt.graph_dev.gui_attached = 1;
  dt_graph_history_init(&vkdt.graph_dev);

  if(dt_graph_read_config_ascii(&vkdt.graph_dev, graph_cfg))
  {
    dt_log(s_log_err|s_log_gui, "could not load graph configuration from '%s'!", graph_cfg);
    dt_graph_cleanup(&vkdt.graph_dev);
    return 2;
  }

  if(load_default)
  {
    dt_graph_set_searchpath(&vkdt.graph_dev, realimg);
    char *basen = fs_basename(realimg); // cut away path so we can relocate more easily
    int modid = dt_module_get(&vkdt.graph_dev, input_module, dt_token("main"));
    if(modid < 0 ||
       dt_module_set_param_string(vkdt.graph_dev.module + modid, dt_token("filename"),
         basen))
    {
      dt_log(s_log_err|s_log_gui, "config '%s' has no valid input module!", graph_cfg);
      dt_graph_cleanup(&vkdt.graph_dev);
      return 3;
    }
  }
  dt_graph_history_reset(&vkdt.graph_dev);

  if((vkdt.graph_res = dt_graph_run(&vkdt.graph_dev, s_graph_run_all)) != VK_SUCCESS)
    dt_gui_notification("running the graph failed (%s)!",
        qvk_result_to_string(vkdt.graph_res));

  // nodes are only constructed after running once
  // (could run up to s_graph_run_create_nodes)
  if(!dt_graph_get_display(&vkdt.graph_dev, dt_token("main")))
    dt_gui_notification("graph does not contain a display:main node!");

  // do this after running the graph, it may only know
  // after initing say the output roi, after loading an input file
  vkdt.state.anim_max_frame = vkdt.graph_dev.frame_cnt-1;

  // rebuild gui specific to this image
  dt_gui_read_favs("darkroom.ui");
#if 1//ndef QVK_ENABLE_VALIDATION // debug build does not reset zoom (reload shaders keeping focus is nice)
  dt_image_reset_zoom(&vkdt.wstate.img_widget);
#endif

  dt_gamepadhelp_set(dt_gamepadhelp_button_circle, "back to lighttable");
  dt_gamepadhelp_set(dt_gamepadhelp_button_square, "plus L1/R1: switch panel");
  dt_gamepadhelp_set(dt_gamepadhelp_ps, "display this help");
  dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_L, "pan around");
  dt_gamepadhelp_set(dt_gamepadhelp_L1, "previous image. anim: stop/reset");
  dt_gamepadhelp_set(dt_gamepadhelp_R1, "next image. anim: play");
  dt_gamepadhelp_set(dt_gamepadhelp_L2, "zoom out");
  dt_gamepadhelp_set(dt_gamepadhelp_R2, "zoom in. while holding L2: toggle fullscreen");
  dt_gamepadhelp_set(dt_gamepadhelp_L3, "reset zoom");
  dt_gamepadhelp_set(dt_gamepadhelp_R3, "reset focussed control");
  return 0;
}

int
darkroom_leave()
{
  dt_gui_dr_anim_stop();
  char filename[1024];
  dt_db_image_path(&vkdt.db, dt_db_current_imgid(&vkdt.db), filename, sizeof(filename));
  if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
    dt_graph_write_config_ascii(&vkdt.graph_dev, filename);

  if(vkdt.graph_dev.frame_cnt != 1) dt_gui_label_set(s_image_label_video);
  else dt_gui_label_unset(s_image_label_video);

  // TODO: start from already loaded/inited graph instead of from scratch!
  const uint32_t imgid = dt_db_current_imgid(&vkdt.db);
  dt_thumbnails_cache_list(
      &vkdt.thumbnail_gen,
      &vkdt.db,
      &imgid, 1,
      &glfwPostEmptyEvent);

  // TODO: repurpose instead of cleanup!
  dt_graph_cleanup(&vkdt.graph_dev);
  dt_graph_history_cleanup(&vkdt.graph_dev);
  vkdt.graph_res = VK_INCOMPLETE; // invalidate
  dt_gamepadhelp_clear();
  return 0;
}

void
darkroom_pentablet_data(double x, double y, double z, double pressure, double pitch, double yaw, double roll)
{
  dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  if(!out) return; // should never happen
  if(vkdt.wstate.active_widget_modid >= 0)
  {
    dt_token_t type = vkdt.graph_dev.module[
        vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(type == dt_token("draw"))
    {
      float v[] = {(float)x, (float)y}, n[2] = {0};
      dt_image_from_view(&vkdt.wstate.img_widget, v, n);
      if(glfwGetMouseButton(qvk.window, GLFW_MOUSE_BUTTON_MIDDLE) != GLFW_PRESS)
        dt_gui_dr_draw_position(n, pressure);
    }
  }
}
