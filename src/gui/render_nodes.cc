extern "C"
{
#include "view.h"
#include "gui.h"
#include "pipe/modules/api.h"
#include "nodes.h"
}
#include "render_view.hh"
#include "render.h"
#include "imnodes.h"
#include <stdint.h>

void render_nodes()
{
  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_NoTitleBar;
  window_flags |= ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoResize;
  window_flags |= ImGuiWindowFlags_NoBackground;
  ImGui::SetNextWindowPos (ImVec2(vkdt.state.center_x,  vkdt.state.center_y),  ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.center_wd, vkdt.state.center_ht), ImGuiCond_Always);
  ImGui::Begin("nodes center", 0, window_flags);

  ImNodes::BeginNodeEditor();

  dt_graph_t *g = &vkdt.graph_dev;
  for(uint32_t m=0;m<g->num_modules;m++)
  {
    ImNodes::BeginNode(m);
    dt_module_t *mod = g->module+m;

    ImNodes::BeginNodeTitleBar();
    ImGui::Text("%" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
    ImNodes::EndNodeTitleBar();

    for(int c=0;c<mod->num_connectors;c++)
    {
      const int cid = (m<<5) + c;
      if(dt_connector_output(mod->connector+c))
      {
        ImNodes::BeginOutputAttribute(cid);
        ImGui::Text("%" PRItkn, dt_token_str(mod->connector[c].name));
        ImNodes::EndOutputAttribute();
      }
      else
      {
        ImNodes::BeginInputAttribute(cid);
        ImGui::Text("%" PRItkn, dt_token_str(mod->connector[c].name));
        ImNodes::EndInputAttribute();
      }
    }
    ImNodes::EndNode();
  }

  int lid = 0;
  for(uint32_t m=0;m<g->num_modules;m++)
  {
    dt_module_t *mod = g->module+m;
    for(int c=0;c<mod->num_connectors;c++)
    {
      if(dt_connector_input(mod->connector+c) && dt_connected(mod->connector+c))
      {
        int id0 = (m<<5) + c;
        int id1 = (mod->connector[c].connected_mi<<5) + mod->connector[c].connected_mc;
        ImNodes::Link(lid++, id0, id1);
      }
    }
  }

  ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopRight);
  ImNodes::EndNodeEditor();

  // TODO: if something got connected/disconnected
  // TODO: we'll get attribute ids and even node ids
  // TODO: but if a link is deleted we'll get the link id, so we need to encode it!

  ImGui::End(); // nodes center
  if(ImGui::IsKeyPressed(ImGuiKey_Escape) ||
     ImGui::IsKeyPressed(ImGuiKey_CapsLock))
    dt_view_switch(s_view_darkroom);

  // TODO: right panel with:
  // displays: main, hist, view0, ..
  // add module, insert block
}

// void render_nodes_init() {}
// void render_nodes_cleanup() {}

extern "C" int nodes_enter()
{
  // TODO: LoadCurrentEditorStateFromIniFile
  // TODO: if that fails, perform automatic positioning of the nodes
  // TODO: by calling the same code as darkroom mode but horizontally
  return 0;
}

extern "C" int nodes_leave()
{
  // TODO:
  // ImNodes::SaveCurrentEditorStateToIniFile("/tmp/dreggn.ini");
  // this only saves positions of modules by id. since that might change
  // when adding/removing modules, i'll suggest rewriting it with mod name/inst.
  // TODO:
  // decide whether to write to ~/.config/nodes/<hash>.dat or a visible sidecar.
  // may need complete cfg file name in .dat for collision handling
  return 0;
}
