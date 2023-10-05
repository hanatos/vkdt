#pragma once
#include "imgui_internal.h"

namespace {

inline void
dt_draw_quad(float u, float v, float size, uint32_t col)
{
  float c[] = {
     0,  1,  1,  0,
     0, -1, -1,  0};
  float x[20];
  for(int i=0;i<4;i++)
  {
    x[2*i+0] = u + size * c[2*i+0];
    x[2*i+1] = v + size * c[2*i+1];
  }
  ImGui::GetWindowDrawList()->AddConvexPolyFilled((ImVec2 *)x, 4, col);
  ImGui::GetWindowDrawList()->AddPolyline( (ImVec2 *)x, 4, 0xff000000, true, .1*size);
}

inline int screen_to_frame(float sx, dt_graph_t *g, ImRect bb)
{
  return CLAMP((int)((sx - bb.Min[0])/(bb.Max[0]-bb.Min[0]) * g->frame_cnt + 0.5), 0, g->frame_cnt-1);
}

inline float frame_to_screen(int f, dt_graph_t *g, ImRect bb)
{
  return CLAMP(bb.Min[0] + (bb.Max[0]-bb.Min[0]) * f / (float)g->frame_cnt, bb.Min[0], bb.Max[0]);
}

inline float
dt_draw_param_line(
    dt_module_t *mod,
    int          p)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) return 0.0f;

  ImGui::PushID(p + 200*(mod-mod->graph->module));
  const ImGuiID id = window->GetID("#image");
  ImGui::PopID();

  int win_w = vkdt.state.center_wd;
  int ht = vkdt.wstate.fontsize; // line height = font_size + 2*FramePadding.y ?
  const ImVec2 cp = ImVec2((int)window->DC.CursorPos[0], (int)window->DC.CursorPos[1]);
  const ImRect bb(ImVec2(cp[0]+(int)(0.12*win_w), cp[1]), ImVec2(cp[0] + win_w, cp[1] + ht));

  static int drag_k = -1, drag_mod = -1;

  int have_keys = 0;
  for(int k=0;k<mod->keyframe_cnt;k++)
  {
    if(mod->keyframe[k].param == mod->so->param[p]->name)
    {
      if(!have_keys)
      {
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return 0.0f;
        if(ImGui::IsItemClicked(0))
        { // left click: move animation time
          // TODO: api functions! create/delete keyframe, set animation time
          vkdt.graph_dev.frame = vkdt.state.anim_frame = screen_to_frame(ImGui::GetMousePos().x, mod->graph, bb);
          vkdt.state.anim_no_keyframes = 0;  // (re-)enable keyframes
          dt_graph_apply_keyframes(&vkdt.graph_dev); // rerun once
          vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
        }
        ImGui::SameLine();
        char text[60];
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Button));
        int len = snprintf(text, sizeof(text), "%" PRItkn " %" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst), dt_token_str(mod->so->param[p]->name));
        window->DrawList->AddText(cp, ImGui::GetColorU32(ImGuiCol_Text), text, text+len);
        have_keys = 1;
      }
      float x = frame_to_screen(mod->keyframe[k].frame, mod->graph, bb);
      float y = (bb.Min[1]+bb.Max[1])/2.0;
      dt_draw_quad(x, y, ht/2.0, ImGui::GetColorU32(ImGuiCol_PlotHistogram));
      const ImRect bbk(ImVec2(x-ht/2.0, y-ht/2.0), ImVec2(x+ht/2.0, y+ht/2.0));
      ImGui::ItemSize(bbk);
      ImGui::PushID(k + 30*p + 1000*(mod-mod->graph->module));
      const ImGuiID idk = window->GetID("#image");
      ImGui::PopID();
      if (!ImGui::ItemAdd(bbk, idk)) continue;
      if(ImGui::IsItemHovered())
      { // TODO: make this at least respect length (mod->so->param[p]->{type, cnt}
        dt_gui_set_tooltip("%" PRItkn " %f\nright click to delete\nleft click and drag to move",
            dt_token_str(mod->so->param[p]->name), *((float*)mod->keyframe[k].data));
      }
      if(ImGui::IsItemClicked(0))
      { // set state: dragging keyframe k
        drag_k = k;
        drag_mod = mod-mod->graph->module;
      }
      if(ImGui::IsKeyReleased(ImGuiKey_MouseLeft) && drag_k == k && drag_mod == mod-mod->graph->module)
      { // drag finished
        mod->keyframe[k].frame = screen_to_frame(ImGui::GetMousePos().x, mod->graph, bb);
        drag_k = drag_mod = -1;
      }
      if(ImGui::IsItemClicked(1))
      { // right click: delete this keyframe by copying the last one over it and decreasing keyframe_cnt
        mod->keyframe[k--] = mod->keyframe[--mod->keyframe_cnt];
      }
      ImGui::SameLine();
    }
  }
  if(have_keys)
  {
    const float x[4] = {
      frame_to_screen(mod->graph->frame, mod->graph, bb), bb.Min[1],
      frame_to_screen(mod->graph->frame, mod->graph, bb), bb.Max[1] };
    ImGui::GetWindowDrawList()->AddPolyline((ImVec2 *)x, 2, 0xff000000, false, .1*ht);
    ImGui::NewLine();
    return ht;
  }
  return 0.0f;
}

inline float
dt_dopesheet_module(dt_graph_t *g, uint32_t modid)
{ // draw all parameters of a module
  dt_module_t *mod = g->module + modid;
  if(mod->keyframe_cnt == 0) return 0.0f; // no keyframes to draw
  float size = 0.0f;
  for(int p=0;p<mod->so->num_params;p++)
    size += dt_draw_param_line(mod, p);
  return size;
}

} // end anonymous namespace

inline void
dt_dopesheet()
{ // draw all modules connected on the graph in same order as right panel in darkroom mode
  dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  float size = 0.0;
  for(int m=cnt-1;m>=0;m--)
    size += dt_dopesheet_module(graph, modid[m]);
  // this is unfortunately not reliable since we skip drawing and everything
  // in case a line cannot be seen on screen.
  // vkdt.wstate.dopesheet_view = MIN(0.5*vkdt.state.center_ht, size);
  (void)size;
  vkdt.wstate.dopesheet_view = 0.2*vkdt.state.center_ht; // fixed size
}
