#pragma once
#include "imgui.h"

// this is copied from much ImGui::SliderFloat with a few added features:

namespace ImGui {

  // Those MIN/MAX values are not define because we need to point to them
  static const ImS32          IM_S32_MIN = INT_MIN;    // (-2147483647 - 1), (0x80000000);
  static const ImS32          IM_S32_MAX = INT_MAX;    // (2147483647), (0x7FFFFFFF)

  template<typename TYPE, typename FLOATTYPE>
  float SliderCalcRatioFromValueT2(ImGuiDataType data_type, TYPE v, TYPE v_min, TYPE v_max)
  {
    if (v_min == v_max)
      return 0.0f;

    const TYPE v_clamped = (v_min < v_max) ? ImClamp(v, v_min, v_max) : ImClamp(v, v_max, v_min);
    // Linear slider
    return (float)((FLOATTYPE)(v_clamped - v_min) / (FLOATTYPE)(v_max - v_min));
  }

  // FIXME-LEGACY: Prior to 1.61 our DragInt() function internally used floats and because of this the compile-time default value for format was "%.0f".
  // Even though we changed the compile-time default, we expect users to have carried %f around, which would break the display of DragInt() calls.
  // To honor backward compatibility we are rewriting the format string, unless IMGUI_DISABLE_OBSOLETE_FUNCTIONS is enabled. What could possibly go wrong?!
  static const char* PatchFormatStringFloatToInt(const char* fmt)
  {
      if (fmt[0] == '%' && fmt[1] == '.' && fmt[2] == '0' && fmt[3] == 'f' && fmt[4] == 0) // Fast legacy path for "%.0f" which is expected to be the most common case.
          return "%d";
      const char* fmt_start = ImParseFormatFindStart(fmt);    // Find % (if any, and ignore %%)
      const char* fmt_end = ImParseFormatFindEnd(fmt_start);  // Find end of format specifier, which itself is an exercise of confidence/recklessness (because snprintf is dependent on libc or user).
      if (fmt_end > fmt_start && fmt_end[-1] == 'f')
      {
  #ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
          if (fmt_start == fmt && fmt_end[0] == 0)
              return "%d";
          ImGuiContext& g = *GImGui;
          ImFormatString(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), "%.*s%%d%s", (int)(fmt_start - fmt), fmt, fmt_end); // Honor leading and trailing decorations, but lose alignment/precision.
          return g.TempBuffer;
  #else
          IM_ASSERT(0 && "DragInt(): Invalid format string!"); // Old versions used a default parameter of "%.0f", please replace with e.g. "%d"
  #endif
      }
      return fmt;
  }

  template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
  bool SliderBehaviorT2(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, TYPE* v, const TYPE v_min, const TYPE v_max, const TYPE v_def, const char* format, ImRect* out_grab_bb)
  {
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiAxis axis = ImGuiAxis_X;
    const bool is_decimal = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
    const float grab_padding = 2.0f;
    const float slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;
    float grab_sz = style.GrabMinSize;
    SIGNEDTYPE v_range = (v_min < v_max ? v_max - v_min : v_min - v_max);
    if (!is_decimal && v_range >= 0)                                             // v_range < 0 may happen on integer overflows
      grab_sz = ImMax((float)(slider_sz / (v_range + 1)), style.GrabMinSize);  // For integer sliders: if possible have the grab size represent 1 unit
    grab_sz = ImMin(grab_sz, slider_sz);
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;
    // Process interacting with the slider
    bool value_changed = false;
    if (g.ActiveId == id || g.HoveredId == id)
    {
      bool set_new_value = false;
      bool set_def_value = false;
      float clicked_t = 0.0f;
      if (g.HoveredId == id && g.IO.MouseDoubleClicked[0])
      {
        set_def_value = true;
      }
      else if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse)
      {
        if (!g.IO.MouseDown[0])
        {
          ClearActiveID();
        }
        else
        {
          const float mouse_abs_pos = g.IO.MousePos[axis];
          clicked_t = (slider_usable_sz > 0.0f) ? ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_usable_sz, 0.0f, 1.0f) : 0.0f;
          if (axis == ImGuiAxis_Y)
            clicked_t = 1.0f - clicked_t;
          set_new_value = true;
        }
      }
      else if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Nav)
      {
        const ImVec2 delta2 = GetNavInputAmount2d(ImGuiNavDirSourceFlags_Keyboard | ImGuiNavDirSourceFlags_PadDPad, ImGuiInputReadMode_RepeatFast, 0.0f, 0.0f);
        float delta = (axis == ImGuiAxis_X) ? delta2.x : -delta2.y;
        if (g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
        {
          ClearActiveID();
        }
        else if (delta != 0.0f)
        {
          clicked_t = SliderCalcRatioFromValueT2<TYPE,FLOATTYPE>(data_type, *v, v_min, v_max);
          const int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 0;
          if ((decimal_precision > 0))
          {
            delta /= 100.0f;    // Gamepad/keyboard tweak speeds in % of slider bounds
            if (IsNavInputDown(ImGuiNavInput_TweakSlow))
              delta /= 10.0f;
          }
          else
          {
            if ((v_range >= -100.0f && v_range <= 100.0f) || IsNavInputDown(ImGuiNavInput_TweakSlow))
              delta = ((delta < 0.0f) ? -1.0f : +1.0f) / (float)v_range; // Gamepad/keyboard tweak speeds in integer steps
            else
              delta /= 100.0f;
          }
          if (IsNavInputDown(ImGuiNavInput_TweakFast))
            delta *= 10.0f;
          set_new_value = true;
          if ((clicked_t >= 1.0f && delta > 0.0f) || (clicked_t <= 0.0f && delta < 0.0f)) // This is to avoid applying the saturation when already past the limits
            set_new_value = false;
          else
            clicked_t = ImSaturate(clicked_t + delta);
        }
      }
      else if (g.HoveredId == id && g.IO.MouseWheel != 0.0f)
      {
        float delta = g.IO.MouseWheel;
        clicked_t = SliderCalcRatioFromValueT2<TYPE,FLOATTYPE>(data_type, *v, v_min, v_max);
        const int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 0;
        if ((decimal_precision > 0))
        {
          delta /= 100.0f;
          if (g.IO.KeyCtrl)
            delta /= 10.0f;
        }
        else
        {
          if ((v_range >= -100.0f && v_range <= 100.0f) || g.IO.KeyCtrl)
            delta = ((delta < 0.0f) ? -1.0f : +1.0f) / (float)v_range;
          else
            delta /= 100.0f;
        }
        if (g.IO.KeyShift)
          delta *= 10.0f;
        set_new_value = true;
        clicked_t += delta; // To allow to exceeds the limits
      }

      if (set_new_value)
      {
        TYPE v_new;
        // Linear slider
        if (is_decimal)
        {
          v_new = ImLerp(v_min, v_max, clicked_t);
        }
        else
        {
          // For integer values we want the clicking position to match the grab box so we round above
          // This code is carefully tuned to work with large values (e.g. high ranges of U64) while preserving this property..
          FLOATTYPE v_new_off_f = (v_max - v_min) * clicked_t;
          TYPE v_new_off_floor = (TYPE)(v_new_off_f);
          TYPE v_new_off_round = (TYPE)(v_new_off_f + (FLOATTYPE)0.5);
          if (v_new_off_floor < v_new_off_round)
            v_new = v_min + v_new_off_round;
          else
            v_new = v_min + v_new_off_floor;
        }
        // Round to user desired precision based on format string
        v_new = RoundScalarWithFormatT<TYPE,SIGNEDTYPE>(format, data_type, v_new);
        // Apply result
        if (*v != v_new)
        {
          *v = v_new;
          value_changed = true;
        }
      }
      else  if (set_def_value)
      {
        if (v_def != NAN)
        {
          *v = v_def;
          value_changed = true;
        }
      }
    }
    if (slider_sz < 1.0f)
    {
      *out_grab_bb = ImRect(bb.Min, bb.Min);
    }
    else
    {
      // Output grab position so it can be displayed by the caller
      float grab_t = SliderCalcRatioFromValueT2<TYPE, FLOATTYPE>(data_type, *v, v_min, v_max);
      if (axis == ImGuiAxis_Y)
        grab_t = 1.0f - grab_t;
      const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
      if (axis == ImGuiAxis_X)
        *out_grab_bb = ImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
      else
        *out_grab_bb = ImRect(bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
    }
    return value_changed;
  }

  // For 32-bit and larger types, slider bounds are limited to half the natural type range.
  // So e.g. an integer Slider between INT_MAX-10 and INT_MAX will fail, but an integer Slider between INT_MAX/2-10 and INT_MAX/2 will be ok.
  // It would be possible to lift that limitation with some work but it doesn't seem to be worth it for sliders.
  bool DtSliderBehavior(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, void* p_v, const void* p_min, const void* p_max, const void* p_def, const char* format, ImRect* out_grab_bb)
  {
    switch (data_type)
    {
    case ImGuiDataType_S32:
      IM_ASSERT(*(const ImS32*)p_min >= IM_S32_MIN/2 && *(const ImS32*)p_max <= IM_S32_MAX/2);
      return SliderBehaviorT2<ImS32, ImS32, float >(bb, id, data_type, (ImS32*)p_v, *(const ImS32*)p_min, *(const ImS32*)p_max, *(const ImS32*)p_def, format, out_grab_bb);
    case ImGuiDataType_Float:
      IM_ASSERT(*(const float*)p_min >= -FLT_MAX/2.0f && *(const float*)p_max <= FLT_MAX/2.0f);
      return SliderBehaviorT2<float, float, float >(bb, id, data_type, (float*)p_v, *(const float*)p_min, *(const float*)p_max, *(const float*)p_def, format, out_grab_bb);
    case ImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
    return false;
  }

  // Note: p_data, p_min and p_max are _pointers_ to a memory address holding the data. For a slider, they are all required.
  // Read code of e.g. SliderFloat(), SliderInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
  bool DtSliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const void* p_def, const char* format)
  {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
      return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos[0] + w, window->DC.CursorPos[1] + label_size.y + style.FramePadding.y*2.0f));
    const ImRect total_bb(frame_bb.Min, ImVec2(frame_bb.Max[0] + ((label_size.x > 0.0f) ? style.ItemInnerSpacing.x + label_size.x : 0.0f), frame_bb.Max[1]));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb))
      return false;

    // Default format string when passing NULL
    if (format == NULL)
      format = DataTypeGetInfo(data_type)->PrintFmt;
    else if (data_type == ImGuiDataType_S32 && strcmp(format, "%d") != 0) // (FIXME-LEGACY: Patch old "%.0f" format string to use "%d", read function more details.)
      format = PatchFormatStringFloatToInt(format);

    // Tabbing or CTRL-clicking on Slider turns it into an input box
    const bool hovered = ItemHoverable(frame_bb, id);
    bool temp_input_is_active = TempInputIsActive(id);
    bool temp_input_start = false;
    if (!temp_input_is_active)
    {
      const bool focus_requested = FocusableItemRegister(window, id);
      const bool clicked = (hovered && g.IO.MouseClicked[0]);
      if (focus_requested || clicked || g.NavActivateId == id || g.NavInputId == id)
      {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        if (focus_requested || (clicked && g.IO.KeyCtrl) || g.NavInputId == id)
        {
          temp_input_start = true;
          FocusableItemUnregister(window);
        }
      }
    }
    if (temp_input_is_active || temp_input_start)
      return TempInputScalar(frame_bb, id, label, data_type, p_data, format);

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImRect grab_bb;
    const bool value_changed = DtSliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, p_def, format, &grab_bb);
    if (value_changed)
      MarkItemEdited(id);

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
      window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f,0.5f));

    if (label_size.x > 0.0f)
      RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    return value_changed;
  }

  bool DtSliderFloat(const char* label, float* v, const float v_min, const float v_max, const float v_def, const char* format)
  {
    return DtSliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, &v_def, format);
  }

  bool DtSliderInt(const char* label, int* v, const int v_min, const int v_max, const int v_def, const char* format)
  {
    return DtSliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, &v_def, format);
  }


} // namespace ImGui
