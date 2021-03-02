#include "core/log.h"
#include "core/core.h"
#include "qvk/qvk.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/darkroom.h"
#include "gui/darkroom-util.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/modules/api.h"

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
  if(x >= vkdt.state.center_x + vkdt.state.center_wd) return; // over panel
  const float px_dist = 0.1*qvk.win_height;

  dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  if(!out) return; // should never happen
  assert(out);
  vkdt.wstate.wd = (float)out->connector[0].roi.wd;
  vkdt.wstate.ht = (float)out->connector[0].roi.ht;
  vkdt.wstate.selected = -1;

  if(vkdt.wstate.active_widget_modid >= 0)
  {
    dt_token_t type = vkdt.graph_dev.module[
        vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(type == dt_token("pers"))
    {
      if(action == GLFW_RELEASE)
      {
        vkdt.wstate.selected = -1;
      }
      else if(action == GLFW_PRESS)
      {
        // find active corner if close enough
        float m[] = {(float)x, (float)y};
        float max_dist = FLT_MAX;
        for(int cc=0;cc<4;cc++)
        {
          float n[] = {vkdt.wstate.state[2*cc+0], vkdt.wstate.state[2*cc+1]}, v[2];
          dt_image_to_view(n, v);
          float dist2 =
            (v[0]-m[0])*(v[0]-m[0])+
            (v[1]-m[1])*(v[1]-m[1]);
          if(dist2 < px_dist*px_dist)
          {
            if(dist2 < max_dist)
            {
              max_dist = dist2;
              vkdt.wstate.selected = cc;
            }
          }
        }
        if(max_dist < FLT_MAX) return;
      }
    }
    else if(type == dt_token("crop"))
    {
      if(action == GLFW_RELEASE)
      {
        vkdt.wstate.selected = -1;
      }
      else if(action == GLFW_PRESS)
      {
        // find active corner if close enough
        float m[2] = {(float)x, (float)y};
        float max_dist = FLT_MAX;
        for(int ee=0;ee<4;ee++)
        {
          float n[] = {ee < 2 ? vkdt.wstate.state[ee] : 0, ee >= 2 ? vkdt.wstate.state[ee] : 0}, v[2];
          dt_image_to_view(n, v);
          float dist2 =
            ee < 2 ?
            (v[0]-m[0])*(v[0]-m[0]) :
            (v[1]-m[1])*(v[1]-m[1]);
          if(dist2 < px_dist*px_dist)
          {
            if(dist2 < max_dist)
            {
              max_dist = dist2;
              vkdt.wstate.selected = ee;
            }
          }
        }
        if(max_dist < FLT_MAX) return;
      }
    }
    else if(type == dt_token("pick"))
    {
      if(button == GLFW_MOUSE_BUTTON_LEFT)
      {
        float v[] = {(float)x, (float)y}, n[2] = {0};
        dt_view_to_image(v, n);
        if(action == GLFW_RELEASE)
        {
          vkdt.wstate.state[0] = MIN(vkdt.wstate.state[0], n[0]);
          vkdt.wstate.state[1] = MAX(vkdt.wstate.state[1], n[0]);
          vkdt.wstate.state[2] = MIN(vkdt.wstate.state[2], n[1]);
          vkdt.wstate.state[3] = MAX(vkdt.wstate.state[3], n[1]);
        }
        else if(action == GLFW_PRESS)
        {
          vkdt.wstate.state[0] = n[0];
          vkdt.wstate.state[1] = n[0];
          vkdt.wstate.state[2] = n[1];
          vkdt.wstate.state[3] = n[1];
        }
        return;
      }
    }
    else if(type == dt_token("draw"))
    {
      uint32_t *dat = (uint32_t *)vkdt.wstate.mapped;
      if(action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_RIGHT)
      { // right mouse click resets the last stroke
        for(int i=dat[0]-1;i>=0;i--)
        {
          if(i == 0 || dat[1+2*i+1] == 0) // detected end marker
          {
            dat[0] = i; // reset count
            break;
          }
        }
        // trigger recomputation:
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
        vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].flags = s_module_request_read_source;
        return;
      }
      else if(action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
      { // left mouse click starts new stroke by appending an end marker
        int vcnt = dat[0];
        if(vcnt && (2*vcnt+2 < vkdt.wstate.mapped_size/sizeof(uint32_t)))
        {
          dat[1+2*vcnt+0] = 0; // vertex coordinate 16+16
          dat[1+2*vcnt+1] = 0; // radius 16 + opacity 8 + hardness 8, all 0 is the end marker
          dat[0]++;
        }
        // trigger recomputation:
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
        vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].flags = s_module_request_read_source;
        return;
      }
    }
  }

  if(action == GLFW_RELEASE)
  {
    vkdt.wstate.m_x = vkdt.wstate.m_y = -1;
  }
  else if(action == GLFW_PRESS &&
      x < vkdt.state.center_x + vkdt.state.center_wd)
  {
    if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
      vkdt.wstate.m_x = x;
      vkdt.wstate.m_y = y;
      vkdt.wstate.old_look_x = vkdt.state.look_at_x;
      vkdt.wstate.old_look_y = vkdt.state.look_at_y;
    }
    else if(button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
      // zoom 1:1
      // where does the mouse look in the current image?
      float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
      float scale = vkdt.state.scale <= 0.0f ? MIN(imwd/vkdt.wstate.wd, imht/vkdt.wstate.ht) : vkdt.state.scale;
      float im_x = (x - (vkdt.state.center_x + imwd)/2.0f);
      float im_y = (y - (vkdt.state.center_y + imht)/2.0f);
      if(vkdt.state.scale <= 0.0f)
      {
        vkdt.state.scale = 1.0f;
        const float dscale = 1.0f/scale - 1.0f/vkdt.state.scale;
        vkdt.state.look_at_x += im_x  * dscale;
        vkdt.state.look_at_y += im_y  * dscale;
      }
      else if(vkdt.state.scale >= 8.0f || vkdt.state.scale < 1.0f)
      {
        vkdt.state.scale = -1.0f;
        vkdt.state.look_at_x = vkdt.wstate.wd/2.0f;
        vkdt.state.look_at_y = vkdt.wstate.ht/2.0f;
      }
      else if(vkdt.state.scale >= 1.0f)
      {
        vkdt.state.scale *= 2.0f;
        const float dscale = 1.0f/scale - 1.0f/vkdt.state.scale;
        vkdt.state.look_at_x += im_x  * dscale;
        vkdt.state.look_at_y += im_y  * dscale;
      }
    }
  }
}

