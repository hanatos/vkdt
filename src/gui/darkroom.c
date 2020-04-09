#include "core/log.h"
#include "core/core.h"
#include "qvk/qvk.h"
#include "gui/gui.h"
#include "gui/render.h" // for set_lod
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

void
darkroom_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  double x, y;
  glfwGetCursorPos(qvk.window, &x, &y);
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
    if(type == dt_token("quad"))
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
    else if(type == dt_token("axquad"))
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
    else if(type == dt_token("draw"))
    {
      // record mouse position relative to image
      // append to state until 1000 lines
      if(action == GLFW_RELEASE)
      {
        vkdt.wstate.selected = -1;
        return;
      }
      if(action == GLFW_PRESS)
      {
        if(vkdt.wstate.selected < 0) vkdt.wstate.selected = 0;
        // right click: remove stroke
        if(button == GLFW_MOUSE_BUTTON_RIGHT) vkdt.wstate.selected = -1;
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
      // TODO: zoom 1:1
      // TODO: two things: one is the display node which has
      // TODO: parameters to be set to zoom and wd/ht so it'll affect the ROI.
      // TODO: the other is zoom/pan in the gui for 200% or more and to
      // TODO: move the image smoothly
      // where does the mouse look in the current image?
      float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
      float scale = vkdt.state.scale <= 0.0f ? MIN(imwd/vkdt.wstate.wd, imht/vkdt.wstate.ht) : vkdt.state.scale;
      float im_x = (x - (vkdt.state.center_x + imwd)/2.0f) / scale;
      float im_y = (y - (vkdt.state.center_y + imht)/2.0f) / scale;
      im_x += vkdt.state.look_at_x;
      im_y += vkdt.state.look_at_y;
      if(vkdt.state.scale <= 0.0f)
      {
        vkdt.state.scale = 1.0f;
        vkdt.state.look_at_x = im_x;
        vkdt.state.look_at_y = im_y;
      }
      else if(vkdt.state.scale >= 8.0f)
      {
        vkdt.state.scale = -1.0f;
        vkdt.state.look_at_x = vkdt.wstate.wd/2.0f;
        vkdt.state.look_at_y = vkdt.wstate.ht/2.0f;
      }
      else if(vkdt.state.scale >= 1.0f)
      {
        vkdt.state.scale *= 2.0f;
        vkdt.state.look_at_x = im_x;
        vkdt.state.look_at_y = im_y;
      }
    }
  }
}

