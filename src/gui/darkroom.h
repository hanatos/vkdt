#pragma once

#include "core/log.h"
#include "core/core.h"
#include "gui/gui.h"
#include "gui/render.h" // for set_lod
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/modules/api.h"

#include <SDL.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// some static helper functions for the gui
static inline void
darkroom_handle_event(SDL_Event *event)
{
  dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  if(!out) return; // should never happen
  assert(out);
  float wd = (float)out->connector[0].roi.wd;
  float ht = (float)out->connector[0].roi.ht;
  static int m_x = -1, m_y = -1;
  static float old_look_x = -1.0f, old_look_y = -1.0f;
  if(event->type == SDL_MOUSEMOTION)
  {
    if(m_x >= 0 && vkdt.state.scale > 0.0f)
    {
      int dx = event->button.x - m_x;
      int dy = event->button.y - m_y;
      vkdt.state.look_at_x = old_look_x - dx / vkdt.state.scale;
      vkdt.state.look_at_y = old_look_y - dy / vkdt.state.scale;
      vkdt.state.look_at_x = CLAMP(vkdt.state.look_at_x, 0.0f, wd);
      vkdt.state.look_at_y = CLAMP(vkdt.state.look_at_y, 0.0f, ht);
    }
  }
  else if(event->type == SDL_MOUSEBUTTONUP)
  {
    m_x = m_y = -1;
  }
  else if(event->type == SDL_MOUSEBUTTONDOWN &&
      event->button.x < vkdt.state.center_x + vkdt.state.center_wd)
  {
    if(event->button.button == SDL_BUTTON_LEFT)
    {
      m_x = event->button.x;
      m_y = event->button.y;
      old_look_x = vkdt.state.look_at_x;
      old_look_y = vkdt.state.look_at_y;
    }
    else if(event->button.button == SDL_BUTTON_MIDDLE)
    {
      // TODO: zoom 1:1
      // TODO: two things: one is the display node which has
      // TODO: parameters to be set to zoom and wd/ht so it'll affect the ROI.
      // TODO: the other is zoom/pan in the gui for 200% or more and to
      // TODO: move the image smoothly
      // where does the mouse look in the current image?
      float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
      float scale = vkdt.state.scale <= 0.0f ? MIN(imwd/wd, imht/ht) : vkdt.state.scale;
      float im_x = (event->button.x - (vkdt.state.center_x + imwd)/2.0f) / scale;
      float im_y = (event->button.y - (vkdt.state.center_y + imht)/2.0f) / scale;
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
        vkdt.state.look_at_x = wd/2.0f;
        vkdt.state.look_at_y = ht/2.0f;
      }
      else if(vkdt.state.scale >= 1.0f)
      {
        vkdt.state.scale *= 2.0f;
        vkdt.state.look_at_x = im_x;
        vkdt.state.look_at_y = im_y;
      }
    }
  }
  else if (event->type == SDL_KEYDOWN)
  {
    if(event->key.keysym.sym == SDLK_r)
    {
      dt_view_switch(s_view_cnt);
      dt_pipe_global_cleanup();
      // this will crash on shutdown.
      // actually we'd have to shutdown and re-init thumbnails, too
      // because they hold a graph which holds pointers to global modules.
      // this would mean to re-init the db, too ..
      system("make debug"); // build shaders
      dt_pipe_global_init();
      dt_view_switch(s_view_darkroom);
    }
    else if(event->key.keysym.sym == SDLK_PERIOD)
    {
      dt_view_switch(s_view_lighttable);
    }
  }
}

static inline void
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

static inline int
darkroom_enter()
{
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
    int len = strlen(imgfilename);
    assert(len > 4);
    imgfilename[len-4] = 0; // cut away ".cfg"
    int modid = dt_module_get(&vkdt.graph_dev, dt_token("i-raw"), dt_token("01"));
    if(modid < 0 ||
       dt_module_set_param_string(vkdt.graph_dev.module + modid, dt_token("filename"),
         imgfilename))
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

static inline int
darkroom_leave()
{
  dt_graph_write_config_ascii(&vkdt.graph_dev, "shutdown.cfg");
  dt_graph_cleanup(&vkdt.graph_dev);
  return 0;
}
