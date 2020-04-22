// TODO:
// in imconfig.h, define IMGUI_INCLUDE_IMGUI_USER_H
// and in imgui_user.h declare the following functions!

// TODO: make a thumbnail with bells and whistles and return value
// TODO: to accomodate all required user interaction!
// frame_padding < 0: uses FramePadding from style (default)
// frame_padding = 0: no framing
// frame_padding > 0: set framing size
// The color used are the button colors.
uint32_t ImGui::ImageThumbnail(
    ImTextureID user_texture_id,
    const ImVec2& size,
    const ImVec2& uv0,  // XXX probably not these two
    const ImVec2& uv1,
    int frame_padding,  // also that can be hidden somewhere i think
    const ImVec4& bg_col,
    const ImVec4& tint_col)
{
  ImGuiWindow* window = GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;

  // Default to using texture ID as ID. User can still push string/integer prefixes.
  // We could hash the size/uv to create a unique ID but that would prevent the user from animating UV.
  PushID((void*)(intptr_t)user_texture_id);
  const ImGuiID id = window->GetID("#image");
  PopID();

  const ImVec2 padding = (frame_padding >= 0) ? ImVec2((float)frame_padding, (float)frame_padding) : style.FramePadding;
  const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size + padding * 2);
  const ImRect image_bb(window->DC.CursorPos + padding, window->DC.CursorPos + padding + size);
  ItemSize(bb);
  if (!ItemAdd(bb, id))
    return false;

  bool hovered, held;
  bool pressed = ButtonBehavior(bb, id, &hovered, &held);

  // Render
  const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
  RenderNavHighlight(bb, id);
  RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, style.FrameRounding));
  if (bg_col.w > 0.0f)
    window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));
  window->DrawList->AddImage(user_texture_id, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

  // render decorations (colour labels/stars/etc?)
  float c[] = {0, 1,
    0.951057, 0.309017,
    0.587785, -0.809017,
    -0.587785, -0.809017,
    -0.951057, 0.309017};
  float x[20];
  for(int i=0;i<5;i++)
  {
    x[4*i+0] = 0.1*size * c[2*i+0]
    x[4*i+1] = 0.1*size * c[2*i+1]
    x[4*i+2] = 0.05*size * c[2*i+4+0]
    x[4*i+3] = 0.05*size * c[2*i+4+1]
  }
  ImGui::GetWindowDrawList()->AddPolyline(
      (ImVec2 *)x, 10, IM_COL32_WHITE, true, vkdt.state.center_ht/500.0f);

  return pressed;
}
