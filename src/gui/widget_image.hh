#pragma once

// widget for window-size image with zoom/pan interaction

struct dt_image_widget_t
{
  dt_node_t *out;
  float look_at_x, look_at_y; // look at this position in image space
  float old_look_x, old_look_y;
  float scale;                // scale/magnification factor of image (8.0f means 800%)
  float win_x, win_y, win_w, win_h;
  float m_x, m_y;
  float wd, ht;
};

// TODO: need cleanup/zero init?

// darkroom_reset_zoom() // delete this TODO
// TODO: delete darkroom-util.h
// api.h dt_gui_dr_zoom() TODO: move this here too

inline void
dt_image_zoom(
    dt_image_widget_t *w,
    float              x, // mouse coordinate in screen space
    float              y)
{ // zoom 1:1
  // where does the mouse look in the current image?
  if(w->m_x > 0 && w->scale > 0.0f)
  { // maybe not needed? update as if mouse cursor moved (for gamepad nav)
    int dx = x - w->m_x;
    int dy = y - w->m_y;
    w->look_at_x = w->old_look_x - dx / w->scale;
    w->look_at_y = w->old_look_y - dy / w->scale;
    w->look_at_x = CLAMP(w->look_at_x, 0.0f, w->wd);
    w->look_at_y = CLAMP(w->look_at_y, 0.0f, w->ht);
  }
  float scale = w->scale <= 0.0f ? MIN(w->win_w/w->wd, w->win_h/w->ht) : w->scale;
  float im_x = x - (w->win_x + w->win_w/2.0f);
  float im_y = y - (w->win_y + w->win_h/2.0f);
  if(w->scale < 1.0f)
  {
    w->scale = 1.0f;
    const float dscale = 1.0f/scale - 1.0f/w->scale;
    w->look_at_x += im_x * dscale;
    w->look_at_y += im_y * dscale;
  }
  else if(w->scale >= 8.0f || w->scale < 1.0f)
  {
    w->scale = -1.0f;
    w->look_at_x = w->wd/2.0f;
    w->look_at_y = w->ht/2.0f;
  }
  else if(w->scale >= 1.0f)
  {
    w->scale *= 2.0f;
    const float dscale = 1.0f/scale - 1.0f/w->scale;
    w->look_at_x += im_x * dscale;
    w->look_at_y += im_y * dscale;
  }
}

inline void
dt_image_reset_zoom(
    dt_image_widget_t *w)
{
  w->look_at_x = FLT_MAX;
  w->look_at_y = FLT_MAX;
  w->scale = -1;
}

inline void
dt_image_from_view(
    dt_image_widget_t *w,
    const float        v[2],
    float              img[2])
{
  float scale = MIN(w->win_w/w->wd, w->win_h/w->ht);
  if(w->scale > 0.0f) scale = w->scale;
  float cvx = w->win_x + w->win_w *.5f;
  float cvy = w->win_y + w->win_h *.5f;
  if(w->look_at_x == FLT_MAX) w->look_at_x = w->wd/2.0f;
  if(w->look_at_y == FLT_MAX) w->look_at_y = w->ht/2.0f;
  float ox = cvx - scale * (w->look_at_x - w->wd/2.0f);
  float oy = cvy - scale * (w->look_at_y - w->ht/2.0f);
  img[0] = (v[0] - ox) / (scale * w->wd) + 0.5f;
  img[1] = (v[1] - oy) / (scale * w->ht) + 0.5f;
}

// convert normalised image coordinates to pixel coord on screen
inline void
dt_image_to_view(
    dt_image_widget_t *w,
    const float        img[2], // image pixel coordinate in [0,1]^2
    float              v[2])   // window pixel coordinate
{
  float scale = MIN(w->win_w/w->wd, w->win_h/w->ht);
  if(w->scale > 0.0f) scale = w->scale;
  float cvx = w->win_x + w->win_w *.5f;
  float cvy = w->win_y + w->win_h *.5f;
  if(w->look_at_x == FLT_MAX) w->look_at_x = w->wd/2.0f;
  if(w->look_at_y == FLT_MAX) w->look_at_y = w->ht/2.0f;
  float ox = cvx - scale * (w->look_at_x - w->wd/2.0f);
  float oy = cvy - scale * (w->look_at_y - w->ht/2.0f);
  v[0] = ox + scale * (img[0]-0.5f) * w->wd;
  v[1] = oy + scale * (img[1]-0.5f) * w->ht;
}

