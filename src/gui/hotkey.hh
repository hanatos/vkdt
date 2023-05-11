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

namespace ImHotKey
{
  struct HotKey
  {
    const char *functionName;
    const char *functionLib;
    uint16_t    key[4];
  };

  struct Key
  {
    const char   *lib;
    unsigned int  imkey;
    float         offset;
    float         width;
  };

  static const Key Keys[6][18] = {{
    {"esc", ImGuiKey_Escape, 18},
    {"f1",  ImGuiKey_F1, 18},
    {"f2",  ImGuiKey_F2},
    {"f3",  ImGuiKey_F3},
    {"f4",  ImGuiKey_F4},
    {"f5",  ImGuiKey_F5, 24},
    {"f6",  ImGuiKey_F6},
    {"f7",  ImGuiKey_F7},
    {"f8",  ImGuiKey_F8},
    {"f9",  ImGuiKey_F9, 24},
    {"f10", ImGuiKey_F10},
    {"f11", ImGuiKey_F11},
    {"f12", ImGuiKey_F12},
    {"prsn",ImGuiKey_PrintScreen, 24},
    {"sclk",ImGuiKey_ScrollLock},
    {"brk", ImGuiKey_Pause},
  },{
    {"~", }, // ???
    {"1", ImGuiKey_1},
    {"2", ImGuiKey_2},
    {"3", ImGuiKey_3},
    {"4", ImGuiKey_4},
    {"5", ImGuiKey_5},
    {"6", ImGuiKey_6},
    {"7", ImGuiKey_7},
    {"8", ImGuiKey_8},
    {"9", ImGuiKey_9},
    {"0", ImGuiKey_0},
    {"-", ImGuiKey_Minus},
    {"+", }, // ???
    {"backspace", ImGuiKey_Backspace, 0, 80},
    {"ins", ImGuiKey_Insert, 24},
    {"hom", ImGuiKey_Home},
    {"pgu", ImGuiKey_PageUp}
  },{
    {"tab", ImGuiKey_Tab, 0, 60},
    {"q", ImGuiKey_Q},
    {"w", ImGuiKey_W},
    {"e", ImGuiKey_E},
    {"r", ImGuiKey_R},
    {"t", ImGuiKey_T},
    {"y", ImGuiKey_Y},
    {"u", ImGuiKey_U},
    {"i", ImGuiKey_I},
    {"o", ImGuiKey_O},
    {"p", ImGuiKey_P},
    {"[", ImGuiKey_LeftBracket},
    {"]", ImGuiKey_RightBracket},
    {"|", 0, 0, 60}, // ???
    {"del", ImGuiKey_Delete, 24},
    {"end", ImGuiKey_End},
    {"pgd", ImGuiKey_PageDown}
  },{
    {"caps lock", ImGuiKey_CapsLock, 0, 80},
    {"a", ImGuiKey_A},
    {"s", ImGuiKey_S},
    {"d", ImGuiKey_D},
    {"f", ImGuiKey_F},
    {"g", ImGuiKey_G},
    {"h", ImGuiKey_H},
    {"j", ImGuiKey_J},
    {"k", ImGuiKey_K},
    {"l", ImGuiKey_L},
    {";", ImGuiKey_Semicolon},
    {"'", ImGuiKey_Apostrophe},
    {"ret", ImGuiKey_Enter, 0, 84}
  },{
    {"shift", ImGuiKey_LeftShift, 0, 104},
    {"z", ImGuiKey_Z},
    {"x", ImGuiKey_X},
    {"c", ImGuiKey_C},
    {"v", ImGuiKey_V},
    {"b", ImGuiKey_B},
    {"n", ImGuiKey_N},
    {"m", ImGuiKey_M},
    {",", ImGuiKey_Comma},
    {".", ImGuiKey_Period},
    {"/", ImGuiKey_Slash},
    {"shift", ImGuiKey_RightShift, 0, 104},
    {"up", ImGuiKey_UpArrow, 68}
  },{
    {"ctrl",  ImGuiKey_LeftCtrl,  0, 60},
    {"alt",   ImGuiKey_LeftAlt, 68, 60},
    {"space", ImGuiKey_Space,  0, 260},
    {"alt",   ImGuiKey_RightAlt,  0, 60},
    {"ctrl",  ImGuiKey_RightCtrl, 68, 60},
    {"left",  ImGuiKey_LeftArrow, 24},
    {"down",  ImGuiKey_DownArrow},
    {"right", ImGuiKey_RightArrow}
  }};

