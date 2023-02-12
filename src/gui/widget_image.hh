#pragma once
#include "widget_image.h"
// widget for window-size image with zoom/pan interaction

inline void
dt_image(
    dt_image_widget_t *w,
    dt_node_t         *out,     // make sure the out->dset is valid!
    int                events)  // if !=0 provide zoom/pan interaction
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
  if(ImGui::IsItemHovered())
  {
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
  } // end if mouse over image
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