void
darkroom_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  double x, y;
  glfwGetCursorPos(qvk.window, &x, &y);
  if(x >= vkdt.state.center_x + vkdt.state.center_wd) return;

  // active widgets grabbed input?
  if(vkdt.wstate.active_widget_modid >= 0)
  {
    dt_token_t type =
      vkdt.graph_dev.module[
      vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(type == dt_token("draw"))
    {
      const float scale = yoff > 0.0 ? 1.2f : 1.0/1.2f;
      if(glfwGetKey(qvk.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) // opacity
        vkdt.wstate.state[1] = CLAMP(vkdt.wstate.state[1] * scale, 0.1f, 1.0f);
      else if(glfwGetKey(qvk.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) // hardness
        vkdt.wstate.state[2] = CLAMP(vkdt.wstate.state[2] * scale, 0.1f, 1.0f);
      else // radius
        vkdt.wstate.state[0] = CLAMP(vkdt.wstate.state[0] * scale, 0.1f, 1.0f);
      return; // don't zoom, we processed the input
    }
  }
  
  // zoom:
  {
    dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(!out) return; // should never happen
    assert(out);
    vkdt.wstate.wd = (float)out->connector[0].roi.wd;
    vkdt.wstate.ht = (float)out->connector[0].roi.ht;

    const float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
    const float fit_scale = MIN(imwd/vkdt.wstate.wd, imht/vkdt.wstate.ht);
    const float scale = vkdt.state.scale <= 0.0f ? fit_scale : vkdt.state.scale;
    const float im_x = (x - (vkdt.state.center_x + imwd)/2.0f);
    const float im_y = (y - (vkdt.state.center_y + imht)/2.0f);
    vkdt.state.scale = scale * (yoff > 0.0f ? 1.1f : 0.9f);
    vkdt.state.scale = CLAMP(vkdt.state.scale , 0.1f, 8.0f);
    if(vkdt.state.scale >= fit_scale)
    {
      const float dscale = 1.0f/scale - 1.0f/vkdt.state.scale;
      vkdt.state.look_at_x += im_x  * dscale;
      vkdt.state.look_at_y += im_y  * dscale;
    }
    else
    {
      vkdt.state.look_at_x = vkdt.wstate.wd/2.0f;
      vkdt.state.look_at_y = vkdt.wstate.ht/2.0f;
    }
  }
}

void
darkroom_mouse_position(GLFWwindow* window, double x, double y)
{
  if(x >= vkdt.state.center_x + vkdt.state.center_wd) return;
  if(vkdt.wstate.active_widget_modid >= 0)
  {
    // convert view space mouse coordinate to normalised image
    float v[] = {(float)x, (float)y}, n[2] = {0};
    dt_view_to_image(v, n);
    dt_token_t type =
      vkdt.graph_dev.module[
      vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(type == dt_token("pers"))
    {
      if(vkdt.wstate.selected >= 0)
      {
        // copy to quad state at corner c
        vkdt.wstate.state[2*vkdt.wstate.selected+0] = n[0];
        vkdt.wstate.state[2*vkdt.wstate.selected+1] = n[1];
        return;
      }
    }
    else if(type == dt_token("crop"))
    {
      if(vkdt.wstate.selected >= 0)
      {
        float edge = vkdt.wstate.selected < 2 ? n[0] : n[1];
        vkdt.wstate.state[vkdt.wstate.selected] = edge;
        if(vkdt.wstate.aspect > 0.0f)
        { // fix up aspect ratio
          float target_aspect = vkdt.wstate.aspect;
          if(vkdt.wstate.selected == 0)
          { // left, move right side along:
            vkdt.wstate.state[1] = vkdt.wstate.state[0] + target_aspect * (vkdt.wstate.state[3]-vkdt.wstate.state[2]);
          }
          else if(vkdt.wstate.selected == 2)
          { // bottom, move top side along:
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
          { // top, move left and right simultaneously to keep center:
            float c = (vkdt.wstate.state[1] + vkdt.wstate.state[0])*0.5f;
            float w =  vkdt.wstate.state[3] - vkdt.wstate.state[2];
            vkdt.wstate.state[1] = c + 0.5f * target_aspect * w;
            vkdt.wstate.state[0] = c - 0.5f * target_aspect * w;
          }
        }
        return;
      }
    }
    else if(type == dt_token("pick"))
    {
      if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
      {
        vkdt.wstate.state[0] = MIN(vkdt.wstate.state[0], n[0]);
        vkdt.wstate.state[1] = MAX(vkdt.wstate.state[1], n[0]);
        vkdt.wstate.state[2] = MIN(vkdt.wstate.state[2], n[1]);
        vkdt.wstate.state[3] = MAX(vkdt.wstate.state[3], n[1]);
        return; // we got this
      }
    }
    else if(type == dt_token("draw"))
    {
      if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
      {
        float radius   = vkdt.wstate.state[0];
        float opacity  = vkdt.wstate.state[1];
        float hardness = vkdt.wstate.state[2];
        uint32_t *dat = (uint32_t *)vkdt.wstate.mapped;
        uint16_t xi = CLAMP((int32_t)(n[0]*0xffff), 0, 0xffff);
        uint16_t yi = CLAMP((int32_t)(n[1]*0xffff), 0, 0xffff);
        if(dat[0])
        { // avoid spam
          uint16_t xo = dat[1+2*(dat[0]-1)+0]&0xffff;
          uint16_t yo = dat[1+2*(dat[0]-1)+0]>>16;
          // this cuts off at steps < ~0.005 of the image width
          if(xo != 0 && yo != 0 && abs(xo - xi) < 32 && abs(yo - yi) < 32) return;
        }
        if(2*dat[0]+2 < vkdt.wstate.mapped_size/sizeof(uint32_t))
        { // add vertex
          int v = dat[0]++;
          dat[1+2*v+0] = (yi << 16) | xi;
          dat[1+2*v+1] = CLAMP((int32_t)(0.5*radius*0xffff), 0, 0xffff) | (CLAMP((int32_t)(opacity*0xff), 0, 0xff) << 16) | (CLAMP((int32_t)(hardness*0xff), 0, 0xff) << 24);
        }
        // trigger recomputation:
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
        vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].flags = s_module_request_read_source;
        return;
      }
    }
  }
  if(vkdt.wstate.m_x > 0 && vkdt.state.scale > 0.0f)
  {
    int dx = x - vkdt.wstate.m_x;
    int dy = y - vkdt.wstate.m_y;
    vkdt.state.look_at_x = vkdt.wstate.old_look_x - dx / vkdt.state.scale;
    vkdt.state.look_at_y = vkdt.wstate.old_look_y - dy / vkdt.state.scale;
    vkdt.state.look_at_x = CLAMP(vkdt.state.look_at_x, 0.0f, vkdt.wstate.wd);
    vkdt.state.look_at_y = CLAMP(vkdt.state.look_at_y, 0.0f, vkdt.wstate.ht);
  }
}

// some static helper functions for the gui
void
darkroom_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS && key == GLFW_KEY_R)
  {
    // dt_view_switch(s_view_cnt);
    // view switching will not work because we're doing really wacky things here.
    // hence we call cleanup and below darkroom_enter() instead.
    dt_graph_cleanup(&vkdt.graph_dev);
    dt_pipe_global_cleanup();
    // this will crash on shutdown.
    // actually we'd have to shutdown and re-init thumbnails, too
    // because they hold a graph which holds pointers to global modules.
    // this would mean to re-init the db, too ..
    system("make debug"); // build shaders
    dt_pipe_global_init();
    dt_pipe.modules_reloaded = 1;
    darkroom_enter();
    // dt_view_switch(s_view_darkroom);
  }
  else if(action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_E))
  {
    dt_view_switch(s_view_lighttable);
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_SPACE)
  {
    if(vkdt.graph_dev.frame_cnt > 1)
      vkdt.state.anim_playing ^= 1; // start/stop playing animation
    else
    { // advance to next image in lighttable collection
      uint32_t next = dt_db_current_colid(&vkdt.db) + 1;
      if(next <= vkdt.db.collection_cnt)
      {
        dt_db_selection_add(&vkdt.db, next);
        // darkroom_leave(); // writes back thumbnails. maybe there'd be a cheaper way to invalidate.
        dt_graph_cleanup(&vkdt.graph_dev); // essential to free memory
        darkroom_enter();
      }
    }
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_BACKSPACE)
  {
    if(vkdt.graph_dev.frame_cnt > 1)
      vkdt.graph_dev.frame = vkdt.state.anim_frame = 0; // reset to beginning
    else
    { // backtrack to last image in lighttable collection
      uint32_t next = dt_db_current_colid(&vkdt.db) - 1;
      if(next >= 0)
      {
        dt_db_selection_add(&vkdt.db, next);
        // darkroom_leave(); // writes back thumbnails. maybe there'd be a cheaper way to invalidate.
        dt_graph_cleanup(&vkdt.graph_dev);
        darkroom_enter();
      }
    }
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
      vkdt.state.anim_frame = CLAMP(vkdt.graph_dev.frame + 1, 0, vkdt.graph_dev.frame_cnt-1);
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
          0, vkdt.graph_dev.frame_cnt-1);
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
      if(vkdt.state.anim_frame < vkdt.state.anim_max_frame)
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
    }
  }
  else
  { // if no animation, reset time stamp
    start_time = (struct timespec){0};
  }

  if(vkdt.graph_dev.runflags)
    dt_graph_run(&vkdt.graph_dev,
        vkdt.graph_dev.runflags | s_graph_run_wait_done);

  if(vkdt.state.anim_playing && advance)
  { // new frame for animations need new audio, too
    dt_graph_t *g = &vkdt.graph_dev;
    for(int i=0;i<g->num_modules;i++)
    { // find first audio module, if any
      if(g->module[i].so->audio)
      {
        uint16_t *samples;
        int cnt = g->module[i].so->audio(g->module+i, g->frame, &samples);
        if(cnt > 0) dt_snd_play(&vkdt.snd, samples, cnt);
        break;
      }
    }
  }

  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end);
  double dt = (double)(end.tv_sec - beg.tv_sec) + 1e-9*(end.tv_nsec - beg.tv_nsec);
  dt_log(s_log_perf, "frame time %2.3fs", dt);
}