  static const Key& GetKeyForScanCode(unsigned int scancode)
  {
    if(!scancode) return Keys[0][0];
    for (unsigned int y = 0; y < 6; y++)
    {
      int x = 0;
      while (Keys[y][x].lib)
      {
        if (Keys[y][x].imkey == scancode)
          return Keys[y][x];
        x++;
      }
    }
    return Keys[0][0];
  }

  static void GetHotKeyLib(HotKey *hk, char *buffer, size_t bs)
  {
    buffer[0] = '#'; buffer[1] = '#'; buffer[2] = 0;
    if(hk->key[0] == 0)
    {
      snprintf(buffer, bs, "%s (unassigned)", hk->functionName);
      return;
    }

    int cnt = 0;
    for(int k=0;k<4&&hk->key[k];k++,cnt++);
    const char* k0 = GetKeyForScanCode(hk->key[0]).lib;
    const char* k1 = GetKeyForScanCode(hk->key[1]).lib;
    const char* k2 = GetKeyForScanCode(hk->key[2]).lib;
    const char* k3 = GetKeyForScanCode(hk->key[3]).lib;
    if(cnt == 4)
      snprintf(buffer, bs, "%s (%s + %s + %s + %s)", hk->functionName, k0, k1, k2, k3);
    else if(cnt == 3)
      snprintf(buffer, bs, "%s (%s + %s + %s)", hk->functionName, k0, k1, k2);
    else if(cnt == 2)
      snprintf(buffer, bs, "%s (%s + %s)", hk->functionName, k0, k1);
    else if(cnt == 1)
      snprintf(buffer, bs, "%s (%s)", hk->functionName, k0);
  }

  static void Edit(HotKey *hotkey, size_t hotkeyCount, const char *popupModal)
  {
    static int editingHotkey = -1;
    if (!hotkeyCount) return;
    static bool keyDown[512] = {};
#define IDX(IMK) (IMK >= ImGuiKey_NamedKey_BEGIN && IMK <= ImGuiKey_KeypadEqual ? IMK-ImGuiKey_NamedKey_BEGIN : 0)

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
        if(IDX(hotkey[editingHotkey].key[0]))
          keyDown[IDX(hotkey[editingHotkey].key[0])] = true;
        if(IDX(hotkey[editingHotkey].key[1]))
          keyDown[IDX(hotkey[editingHotkey].key[1])] = true;
        if(IDX(hotkey[editingHotkey].key[2]))
          keyDown[IDX(hotkey[editingHotkey].key[2])] = true;
        if(IDX(hotkey[editingHotkey].key[3]))
          keyDown[IDX(hotkey[editingHotkey].key[3])] = true;
      }
    }
    ImGui::EndChildFrame();
    ImGui::SameLine();
    ImGui::BeginGroup();

    for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_KeypadEqual; i++)
      if (IDX(i) && ImGui::IsKeyPressed(ImGuiKey(i), false))
        keyDown[IDX(i)] = !keyDown[IDX(i)];

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
        const int kid = IDX(key.imkey);
        bool& butSwtch = keyDown[kid];
        if(kid) ImGui::PushStyleColor(ImGuiCol_Button, butSwtch ?
            ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram] :
            ImGui::GetStyle().Colors[ImGuiCol_Button]);
        if (ImGui::Button(Keys[y][x].lib, ImVec2(width*s, 40*s)))
          if(kid)
            butSwtch = !butSwtch;
        if(kid) ImGui::PopStyleColor();
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
    if (keyDownCount && keyDownCount <= 4)
    {
      if (ImGui::Button("set", ImVec2(80*s, 40*s)))
      {
        int scanCodeCount = 0;
        hotkey[editingHotkey].key[0] = 0;
        hotkey[editingHotkey].key[1] = 0;
        hotkey[editingHotkey].key[2] = 0;
        hotkey[editingHotkey].key[3] = 0;
        for(uint32_t i = 1; i < sizeof(keyDown); i++)
        {
          if (keyDown[i])
          {
            if(scanCodeCount++)
            {
              uint16_t *k = hotkey[editingHotkey].key + scanCodeCount-1;
              k[0] = i + ImGuiKey_NamedKey_BEGIN;
              if(k[0] >= ImGuiKey_ModCtrl)
              { // swap control/mod to beginning
                uint16_t tmp = hotkey[editingHotkey].key[0];
                hotkey[editingHotkey].key[0] = k[0];
                k[0] = tmp;
              }
            }
            else hotkey[editingHotkey].key[0] = i + ImGuiKey_NamedKey_BEGIN;
          }
        }
      }
      ImGui::SameLine(0.f, 20.f*s);
    }
    else ImGui::SameLine(0.f, 100.f*s);

    if (ImGui::Button("done", ImVec2(80*s, 40*s))) { ImGui::CloseCurrentPopup(); }
    ImGui::EndGroup();
    ImGui::EndPopup();
