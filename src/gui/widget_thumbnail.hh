#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>

namespace ImGui {

// this is pretty much ImGui::ImageButton with a few custom hacks:
bool ThumbnailImage(
    ImTextureID user_texture_id,
    const ImVec2& size,
    const ImVec2& uv0,
    const ImVec2& uv1,
    int frame_padding,
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

    // make sure we're square:
    float padding = frame_padding >= 0 ? frame_padding : style.FramePadding[0];
    float wd = std::max(size[0], size[1]) + 2*padding;
    // overloaded operators are not exposed in imgui_internal, so we'll do the hard way:
    const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos[0] + wd, window->DC.CursorPos[1] + wd));
    ImVec2 off((wd - size[0])*.5f, (wd - size[1])*.5f);
    const ImRect image_bb(
        ImVec2(window->DC.CursorPos[0] + off[0],
               window->DC.CursorPos[1] + off[1]),
        ImVec2(window->DC.CursorPos[0] + off[0] + size[0],
               window->DC.CursorPos[1] + off[1] + size[1]));
    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    // TODO: double click?
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, ImClamp(padding, 0.0f, style.FrameRounding));
    if (bg_col.w > 0.0f)
        window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));
    window->DrawList->AddImage(user_texture_id, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

    return pressed;
}

} // namespace ImGui
