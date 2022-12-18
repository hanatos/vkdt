extern "C"
{
#include "view.h"
#include "gui.h"
#include "pipe/modules/api.h"
#include "pipe/io.h"
#include "nodes.h"
#include "db/hash.h"
#include "core/fs.h"
}
#include "render_view.hh"
#include "render.h"
#include "imnodes.h"
#include <stdint.h>

namespace {
typedef struct gui_nodes_t
{
  int do_layout;       // got nothing, do initial auto layout
  ImVec2 mod_pos[100]; // read manual positions from file
}
gui_nodes_t;
gui_nodes_t nodes;
}; // anonymous namespace

void render_nodes_module(dt_graph_t *g, int m)
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

void render_nodes_right_panel()
{
  ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
  ImGui::Begin("nodes right panel", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

  dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
  if(out_hist && vkdt.graph_res == VK_SUCCESS)
  {
    int wd = vkdt.state.panel_wd * 0.975;
    int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
    ImGui::Image(out_hist->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES],
        ImVec2(wd, ht),
        ImVec2(0,0), ImVec2(1,1),
        ImVec4(1.0f,1.0f,1.0f,1.0f), ImVec4(1.0f,1.0f,1.0f,0.5f));
  }

  dt_token_t dsp[] = { dt_token("main"), dt_token("view0"), dt_token("view1") };
  for(uint32_t d = 0; d < sizeof(dsp)/sizeof(dsp[0]); d++)
  {
    dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dsp[d]);
    if(out && vkdt.graph_res == VK_SUCCESS)
    {
      float pwd = 0.975 * vkdt.state.panel_wd;
      float iwd = out->connector[0].roi.wd;
      float iht = out->connector[0].roi.ht;
      float scale = MIN(pwd / iwd, 2.0f/3.0f*pwd / iht);
      int ht = scale * iht;
      int wd = scale * iwd;
      ImGui::NewLine(); // center
      ImGui::SameLine((vkdt.state.panel_wd - wd)/2);
      ImGui::Image(out->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES],
          ImVec2(wd, ht),
          ImVec2(0,0), ImVec2(1,1),
          ImVec4(1.0f,1.0f,1.0f,1.0f), ImVec4(1.0f,1.0f,1.0f,0.5f));
    }
  }
  // add module, insert block
  // selected node:
  int  sel_node_cnt = ImNodes::NumSelectedNodes();
  int *sel_node_id  = (int *)alloca(sizeof(int)*sel_node_cnt);
  ImNodes::GetSelectedNodes(sel_node_id);
  // TODO: buttons: disconnect, remove?, insert block before this,
  // TODO: move left, move right
  for(int i=0;i<sel_node_cnt;i++)
  {
    dt_module_t *mod = vkdt.graph_dev.module + sel_node_id[i];
    ImGui::Text("%" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
  }

  ImGui::End();
}

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

  uint32_t mod_id[100];       // module id, including disconnected modules
  assert(g->num_modules < sizeof(mod_id)/sizeof(mod_id[0]));
  for(uint32_t k=0;k<g->num_modules;k++) mod_id[k] = k;
  dt_module_t *const arr = g->module;
  const int arr_cnt = g->num_modules;
  int pos = 0, pos2 = 0; // find pos2 as the swapping position, where mod_id[pos2] = curr
  uint32_t modid[100], cnt = 0;
  for(int m=0;m<arr_cnt;m++)
    modid[m] = m; // init as identity mapping

  const float nodew = 200.0f;

#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(int m=cnt-1;m>=0;m--)
  {
    uint32_t curr = modid[m];
    pos2 = curr;
    while(mod_id[pos2] != curr) pos2 = mod_id[pos2];
    uint32_t tmp = mod_id[pos];
    mod_id[pos++] = mod_id[pos2];
    mod_id[pos2] = tmp;
    render_nodes_module(g, curr);
    if(nodes.do_layout == 1)
      ImNodes::SetNodeEditorSpacePos(curr, ImVec2(nodew*m, 300));
    else if(nodes.do_layout == 2)
      ImNodes::SetNodeEditorSpacePos(curr, nodes.mod_pos[curr]);
  }

  // now draw the disconnected modules
  for(int m=pos;m<arr_cnt;m++)
  {
    render_nodes_module(g, mod_id[m]);
    if(nodes.do_layout == 1)
      ImNodes::SetNodeEditorSpacePos(mod_id[m], ImVec2(nodew*(m-pos), 600));
    else if(nodes.do_layout == 2)
      ImNodes::SetNodeEditorSpacePos(mod_id[m], nodes.mod_pos[mod_id[m]]);
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

  if(nodes.do_layout) nodes.do_layout = 0;

  render_nodes_right_panel();
}

// void render_nodes_init() {}
// void render_nodes_cleanup() {}

extern "C" int nodes_enter()
{
  nodes.do_layout = 1; // assume bad initial auto layout
  dt_graph_t *g = &vkdt.graph_dev;
  char filename[PATH_MAX], datname[PATH_MAX];
  dt_db_image_path(&vkdt.db, vkdt.db.current_imgid, filename, sizeof(filename));
  uint64_t hash = hash64(filename);
  if(snprintf(datname, sizeof(datname), "%s/nodes/%lx.dat", dt_pipe.homedir, hash) < int(sizeof(datname)))
  { // write to ~/.config/vkdt/nodes/<hash>.dat
    FILE *f = fopen(datname, "rb");
    if(f)
    {
      char line[300];
      fscanf(f, "%299[^\n]", line);
      if(!strcmp(line, filename))
      { // only use hashed positions if filename actually matches
        while(!feof(f))
        {
          fscanf(f, "%299[^\n]", line);
          char *l = line;
          if(fgetc(f) == EOF) break; // read \n
          dt_token_t name = dt_read_token(l, &l);
          dt_token_t inst = dt_read_token(l, &l);
          const float px  = dt_read_float(l, &l);
          const float py  = dt_read_float(l, &l);
          for(uint32_t m=0;m<MIN(sizeof(nodes.mod_pos)/sizeof(nodes.mod_pos[0]), g->num_modules);m++)
            if(g->module[m].name == name && g->module[m].inst == inst)
              nodes.mod_pos[m] = ImVec2(px, py);
        }
        nodes.do_layout = 2; // ask to read positions
      }
      fclose(f);
    }
  }
  return 0;
}

extern "C" int nodes_leave()
{
  // serialise node positions to hidden/hashed file in ~/.config
  char filename[PATH_MAX], datname[PATH_MAX];
  dt_db_image_path(&vkdt.db, vkdt.db.current_imgid, filename, sizeof(filename));
  uint64_t hash = hash64(filename);
  if(snprintf(datname, sizeof(datname), "%s/nodes", dt_pipe.homedir) < int(sizeof(datname)))
    fs_mkdir(datname, 0755);
  if(snprintf(datname, sizeof(datname), "%s/nodes/%lx.dat", dt_pipe.homedir, hash) < int(sizeof(datname)))
  { // write to ~/.config/vkdt/nodes/<hash>.dat
    FILE *f = fopen(datname, "wb");
    if(f)
    {
      fprintf(f, "%s\n", filename);
      dt_graph_t *g = &vkdt.graph_dev;
      for(uint32_t m=0;m<g->num_modules;m++)
      {
        ImVec2 pos = ImNodes::GetNodeEditorSpacePos(m);
        fprintf(f, "%" PRItkn ":%" PRItkn ":%g:%g\n",
            dt_token_str(g->module[m].name), dt_token_str(g->module[m].inst),
            pos.x, pos.y);
      }
      fclose(f);
    }
  }
  return 0;
}
