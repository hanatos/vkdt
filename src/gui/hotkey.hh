// loosely based on (mainly the cool clickable keyboard layout):
// ImHotKey v1.0
// https://github.com/CedricGuillemet/ImHotKey
//
// The MIT License(MIT)
// 
// Copyright(c) 2019 Cedric Guillemet
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#pragma once

extern "C" {
#include "gui/gui.h"
}
#include "render.h"
#include "imgui.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ImHotKey
{
  struct HotKey
  {
    const char *functionName;
    const char *functionLib;
    uint16_t    key0;
    uint16_t    key1;
  };

  struct Key
  {
    const char   *lib;
    unsigned int  glfw_key;
    float         offset;
    float         width;
  };

  static const Key Keys[6][18] = {{
    {"esc", GLFW_KEY_ESCAPE, 18},
    {"f1",  GLFW_KEY_F1, 18},
    {"f2",  GLFW_KEY_F2},
    {"f3",  GLFW_KEY_F3},
    {"f4",  GLFW_KEY_F4},
    {"f5",  GLFW_KEY_F5, 24},
    {"f6",  GLFW_KEY_F6},
    {"f7",  GLFW_KEY_F7},
    {"f8",  GLFW_KEY_F8},
    {"f9",  GLFW_KEY_F9, 24},
    {"f10", GLFW_KEY_F10},
    {"f11", GLFW_KEY_F11},
    {"f12", GLFW_KEY_F12},
    {"prsn",GLFW_KEY_PRINT_SCREEN, 24},
    {"sclk",GLFW_KEY_SCROLL_LOCK},
    {"brk", GLFW_KEY_PAUSE},
  },{
    {"~", }, // ???
    {"1", GLFW_KEY_1},
    {"2", GLFW_KEY_2},
    {"3", GLFW_KEY_3},
    {"4", GLFW_KEY_4},
    {"5", GLFW_KEY_5},
    {"6", GLFW_KEY_6},
    {"7", GLFW_KEY_7},
    {"8", GLFW_KEY_8},
    {"9", GLFW_KEY_9},
    {"0", GLFW_KEY_0},
    {"-", GLFW_KEY_MINUS},
    {"+", }, // ???
    {"backspace", GLFW_KEY_BACKSPACE, 0, 80},
    {"ins", GLFW_KEY_INSERT, 24},
    {"hom", GLFW_KEY_HOME},
    {"pgu", GLFW_KEY_PAGE_UP}
  },{
    {"tab", GLFW_KEY_TAB, 0, 60},
    {"q", GLFW_KEY_Q},
    {"w", GLFW_KEY_W},
    {"e", GLFW_KEY_E},
    {"r", GLFW_KEY_R},
    {"t", GLFW_KEY_T},
    {"y", GLFW_KEY_Y},
    {"u", GLFW_KEY_U},
    {"i", GLFW_KEY_I},
    {"o", GLFW_KEY_O},
    {"p", GLFW_KEY_P},
    {"[", GLFW_KEY_LEFT_BRACKET},
    {"]", GLFW_KEY_RIGHT_BRACKET},
    {"|", 0, 0, 60}, // ???
    {"del", GLFW_KEY_DELETE, 24},
    {"end", GLFW_KEY_END},
    {"pgd", GLFW_KEY_PAGE_DOWN}
  },{
    {"caps lock", GLFW_KEY_CAPS_LOCK, 0, 80},
    {"a", GLFW_KEY_A},
    {"s", GLFW_KEY_S},
    {"d", GLFW_KEY_D},
    {"f", GLFW_KEY_F},
    {"g", GLFW_KEY_G},
    {"h", GLFW_KEY_H},
    {"j", GLFW_KEY_J},
    {"k", GLFW_KEY_K},
    {"l", GLFW_KEY_L},
    {";", GLFW_KEY_SEMICOLON},
    {"'", GLFW_KEY_APOSTROPHE},
    {"ret", GLFW_KEY_ENTER, 0, 84}
  },{
    {"shift", GLFW_KEY_LEFT_SHIFT, 0, 104},
    {"z", GLFW_KEY_Z},
    {"x", GLFW_KEY_X},
    {"c", GLFW_KEY_C},
    {"v", GLFW_KEY_V},
    {"b", GLFW_KEY_B},
    {"n", GLFW_KEY_N},
    {"m", GLFW_KEY_M},
    {",", GLFW_KEY_COMMA},
    {".", GLFW_KEY_PERIOD},
    {"/", GLFW_KEY_SLASH},
    {"shift", GLFW_KEY_RIGHT_SHIFT, 0, 104},
    {"up", GLFW_KEY_UP, 68}
  },{
    {"ctrl",  GLFW_KEY_LEFT_CONTROL,  0, 60},
    {"alt",   GLFW_KEY_LEFT_ALT, 68, 60},
    {"space", GLFW_KEY_SPACE,  0, 260},
    {"alt",   GLFW_KEY_RIGHT_ALT,  0, 60},
    {"ctrl",  GLFW_KEY_RIGHT_CONTROL, 68, 60},
    {"left",  GLFW_KEY_LEFT, 24},
    {"down",  GLFW_KEY_DOWN},
    {"right", GLFW_KEY_RIGHT}
  }};

  static const Key& GetKeyForScanCode(unsigned int scancode)
  {
    for (unsigned int y = 0; y < 6; y++)
    {
      int x = 0;
      while (Keys[y][x].lib)
      {
        if (Keys[y][x].glfw_key == scancode)
          return Keys[y][x];
        x++;
      }
    }
    return Keys[0][0];
  }

  static void GetHotKeyLib(HotKey *hk, char *buffer, size_t bs)
  {
    buffer[0] = 0;
    if(hk->key0 == 0 || hk->key0 >= 512) return;

    const char* k0 = GetKeyForScanCode(hk->key0).lib;
    const char* k1 = GetKeyForScanCode(hk->key1).lib;
    if(hk->key1 != 0 && hk->key1 < 512)
      snprintf(buffer, bs, "%s (%s + %s)", hk->functionName, k0, k1);
    else
      snprintf(buffer, bs, "%s (%s)", hk->functionName, k0);
  }

  static void Edit(HotKey *hotkey, size_t hotkeyCount, const char *popupModal)
  {
    static int editingHotkey = -1;
    if (!hotkeyCount) return;
    static bool keyDown[512] = {};

    // scale with width of lt center view
    float s = vkdt.state.center_wd / 1000.0;

    ImGui::SetNextWindowSize(ImVec2(1060*s, 400*s));
    if (!ImGui::BeginPopupModal(popupModal, NULL, ImGuiWindowFlags_NoResize))
      return;

    ImGui::BeginChildFrame(127*s, ImVec2(220*s, -1));
    for(size_t i = 0;i < hotkeyCount;i++)
    {
      char hotKeyLib[128];
      GetHotKeyLib(hotkey+i, hotKeyLib, sizeof(hotKeyLib));
      if (ImGui::Selectable(hotKeyLib, editingHotkey == int(i)) || editingHotkey == -1)
      {
        editingHotkey = int(i);
        memset(keyDown, 0, sizeof(keyDown));
        if(hotkey[editingHotkey].key0 && hotkey[editingHotkey].key0 < 512)
          keyDown[hotkey[editingHotkey].key0] = true;
        if(hotkey[editingHotkey].key1 && hotkey[editingHotkey].key1 < 512)
          keyDown[hotkey[editingHotkey].key1] = true;
      }
    }
    ImGui::EndChildFrame();
    ImGui::SameLine();
    ImGui::BeginGroup();

    for (int i = 0; i < 512; i++)
    {
      if (ImGui::IsKeyPressed(i, false))
      {
        int imKey = i;
        keyDown[imKey] = !keyDown[imKey];
      }
    }
    for (unsigned int y = 0; y < 6; y++)
    {
      int x = 0;
      ImGui::BeginGroup();
      while (Keys[y][x].lib)
      {
        const Key& key = Keys[y][x];
        const float ofs = key.offset + (x?4.f:0.f);
        const float width = key.width > 0 ? key.width : 40;
        if (x)
          ImGui::SameLine(0.f, ofs*s);
        else if (ofs >= 1.f)
          ImGui::Indent(ofs*s);
        bool& butSwtch = keyDown[key.glfw_key];
        ImGui::PushStyleColor(ImGuiCol_Button, butSwtch ? 0xFF1040FF : 0x80000000);
        if (ImGui::Button(Keys[y][x].lib, ImVec2(width*s, 40*s)))
          butSwtch = !butSwtch;
        ImGui::PopStyleColor();
        x++;
      }
      ImGui::EndGroup();
    }
    ImGui::InvisibleButton("space", ImVec2(10*s, 55*s));
    ImGui::BeginChildFrame(18, ImVec2(540*s, 40*s));
    ImGui::Text("%s:", hotkey[editingHotkey].functionName);
    ImGui::SameLine();
    ImGui::TextWrapped("%s", hotkey[editingHotkey].functionLib);
    ImGui::EndChildFrame();
    ImGui::SameLine();
    int keyDownCount = 0;
    for (auto d : keyDown)
      keyDownCount += d ? 1 : 0;
    if (ImGui::Button("clear", ImVec2(80*s, 40*s)))
      memset(keyDown, 0, sizeof(keyDown));
    ImGui::SameLine();
    if (keyDownCount && keyDownCount < 3)
    {
      if (ImGui::Button("set", ImVec2(80*s, 40*s)))
      {
        int scanCodeCount = 0;
        hotkey[editingHotkey].key0 = 0;
        hotkey[editingHotkey].key1 = 0;
        for(uint32_t i = 1; i < sizeof(keyDown); i++)
        {
          if (keyDown[i])
          {
            if(scanCodeCount++)
            {
              hotkey[editingHotkey].key1 = GetKeyForScanCode(i).glfw_key;
              if(hotkey[editingHotkey].key1 > 256)
              { // swap control/mod to beginning
                uint16_t tmp = hotkey[editingHotkey].key0;
                hotkey[editingHotkey].key0 = hotkey[editingHotkey].key1;
                hotkey[editingHotkey].key1 = tmp;
              }
            }
            else hotkey[editingHotkey].key0 = GetKeyForScanCode(i).glfw_key;
          }
        }
      }
      ImGui::SameLine(0.f, 20.f*s);
    }
    else ImGui::SameLine(0.f, 100.f*s);

    if (ImGui::Button("done", ImVec2(80*s, 40*s))) { ImGui::CloseCurrentPopup(); }
    ImGui::EndGroup();
    ImGui::EndPopup();
  }

  static int GetHotKey(HotKey *hotkey, size_t hotkeyCount)
  {
    if(dt_gui_imgui_want_text()) return -1;
    for(uint32_t i=0;i<hotkeyCount;i++)
      if((hotkey[i].key0 && glfwGetKey(qvk.window, hotkey[i].key0) == GLFW_PRESS) &&
        (!hotkey[i].key1 || glfwGetKey(qvk.window, hotkey[i].key1) == GLFW_PRESS))
        return i;
    return -1;
  }
};