// make sure the out->dset is valid!
inline void
dt_image(
    dt_image_widget_t *w,
    dt_node_t         *out)
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
    ImGui::GetWindowDrawList()->AddText(ImVec2(w->win_x+0.9f*w->win_w,0.5f*w->win_y), 0xffffffffu, scaletext);
  }

  // now the controls:
  // XXX FIXME: somehow all the mouse lookat are wrong
  // TODO: if not hovered reset the mouse drag (m_x = -1)
  // ImGui::SameLine((w->win_w-(v1[0]-v0[0]))/2.0f);
  ImGui::InvisibleButton("display", ImVec2(w->win_w, w->win_h));
      // ImVec2(v1[0]-v0[0], v1[1]-v0[1]));
  if(ImGui::IsItemHovered())
      // ImGui::IsMousePosValid(&mpos) &&
      // mpos.x >= v0[0] && mpos.y >= v0[1] &&
      // mpos.x <= v1[0] && mpos.y <= v1[1])
  {
      // fprintf(stderr, "XXX hovered, pentablet %d\n", vkdt.wstate.pentablet_enabled);
    ImVec2 mpos = ImGui::GetMousePos();
#if 0
    // fprintf(stderr, "XXX rel mouse %g %g\n", mpos.x-w->win_x, mpos.y-w->win_y);
        float v[] = {mpos.x, mpos.y};
        float im[2];
        dt_image_from_view(w, v, im);
    fprintf(stderr, "XXX im mouse %g %g\n", im[0], im[1]);
#endif
    // ImGui::IsMousePosValid(&mpos);
    // TODO: set cursor when dragging?
    // SetMouseCursor(ImGuiMouseCursor_Arrow);
    // SetMouseCursor(ImGuiMouseCursor_ResizeAll);//Hand?
    if(ImGui::IsKeyReleased(ImGuiKey_MouseLeft) ||
       ImGui::IsKeyReleased(ImGuiKey_MouseMiddle))
    {
      // fprintf(stderr, "XXX release click %g %g\n", mpos.x, mpos.y);
      w->m_x = w->m_y = -1;
    }
    if((!vkdt.wstate.pentablet_enabled && ImGui::IsKeyPressed(ImGuiKey_MouseLeft)) ||
       ( vkdt.wstate.pentablet_enabled && ImGui::IsKeyPressed(ImGuiKey_MouseMiddle)))
    { // click to pan
      // fprintf(stderr, "XXX left click %g %g\n", mpos.x, mpos.y);
      w->m_x = mpos.x;
      w->m_y = mpos.y;
      w->old_look_x = w->look_at_x;
      w->old_look_y = w->look_at_y;
      fprintf(stderr, "XXX left click lookat %g %g\n", w->look_at_x, w->look_at_y);
    }
    else if(!vkdt.wstate.pentablet_enabled && ImGui::IsKeyPressed(ImGuiKey_MouseMiddle))
    { // middle click to 1:1
      fprintf(stderr, "XXX middle click %g %g sc %g\n", mpos.x, mpos.y, w->scale);
      dt_image_zoom(w, mpos.x, mpos.y);
    }
    if(ImGui::IsKeyPressed(ImGuiKey_MouseWheelY))
    { // mouse wheel (yoff is the input)
      float yoff = ImGui::GetIO().MouseWheel;
      const float fit_scale = MIN(w->win_w/w->wd, w->win_h/w->ht);
      const float scale = w->scale <= 0.0f ? fit_scale : w->scale;
      const float im_x = mpos.x - (w->win_x + w->win_w/2.0f);
      const float im_y = mpos.y - (w->win_y + w->win_h/2.0f);
        // float v[] = {mpos.x, mpos.y};
        // float im[2];
        // dt_image_from_view(w, v, im);
      w->scale = scale * (yoff > 0.0f ? 1.1f : 0.9f);
      w->scale = CLAMP(w->scale, 0.1f, 8.0f);
      if(w->scale > fit_scale)
      { // try to place mouse after zoom on same spot:
        // look_at + im / scale is mouse in old zoom image space
        // that - im / new scale should be the new center
        const float dscale = 1.0f/scale - 1.0f/w->scale;
        w->look_at_x += im_x * dscale;
        w->look_at_y += im_y * dscale;
        // w->look_at_x = im[0];
        // w->look_at_y = im[1];
      w->look_at_x = CLAMP(w->look_at_x, 0.0f, w->wd);
      w->look_at_y = CLAMP(w->look_at_y, 0.0f, w->ht);
      }
      else
      {
        w->look_at_x = w->wd/2.0f;
        w->look_at_y = w->ht/2.0f;
      }
      fprintf(stderr, "XXX wheel look %g %g\n", w->look_at_x, w->look_at_y);
    }
    if(w->m_x > 0 && w->scale > 0.0f)
    { // mouse moved while dragging
      float dx = mpos.x - w->m_x;
      float dy = mpos.y - w->m_y;
      w->look_at_x = w->old_look_x - dx / w->scale;
      w->look_at_y = w->old_look_y - dy / w->scale;
      w->look_at_x = CLAMP(w->look_at_x, 0.0f, w->wd);
      w->look_at_y = CLAMP(w->look_at_y, 0.0f, w->ht);
    }
    // XXX TODO: gamepad input!
  } // end if mouse over image
  else
  { // reset stuff
    w->m_x = w->m_y = -1;
  }
}