#undef IDX
  }

  static int GetHotKey(HotKey *hotkey, size_t hotkeyCount)
  {
    if(dt_gui_imgui_want_text()) return -1;
    for(uint32_t i=0;i<hotkeyCount;i++)
    {
      int cnt = 0;
      for(int k=0;k<4&&hotkey[i].key[k];k++,cnt++);
      if((cnt == 4 && ImGui::IsKeyDown(ImGuiKey(hotkey[i].key[0])) && ImGui::IsKeyDown(ImGuiKey(hotkey[i].key[1]))
                   && ImGui::IsKeyDown(ImGuiKey(hotkey[i].key[2])) && ImGui::IsKeyPressed(ImGuiKey(hotkey[i].key[3]), false)) ||
         (cnt == 3 && ImGui::IsKeyDown(ImGuiKey(hotkey[i].key[0])) && ImGui::IsKeyDown(ImGuiKey(hotkey[i].key[1]))
                   && ImGui::IsKeyPressed(ImGuiKey(hotkey[i].key[2]), false)) ||
         (cnt == 2 && ImGui::IsKeyDown(ImGuiKey(hotkey[i].key[0])) && ImGui::IsKeyPressed(ImGuiKey(hotkey[i].key[1]), false)) ||
         (cnt == 1 && ImGui::IsKeyPressed(ImGuiKey(hotkey[i].key[0]), false)))
        return i;
    }
    return -1;
  }

  static void Serialise(const char *fn, HotKey *hk, int cnt)
  {
    char filename[PATH_MAX+100];
    snprintf(filename, sizeof(filename), "%s/%s.hotkeys", dt_pipe.homedir, fn);
    FILE *f = fopen(filename, "wb");
    if(f)
    {
      for(int i=0;i<cnt;i++)
        fprintf(f, "%s:%d %d %d %d\n", hk[i].functionName, hk[i].key[0], hk[i].key[1], hk[i].key[2], hk[i].key[3]);
      fclose(f);
    }
  }

  static void Deserialise(const char *fn, HotKey *hk, int cnt)
  {
    char filename[PATH_MAX+100];
    snprintf(filename, sizeof(filename), "%s/%s.hotkeys", dt_pipe.homedir, fn);
    FILE *f = fopen(filename, "rb");
    if(f)
    {
      while(!feof(f))
      {
        char name[100] = {0};
        int key[4] = {0};
        fscanf(f, "%[^:]:%d %d %d %d%*[^\n]", name, key, key+1, key+2, key+3);
        if(!name[0]) break;
        fgetc(f);
        for(int i=0;i<cnt;i++)
          if(!strcmp(hk[i].functionName, name))
            for(int k=0;k<4;k++) hk[i].key[k] = key[k];
      }
      fclose(f);
    }
  }
};
