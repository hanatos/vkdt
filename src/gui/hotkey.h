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

#include "core/log.h"
#include "gui/gui.h"
#include "render.h"
#include "nk.h"

typedef struct hk_t
{
  const char *name;
  const char *lib;
  uint16_t    key[4];
  nk_bool     sel;
} hk_t;

typedef struct hk_key_t
{
  const char   *lib;
  unsigned int  key;
  float         offset;
  float         width;
} hk_key_t;

static const hk_key_t hk_keys[6][18] = {{
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

static inline const hk_key_t*
hk_get_key_for_scancode(unsigned int scancode)
{
  if(!scancode) return &hk_keys[0][0];
  for (unsigned int y = 0; y < 6; y++)
  {
    int x = 0;
    while (hk_keys[y][x].lib)
    {
      if (hk_keys[y][x].key == scancode)
        return &hk_keys[y][x];
      x++;
    }
  }
  return &hk_keys[0][0];
}

static inline void
hk_get_hotkey_lib(hk_t *hk, char *buffer, size_t bs)
{
  buffer[0] = '#'; buffer[1] = '#'; buffer[2] = 0;
  if(hk->key[0] == 0)
  {
    snprintf(buffer, bs, "%s (unassigned)", hk->name);
    return;
  }

  int cnt = 0;
  for(int k=0;k<4&&hk->key[k];k++,cnt++);
  const char* k0 = hk_get_key_for_scancode(hk->key[0])->lib;
  const char* k1 = hk_get_key_for_scancode(hk->key[1])->lib;
  const char* k2 = hk_get_key_for_scancode(hk->key[2])->lib;
  const char* k3 = hk_get_key_for_scancode(hk->key[3])->lib;
  if(cnt == 4)
    snprintf(buffer, bs, "%s (%s + %s + %s + %s)", hk->name, k0, k1, k2, k3);
  else if(cnt == 3)
    snprintf(buffer, bs, "%s (%s + %s + %s)", hk->name, k0, k1, k2);
  else if(cnt == 2)
    snprintf(buffer, bs, "%s (%s + %s)", hk->name, k0, k1);
  else if(cnt == 1)
    snprintf(buffer, bs, "%s (%s)", hk->name, k0);
}

// returns non-zero if popup was closed
static inline int
hk_edit(hk_t *hotkey, size_t num)
{
  static int editingHotkey = -1;
  if (!num) return 1;
  static int keyDown[512] = {};

  nk_layout_row_dynamic(&vkdt.ctx, .2*vkdt.state.center_wd, 2);
  if(nk_group_begin(&vkdt.ctx, "hotkey_list", 0))
  {
    nk_layout_row_static(&vkdt.ctx, 18, 150, 1);
    for(int i=0;i<num;i++)
    {
      char lib[128];
      hk_get_hotkey_lib(hotkey+i, lib, sizeof(lib));
      if(nk_selectable_label(&vkdt.ctx, lib, NK_TEXT_LEFT, &hotkey[i].sel))
      {
        editingHotkey = i;
        memset(keyDown, 0, sizeof(keyDown));
        if(hotkey[editingHotkey].key[0]) keyDown[hotkey[editingHotkey].key[0]] = 1;
        if(hotkey[editingHotkey].key[1]) keyDown[hotkey[editingHotkey].key[1]] = 1;
        if(hotkey[editingHotkey].key[2]) keyDown[hotkey[editingHotkey].key[2]] = 1;
        if(hotkey[editingHotkey].key[3]) keyDown[hotkey[editingHotkey].key[3]] = 1;
      }
      if(editingHotkey == i) hotkey[i].sel = 1;
      else hotkey[i].sel = 0;
    }
    nk_group_end(&vkdt.ctx);
  }

#if 0 // XXX FIXME: somehow have to conspire with the keyboard callback!
  for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_KeypadEqual; i++)
    if (IDX(i) && ImGui::IsKeyPressed(ImGuiKey(i), false))
      keyDown[IDX(i)] = !keyDown[IDX(i)];
#endif

#if 0 // TODO: port this
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
      const int kid = IDX(key.key);
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
  ImGui::Text("%s:", hotkey[editingHotkey].name);
  ImGui::SameLine();
  ImGui::TextWrapped("%s", hotkey[editingHotkey].lib);
  ImGui::EndChildFrame();
  ImGui::SameLine();
  int keyDownCount = 0;
  for (auto d : keyDown)
    keyDownCount += d ? 1 : 0;
  if (ImGui::Button("clear", ImVec2(80*s, 40*s)))
    memset(keyDown, 0, sizeof(keyDown));
  ImGui::SameLine();
  if (ImGui::Button("set", ImVec2(80*s, 40*s)))
  {
    int scanCodeCount = 0;
    hotkey[editingHotkey].key[0] = 0;
    hotkey[editingHotkey].key[1] = 0;
    hotkey[editingHotkey].key[2] = 0;
    hotkey[editingHotkey].key[3] = 0;
    if(keyDownCount && keyDownCount <= 4) // else clear
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

  if (ImGui::Button("done", ImVec2(80*s, 40*s))) { ImGui::CloseCurrentPopup(); }
  ImGui::EndGroup();
  ImGui::EndPopup();
#endif
  int ret = 0;
  if(nk_button_label(&vkdt.ctx, "done"))
    ret = 1;
  return ret;
}

static inline int
hk_get_hotkey(hk_t *hotkey, size_t num, int key)
{
  if(dt_gui_input_blocked()) return -1;
  int max_cnt = 0;
  int res = -1;
  for(uint32_t i=0;i<num;i++)
  {
    int cnt = 0;
    for(int k=0;k<4&&hotkey[i].key[k];k++,cnt++);
    if(key != hotkey[i].key[cnt-1]) goto next;
    for(int k=0;k<cnt-1;k++) if(glfwGetKey(qvk.window, hotkey[i].key[k]) != GLFW_PRESS) goto next;
    if(cnt > max_cnt)
    {
      max_cnt = cnt;
      res = i;
    }
next:;
  }
  return res;
}

static inline void
hk_serialise(const char *fn, hk_t *hk, int cnt)
{
  char filename[PATH_MAX+100];
  snprintf(filename, sizeof(filename), "%s/%s.hotkeys", dt_pipe.homedir, fn);
  FILE *f = fopen(filename, "wb");
  if(f)
  {
    fprintf(f, "v2\n");
    for(int i=0;i<cnt;i++)
      fprintf(f, "%s:%d %d %d %d\n", hk[i].name, hk[i].key[0], hk[i].key[1], hk[i].key[2], hk[i].key[3]);
    fclose(f);
  }
}

static inline void
hk_deserialise(const char *fn, hk_t *hk, int cnt)
{
  char filename[PATH_MAX+100];
  snprintf(filename, sizeof(filename), "%s/%s.hotkeys", dt_pipe.homedir, fn);
  FILE *f = fopen(filename, "rb");
  if(f)
  {
    char name[100] = {0};
    int key[4] = {0};
    fscanf(f, "%[^\n]", name);
    fgetc(f);
    if(strcmp(name, "v2"))
    {
      dt_log(s_log_err, "ignoring old hotkeys for %s, sorry for the inconvenience", fn);
    }
    else while(!feof(f))
    {
      fscanf(f, "%[^:]:%d %d %d %d%*[^\n]", name, key, key+1, key+2, key+3);
      if(!name[0]) break;
      fgetc(f);
      for(int i=0;i<cnt;i++)
        if(!strcmp(hk[i].name, name))
          for(int k=0;k<4;k++) hk[i].key[k] = key[k];
    }
    fclose(f);
  }
}