void
darkroom_mouse_position(GLFWwindow* window, double x, double y)
{
  if(vkdt.wstate.active_widget_modid >= 0)
  {
    // convert view space mouse coordinate to normalised image
    float v[] = {(float)x, (float)y}, n[2] = {0};
    dt_view_to_image(v, n);
    dt_token_t type = 
      vkdt.graph_dev.module[
      vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type;
    if(type == dt_token("quad"))
    {
      if(vkdt.wstate.selected >= 0)
      {
        // copy to quad state at corner c
        vkdt.wstate.state[2*vkdt.wstate.selected+0] = n[0];
        vkdt.wstate.state[2*vkdt.wstate.selected+1] = n[1];
        return;
      }
    }
    else if(type == dt_token("axquad"))
    {
      if(vkdt.wstate.selected >= 0)
      {
        float edge = vkdt.wstate.selected < 2 ? n[0] : n[1];
        vkdt.wstate.state[vkdt.wstate.selected] = edge;
        return;
      }
    }
    else if(type == dt_token("draw"))
    {
      if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
          vkdt.wstate.selected >= 0 && vkdt.wstate.selected < 2004)
      {
        // copy to quad state at corner vkdt.wstate.selected
        vkdt.wstate.mapped[1+2*vkdt.wstate.selected+0] = n[0];
        vkdt.wstate.mapped[1+2*vkdt.wstate.selected+1] = n[1];
        if(vkdt.wstate.selected == 0 || (vkdt.wstate.selected > 0 &&
              fabsf(n[0] - vkdt.wstate.mapped[1+2*(vkdt.wstate.selected-1)+0]) > 0.004 &&
              fabsf(n[1] - vkdt.wstate.mapped[1+2*(vkdt.wstate.selected-1)+1]) > 0.004))
          vkdt.wstate.mapped[0] = vkdt.wstate.selected++;
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
    darkroom_enter();
    // dt_view_switch(s_view_darkroom);
  }
  else if(action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
  {
    dt_view_switch(s_view_lighttable);
  }
}

void
darkroom_process()
{
  // intel says:
  // ==
  // The pipeline is flushed when switching between 3D graphics rendering and
  // compute functions. Asynchronous compute functions are not supported at
  // this time. Batch the compute kernels into groups whenever possible.
  // ==
  // which is unfortunate for us :/

  // VkResult fence = vkGetFenceStatus(qvk.device, vkdt.graph_dev.command_fence);
  // if(fence == VK_SUCCESS)
  if(vkdt.graph_dev.runflags)
    // VkResult err =
    dt_graph_run(&vkdt.graph_dev,
        vkdt.graph_dev.runflags
        | s_graph_run_wait_done);    // if we don't wait we can't resubmit because the fence would be used twice.
  // if(err != VK_SUCCESS) break;
}

int
darkroom_enter()
{
  vkdt.wstate.m_x = vkdt.wstate.m_y = -1;
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.active_widget_parid = -1;
  vkdt.wstate.mapped = 0;
  vkdt.wstate.selected = -1;
  uint32_t imgid = vkdt.db.current_image;
  if(imgid == -1u) return 1;
  char graph_cfg[2048];
  snprintf(graph_cfg, sizeof(graph_cfg), "%s", vkdt.db.image[imgid].filename);

  // stat, if doesn't exist, load default
  // always set filename param? (definitely do that for default cfg)
  int load_default = 0;
  struct stat statbuf;
  if(stat(graph_cfg, &statbuf))
  {
    dt_log(s_log_err|s_log_gui, "individual config %s not found, loading default!", graph_cfg);
    snprintf(graph_cfg, sizeof(graph_cfg), "default-darkroom.cfg");
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

  // TODO: support other file formats instead of just raw?
  if(load_default)
  {
    char imgfilename[256];
    snprintf(imgfilename, sizeof(imgfilename), "%s", vkdt.db.image[imgid].filename);
    snprintf(vkdt.graph_dev.searchpath, sizeof(vkdt.graph_dev.searchpath), "%s", dirname(imgfilename));
    snprintf(imgfilename, sizeof(imgfilename), "%s", vkdt.db.image[imgid].filename);
    int len = strlen(imgfilename);
    assert(len > 4);
    imgfilename[len-4] = 0; // cut away ".cfg"
    char *basen = basename(imgfilename); // cut away path o we can relocate more easily
    int modid = dt_module_get(&vkdt.graph_dev, dt_token("i-raw"), dt_token("01"));
    if(modid < 0 ||
       dt_module_set_param_string(vkdt.graph_dev.module + modid, dt_token("filename"),
         basen))
    {
      dt_log(s_log_err|s_log_gui, "config '%s' has no raw input module!", graph_cfg);
      dt_graph_cleanup(&vkdt.graph_dev);
      return 3;
    }
  }

  vkdt.state.anim_max_frame = vkdt.graph_dev.frame_cnt;
  // XXX remove: super ugly hack
  dt_gui_set_lod(0);

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

  // rebuild gui specific to this image
  dt_gui_read_favs("darkroom.ui");
  return 0;
}

int
darkroom_leave()
{
  dt_image_t *img = vkdt.db.image + vkdt.db.current_image;
  if(!strstr(img->filename, "examples"))
    dt_graph_write_config_ascii(&vkdt.graph_dev, img->filename);
  // TODO: start from already loaded/inited graph instead of from scratch!
  QVK(vkDeviceWaitIdle(qvk.device)); // we won't need this then (guards the reset call inside)
  (void)dt_thumbnails_cache_one(
      &vkdt.graph_dev,
      &vkdt.thumbnails,
      img->filename);
  dt_thumbnails_load_one(&vkdt.thumbnails, img->filename, &img->thumbnail);
  // TODO: repurpose instead of cleanup!
  dt_graph_cleanup(&vkdt.graph_dev);
  return 0;
}
