#pragma once
#include "widget_image.h"
#include "api.hh"
// widget for window-size image with zoom/pan interaction

inline void
dt_image_events(dt_image_widget_t *w, bool hovered, int main)
{
  bool do_events = true;
  if(main && vkdt.wstate.active_widget_modid >= 0)
  { // on-canvas widget interaction
    do_events = false; // by default disable std events (zoom/pan) if there is overlay
    switch(vkdt.graph_dev.module[
        vkdt.wstate.active_widget_modid].so->param[
        vkdt.wstate.active_widget_parid]->widget.type)
    { // distinguish by type:
      case dt_token("pers"):
      {
        float *v = vkdt.wstate.state;
        float p[8];
        for(int k=0;k<4;k++)
          dt_image_to_view(&vkdt.wstate.img_widget, v+2*k, p+2*k);
        ImGui::GetWindowDrawList()->AddPolyline(
            (ImVec2 *)p, 4, IM_COL32_WHITE, true, 1.0);
        if(vkdt.wstate.selected >= 0)
        {
          float q[2] = {
            p[2*vkdt.wstate.selected],
            p[2*vkdt.wstate.selected+1]};
          ImGui::GetWindowDrawList()->AddCircleFilled(
              ImVec2(q[0],q[1]), 0.02 * vkdt.state.center_wd,
              0x77777777u, 20);
          if(hovered && ImGui::IsKeyDown(ImGuiKey_MouseLeft))
          {
            ImVec2 pos = ImGui::GetMousePos();
            float v[] = {pos.x, pos.y}, n[2] = {0};
            dt_image_from_view(&vkdt.wstate.img_widget, v, n);
            dt_gui_dr_pers_adjust(n, 0);
          }
        }
        if(ImGui::IsKeyReleased(ImGuiKey_MouseLeft))
        {
          vkdt.wstate.selected = -1;
        }
        if(hovered && ImGui::IsKeyPressed(ImGuiKey_MouseLeft, false))
        { // find active corner if close enough
          ImVec2 pos = ImGui::GetMousePos();
          float m[] = {pos.x, pos.y};
          float max_dist = FLT_MAX;
          const float px_dist = 0.1*qvk.win_height;
          for(int cc=0;cc<4;cc++)
          {
            float n[] = {vkdt.wstate.state[2*cc+0], vkdt.wstate.state[2*cc+1]}, v[2];
            dt_image_to_view(&vkdt.wstate.img_widget, n, v);
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
        }
        break;
      }
      case dt_token("straight"):
      {
        ImVec2 pos = ImGui::GetMousePos();
        if(vkdt.wstate.selected >= 0)
        {
          float v[4] = { vkdt.wstate.state[1], vkdt.wstate.state[2], vkdt.wstate.state[3], vkdt.wstate.state[4] };
          ImGui::GetWindowDrawList()->AddPolyline(
              (ImVec2 *)v, 2, IM_COL32_WHITE, false, 1.0);
          if(vkdt.wstate.selected > 0)
          {
            vkdt.wstate.state[3] = pos.x;
            vkdt.wstate.state[4] = pos.y;
          }
        }
        if(hovered && ImGui::IsKeyReleased(ImGuiKey_MouseLeft))
        {
          vkdt.wstate.selected = 0;
          float dx = pos.x - vkdt.wstate.state[1];
          float dy = pos.y - vkdt.wstate.state[2];
          float a = atan2f(dy, dx);
          if(fabsf(dx) > fabsf(dy)) a =        + a * 180.0f/M_PI;
          else                      a = 270.0f + a * 180.0f/M_PI;
          if(fabsf(a + 180.0f) < fabsf(a)) a += 180.0f;
          if(fabsf(a - 180.0f) < fabsf(a)) a -= 180.0f;
          if(fabsf(a + 180.0f) < fabsf(a)) a += 180.0f;
          if(fabsf(a - 180.0f) < fabsf(a)) a -= 180.0f;
          vkdt.wstate.state[0] += a;
        }
        if(hovered && ImGui::IsKeyPressed(ImGuiKey_MouseLeft, false))
        {
          vkdt.wstate.state[1] = vkdt.wstate.state[3] = pos.x;
          vkdt.wstate.state[2] = vkdt.wstate.state[4] = pos.y;
          vkdt.wstate.selected = 1;
        }
        break;
      }
      case dt_token("crop"):
      {
        float v[8] = {
          vkdt.wstate.state[0], vkdt.wstate.state[2], vkdt.wstate.state[1], vkdt.wstate.state[2], 
          vkdt.wstate.state[1], vkdt.wstate.state[3], vkdt.wstate.state[0], vkdt.wstate.state[3]
        };
        float p[8];
        for(int k=0;k<4;k++) dt_image_to_view(&vkdt.wstate.img_widget, v+2*k, p+2*k);
        ImGui::GetWindowDrawList()->AddPolyline(
            (ImVec2 *)p, 4, IM_COL32_WHITE, true, 1.0);
        if(!ImGui::IsKeyDown(ImGuiKey_MouseLeft))
          vkdt.wstate.selected = -1;
        if(vkdt.wstate.selected >= 0)
        {
          float o = vkdt.state.center_wd * 0.02;
          float q0[8] = { p[0], p[1], p[2], p[3], p[2],   p[3]-o, p[0],   p[1]-o};
          float q1[8] = { p[2], p[3], p[4], p[5], p[4]+o, p[5],   p[2]+o, p[3]};
          float q2[8] = { p[4], p[5], p[6], p[7], p[6],   p[7]+o, p[4],   p[5]+o};
          float q3[8] = { p[6], p[7], p[0], p[1], p[0]-o, p[1],   p[6]-o, p[7]};
          float *q = q0;
          if(vkdt.wstate.selected == 0) q = q3;
          if(vkdt.wstate.selected == 1) q = q1;
          if(vkdt.wstate.selected == 2) q = q0;
          if(vkdt.wstate.selected == 3) q = q2;
          ImGui::GetWindowDrawList()->AddQuadFilled(
              ImVec2(q[0],q[1]), ImVec2(q[2],q[3]),
              ImVec2(q[4],q[5]), ImVec2(q[6],q[7]), 0x77777777u);

          ImVec2 pos = ImGui::GetMousePos();
          float v[] = {pos.x, pos.y}, n[2] = {0};
          dt_image_from_view(&vkdt.wstate.img_widget, v, n);
          float edge = vkdt.wstate.selected < 2 ? n[0] : n[1];
          dt_gui_dr_crop_adjust(edge, 0);
        }
        else do_events = true; // enable zoom/panning around
        if(hovered && ImGui::IsKeyPressed(ImGuiKey_MouseLeft, false))
        {
          ImVec2 pos = ImGui::GetMousePos();
          float m[2] = {(float)pos.x, (float)pos.y};
          float max_dist = FLT_MAX;
          const float px_dist = 0.1*qvk.win_height;
          for(int ee=0;ee<4;ee++)
          {
            float n[] = {ee < 2 ? vkdt.wstate.state[ee] : 0, ee >= 2 ? vkdt.wstate.state[ee] : 0}, v[2];
            dt_image_to_view(&vkdt.wstate.img_widget, n, v);
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
        }
        break;
      }
      case dt_token("pick"):
      {
        float v[8] = {
          vkdt.wstate.state[0], vkdt.wstate.state[2], vkdt.wstate.state[1], vkdt.wstate.state[2], 
          vkdt.wstate.state[1], vkdt.wstate.state[3], vkdt.wstate.state[0], vkdt.wstate.state[3]
        };
        float p[8];
        for(int k=0;k<4;k++)
          dt_image_to_view(&vkdt.wstate.img_widget, v+2*k, p+2*k);
        ImGui::GetWindowDrawList()->AddPolyline(
            (ImVec2 *)p, 4, IM_COL32_WHITE, true, 1.0);
        ImVec2 pos = ImGui::GetMousePos();
        float vv[] = {pos.x, pos.y}, n[2] = {0};
        dt_image_from_view(&vkdt.wstate.img_widget, vv, n);
        if(hovered && ImGui::IsKeyPressed(ImGuiKey_MouseLeft, false))
        {
          vkdt.wstate.state[0] = n[0];
          vkdt.wstate.state[1] = n[0];
          vkdt.wstate.state[2] = n[1];
          vkdt.wstate.state[3] = n[1];
        }
        if(hovered && ImGui::IsKeyDown(ImGuiKey_MouseLeft))
        {
          vkdt.wstate.state[0] = MIN(vkdt.wstate.state[0], n[0]);
          vkdt.wstate.state[1] = MAX(vkdt.wstate.state[1], n[0]);
          vkdt.wstate.state[2] = MIN(vkdt.wstate.state[2], n[1]);
          vkdt.wstate.state[3] = MAX(vkdt.wstate.state[3], n[1]);
        }
        break;
      }
      case dt_token("draw"):
      { // draw indicators for opacity/hardness/radius for each stroke
        ImVec2 pos = ImGui::GetMousePos();
        float radius   = vkdt.wstate.state[0];
        float opacity  = vkdt.wstate.state[1];
        float hardness = vkdt.wstate.state[2];
        float p[100];
        int cnt = sizeof(p)/sizeof(p[0])/2;

        const int modid = vkdt.wstate.active_widget_modid;
        const float scale = vkdt.wstate.img_widget.scale <= 0.0f ?
          MIN(vkdt.state.center_wd / (float)
              vkdt.graph_dev.module[modid].connector[0].roi.wd,
              vkdt.state.center_ht / (float)
              vkdt.graph_dev.module[modid].connector[0].roi.ht) :
          vkdt.wstate.img_widget.scale;
        for(int i=0;i<2;i++)
        { // ui scaled roi wd * radius * stroke radius
          float r = vkdt.wstate.state[4] * vkdt.wstate.state[3] * radius;
          if(i >= 1) r *= hardness;
          for(int k=0;k<cnt;k++)
          {
            p[2*k+0] = pos.x + scale * r*sinf(k/(cnt-1.0)*M_PI*2.0);
            p[2*k+1] = pos.y + scale * r*cosf(k/(cnt-1.0)*M_PI*2.0);
          }
          ImGui::GetWindowDrawList()->AddPolyline(
              (ImVec2 *)p, cnt, IM_COL32_WHITE, false, 4.0f/(i+1.0f));
        }
        // opacity is a quad
        float sz = 30.0f;
        p[0] = pos.x; p[1] = pos.y;
        p[4] = pos.x+sz; p[5] = pos.y+sz;
        p[2] = opacity * (pos.x+sz) + (1.0f-opacity)*(pos.x+sz/2.0f);
        p[3] = opacity *  pos.y     + (1.0f-opacity)*(pos.y+sz/2.0f);
        p[6] = opacity *  pos.x     + (1.0f-opacity)*(pos.x+sz/2.0f);
        p[7] = opacity * (pos.y+sz) + (1.0f-opacity)*(pos.y+sz/2.0f);
        ImGui::GetWindowDrawList()->AddPolyline(
            (ImVec2 *)p, 4, IM_COL32_WHITE, true, 3.0);

        uint32_t *dat = (uint32_t *)vkdt.wstate.mapped;
        if(hovered && ImGui::IsKeyPressed(ImGuiKey_MouseRight, false))
        { // right mouse click resets the last stroke
          for(int i=dat[0]-2;i>=0;i--)
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
        }
        if(hovered && ImGui::IsKeyDown(ImGuiKey_MouseWheelY))
        {
          float yoff = ImGui::GetIO().MouseWheel;
          const float scale = yoff > 0.0 ? 1.2f : 1.0/1.2f;
          if(ImGui::IsKeyDown(ImGuiKey_LeftShift)) // opacity
            vkdt.wstate.state[1] = CLAMP(vkdt.wstate.state[1] * scale, 0.1f, 1.0f);
          else if(ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) // hardness
            vkdt.wstate.state[2] = CLAMP(vkdt.wstate.state[2] * scale, 0.1f, 1.0f);
          else // radius
            vkdt.wstate.state[0] = CLAMP(vkdt.wstate.state[0] * scale, 0.1f, 1.0f);
        }
        float v[] = {pos.x, pos.y}, n[2] = {0};
        dt_image_from_view(&vkdt.wstate.img_widget, v, n);
        if(hovered && ImGui::IsKeyDown(ImGuiKey_MouseLeft))
          dt_gui_dr_draw_position(n, 1.0f);
        else
          dt_gui_dr_draw_position(n, 0.0f);
        break;
      }
      default:;
    }
  }

  if(!do_events) return; // on-canvas got the interaction

  if(hovered)
  { // zoom/pan interaction
    ImVec2 mpos = ImGui::GetMousePos();
    if(ImGui::IsKeyReleased(ImGuiKey_MouseLeft) ||
       ImGui::IsKeyReleased(ImGuiKey_MouseMiddle))
    {
      w->m_x = w->m_y = -1;
    }
    if((!vkdt.wstate.pentablet_enabled && ImGui::IsKeyPressed(ImGuiKey_MouseLeft, false)) ||
        ( vkdt.wstate.pentablet_enabled && ImGui::IsKeyPressed(ImGuiKey_MouseMiddle, false)))
    { // click to pan
      w->m_x = mpos.x;
      w->m_y = mpos.y;
      w->old_look_x = w->look_at_x;
      w->old_look_y = w->look_at_y;
    }
    else if(!vkdt.wstate.pentablet_enabled && ImGui::IsKeyPressed(ImGuiKey_MouseMiddle, false))
    { // middle click to 1:1
      dt_image_zoom(w, mpos.x, mpos.y);
    }
    if(ImGui::IsKeyDown(ImGuiKey_MouseWheelY))
    { // mouse wheel (yoff is the input)
      float yoff = ImGui::GetIO().MouseWheel;
      const float fit_scale = MIN(w->win_w/w->wd, w->win_h/w->ht);
      const float scale = w->scale <= 0.0f ? fit_scale : w->scale;
      const float im_x = mpos.x - (w->win_x + w->win_w/2.0f);
      const float im_y = mpos.y - (w->win_y + w->win_h/2.0f);
      w->scale = scale * (yoff > 0.0f ? 1.1f : 0.9f);
      w->scale = CLAMP(w->scale, 0.1f, 8.0f);
      if(w->scale > fit_scale)
      { // try to place mouse after zoom on same spot:
        // look_at + im / scale is mouse in old zoom image space
        // that - im / new scale should be the new center
        const float dscale = 1.0f/scale - 1.0f/w->scale;
        w->look_at_x += im_x * dscale;
        w->look_at_y += im_y * dscale;
        w->look_at_x = CLAMP(w->look_at_x, 0.0f, w->wd);
        w->look_at_y = CLAMP(w->look_at_y, 0.0f, w->ht);
      }
      else
      {
        w->look_at_x = w->wd/2.0f;
        w->look_at_y = w->ht/2.0f;
      }
    }
  }
  else
  { // reset stuff, but enable drag outside:
    if(!ImGui::IsKeyDown(ImGuiKey_MouseLeft) &&
       !ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
    {
      w->m_x = w->m_y = -1;
    }
  }
  if(w->m_x > 0 && w->scale > 0.0f)
  { // mouse moved while dragging
    ImVec2 mpos = ImGui::GetMousePos();
    ImVec2 d = ImVec2(mpos.x - w->m_x, mpos.y - w->m_y);
    w->look_at_x = w->old_look_x - d.x / w->scale;
    w->look_at_y = w->old_look_y - d.y / w->scale;
    w->look_at_x = CLAMP(w->look_at_x, 0.0f, w->wd);
    w->look_at_y = CLAMP(w->look_at_y, 0.0f, w->ht);
  }
}

inline void
dt_image(
    dt_image_widget_t *w,
    dt_node_t         *out,     // make sure the out->dset is valid!
    int                events,  // if !=0 provide zoom/pan interaction
    int                main)    // if !=0 do on-canvas ui elements
{
  if(!out) return;
  w->out = out;
  w->wd = (float)out->connector[0].roi.wd;
  w->ht = (float)out->connector[0].roi.ht;
  w->win_x = ImGui::GetWindowPos().x;  w->win_y = ImGui::GetWindowPos().y;
  w->win_w = ImGui::GetWindowSize().x; w->win_h = ImGui::GetWindowSize().y;
  ImTextureID imgid = out->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES];
  float im0[2], im1[2];
  float v0[2] = {w->win_x, w->win_y};
  float v1[2] = {w->win_x+w->win_w, w->win_y+w->win_h};
  dt_image_from_view(w, v0, im0);
  dt_image_from_view(w, v1, im1);
  im0[0] = CLAMP(im0[0], 0.0f, 1.0f);
  im0[1] = CLAMP(im0[1], 0.0f, 1.0f);
  im1[0] = CLAMP(im1[0], 0.0f, 1.0f);
  im1[1] = CLAMP(im1[1], 0.0f, 1.0f);
  dt_image_to_view(w, im0, v0);
  dt_image_to_view(w, im1, v1);
  ImGui::GetWindowDrawList()->AddImage(
      imgid, ImVec2(v0[0], v0[1]), ImVec2(v1[0], v1[1]),
      ImVec2(im0[0], im0[1]), ImVec2(im1[0], im1[1]), IM_COL32_WHITE);
  char scaletext[10];
  if(w->scale >= 1.0f)
  {
    snprintf(scaletext, sizeof(scaletext), "%d%%", (int)(w->scale*100.0));
    // XXX something scaled by fontsize!
    ImGui::GetWindowDrawList()->AddText(ImVec2(w->win_x+w->win_w-120.0f, w->win_y+30.0f), 0xffffffffu, scaletext);
  }

  // now the controls:
  ImGui::InvisibleButton("display", ImVec2(0.975*w->win_w, 0.975*w->win_h));
  if(!events) return;
  dt_image_events(w, ImGui::IsItemHovered(), main);
}