int
darkroom_enter()
{
  vkdt.state.anim_frame = 0;
  vkdt.state.anim_playing = 0;
  vkdt.wstate.m_x = vkdt.wstate.m_y = -1;
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.active_widget_parid = -1;
  vkdt.wstate.mapped = 0;
  vkdt.wstate.selected = -1;
  uint32_t imgid = dt_db_current_imgid(&vkdt.db);
  if(imgid == -1u) return 1;
  char graph_cfg[2048];
  dt_db_image_path(&vkdt.db, imgid, graph_cfg, sizeof(graph_cfg));

  // stat, if doesn't exist, load default
  // always set filename param? (definitely do that for default cfg)
  int load_default = 0;
  char imgfilename[256], realimg[PATH_MAX];
  dt_token_t input_module = dt_token("i-raw");
  struct stat statbuf;
  if(stat(graph_cfg, &statbuf))
  {
    dt_log(s_log_err|s_log_gui, "individual config %s not found, loading default!", graph_cfg);
    dt_db_image_path(&vkdt.db, imgid, imgfilename, sizeof(imgfilename));
    realpath(imgfilename, realimg);
    int len = strlen(realimg);
    assert(len > 4);
    realimg[len-4] = 0; // cut away ".cfg"
    if(!strcasecmp(realimg+len-8, ".mlv"))
      input_module = dt_token("i-mlv");
    if(!strcasecmp(realimg+len-8, ".pfm"))
      input_module = dt_token("i-pfm");
    snprintf(graph_cfg, sizeof(graph_cfg), "%s/default-darkroom.%"PRItkn, dt_pipe.basedir, dt_token_str(input_module));
    load_default = 1;
  }

  dt_graph_init(&vkdt.graph_dev);
  vkdt.graph_dev.gui_attached = 1;

  if(dt_graph_read_config_ascii(&vkdt.graph_dev, graph_cfg))
  {
    dt_log(s_log_err|s_log_gui, "could not load graph configuration from '%s'!", graph_cfg);
    dt_graph_cleanup(&vkdt.graph_dev);
    return 2;
  }

  if(load_default)
  {
    dt_graph_set_searchpath(&vkdt.graph_dev, realimg);
    char *basen = basename(realimg); // cut away path so we can relocate more easily
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


  VkResult err = 0;
  if((err = dt_graph_run(&vkdt.graph_dev, s_graph_run_all)) != VK_SUCCESS)
  {
    // TODO: could consider VK_TIMEOUT which sometimes happens on old intel
    dt_log(s_log_err|s_log_gui, "running the graph failed (%s)!",
        qvk_result_to_string(err));
    dt_graph_cleanup(&vkdt.graph_dev);
    return 4;
  }

  // nodes are only constructed after running once
  // (could run up to s_graph_run_create_nodes)
  if(!dt_graph_get_display(&vkdt.graph_dev, dt_token("main")))
  {
    dt_log(s_log_err|s_log_gui, "graph does not contain a display:main node!");
    dt_graph_cleanup(&vkdt.graph_dev);
    return 5;
  }

  // do this after running the graph, it may only know
  // after initing say the output roi, after loading an input file
  vkdt.state.anim_max_frame = vkdt.graph_dev.frame_cnt;

  // rebuild gui specific to this image
  dt_gui_read_favs("darkroom.ui");
  return 0;
}

int
darkroom_leave()
{
  char filename[1024];
  dt_db_image_path(&vkdt.db, dt_db_current_imgid(&vkdt.db), filename, sizeof(filename));
  if(!strstr(vkdt.db.dirname, "examples") && !strstr(filename, "examples"))
    dt_graph_write_config_ascii(&vkdt.graph_dev, filename);

  // TODO: start from already loaded/inited graph instead of from scratch!
  const uint32_t imgid = dt_db_current_imgid(&vkdt.db);
  dt_thumbnails_cache_list(
      &vkdt.thumbnail_gen,
      &vkdt.db,
      &imgid, 1);

  // TODO: repurpose instead of cleanup!
  dt_graph_cleanup(&vkdt.graph_dev);
  return 0;
}
