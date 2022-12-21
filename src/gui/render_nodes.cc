extern "C"
{
#include "view.h"
#include "gui.h"
#include "pipe/modules/api.h"
#include "pipe/io.h"
#include "pipe/graph-history.h"
#include "nodes.h"
#include "db/hash.h"
#include "core/fs.h"
}
#include "render_view.hh"
#include "render.h"
#include "imnodes.h"
#include "hotkey.hh"
#include "api.hh"
#include <stdint.h>

static ImHotKey::HotKey hk_nodes[] = {
  {"apply preset",    "choose preset to apply",                     {ImGuiKey_LeftCtrl, ImGuiKey_P}},
  {"add module",      "add a new module to the graph",              {ImGuiKey_LeftCtrl, ImGuiKey_M}},
};

namespace {
enum hotkey_names_t
{ // for sane access in code
  s_hotkey_apply_preset    = 0,
  s_hotkey_module_add      = 1,
};

typedef struct gui_nodes_t
{
  int do_layout;       // got nothing, do initial auto layout
  ImVec2 mod_pos[100]; // read manual positions from file
  int hotkey;
  int node_hovered_link;
}
gui_nodes_t;
gui_nodes_t nodes;
}; // anonymous namespace

void render_nodes_module(dt_graph_t *g, int m)
{
  dt_module_t *mod = g->module+m;
  if(mod->name == 0) return; // has been removed, only the zombie left
  if(mod->disabled)
  {
    ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(10,10,10,255));
    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(10,10,10,255));
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(10,10,10,255));
  }
  ImNodes::BeginNode(m);

  ImNodes::BeginNodeTitleBar();
  ImGui::Text("%" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
  if(mod->disabled) ImGui::TextUnformatted("disabled");
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
      ImNodes::PushAttributeFlag( // inputs have only one source so we can disconnect:
          ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
      ImNodes::BeginInputAttribute(cid);
      ImGui::Text("%" PRItkn, dt_token_str(mod->connector[c].name));
      ImNodes::EndInputAttribute();
      ImNodes::PopAttributeFlag();
    }
  }
  ImNodes::EndNode();
  if(mod->disabled) for(int k=0;k<3;k++) ImNodes::PopColorStyle();
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
  int  sel_node_cnt = ImNodes::NumSelectedNodes();
  int *sel_node_id  = (int *)alloca(sizeof(int)*sel_node_cnt);
  ImNodes::GetSelectedNodes(sel_node_id);
  for(int i=0;i<sel_node_cnt;i++)
  {
    dt_module_t *mod = vkdt.graph_dev.module + sel_node_id[i];
    if(mod->name == 0) continue; // skip deleted
    char name[100];
    snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
    if(ImGui::CollapsingHeader(name))
    {
      ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
      if(mod->so->has_inout_chain && !mod->disabled && ImGui::Button("temporarily disable", ImVec2(-1, 0)))
      {
        mod->disabled = 1;
        vkdt.graph_dev.runflags = s_graph_run_all;
      }
      else if(mod->so->has_inout_chain && mod->disabled && ImGui::Button("re-enable", ImVec2(-1, 0)))
      {
        mod->disabled = 0;
        vkdt.graph_dev.runflags = s_graph_run_all;
      }
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip(mod->disabled ? "re-enable this module" :
            "temporarily disable this module without disconnecting it from the graph.\n"
            "this is just a convenience A/B switch in the ui and will not affect your\n"
            "processing history, lighttable thumbnail, or export.");

      int disconnect = 0;
      if(ImGui::Button("disconnect module", ImVec2(-1, 0)))
        disconnect = 1;
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip("disconnect all connectors of this module, try to\n"
                          "establish links to the neighbours directly where possible");
      if(ImGui::Button("remove module", ImVec2(-1, 0)))
        disconnect = 2;
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip("remove this module from the graph completely, try to\n"
                          "establish links to the neighbours directly where possible");
      if(disconnect)
      {
        ImNodes::ClearNodeSelection();
        int id_dspy = dt_module_get(mod->graph, dt_token("display"), dt_token("dspy"));
        if(id_dspy >= 0) dt_module_connect(mod->graph, -1, -1, id_dspy, 0); // no history
        mod->graph->runflags = static_cast<dt_graph_run_t>(s_graph_run_all);

        if(mod->so->has_inout_chain)
        {
          int m_after[5], c_after[5], max_after = 5;
          int c_prev, m_prev = dt_module_get_module_before(mod->graph, mod, &c_prev);
          int cnt = dt_module_get_module_after(mod->graph, mod, m_after, c_after, max_after);
          if(m_prev != -1 && cnt > 0)
          {
            int m_our = mod - mod->graph->module;
            int c_our = dt_module_get_connector(mod, dt_token("input"));
            int cerr = dt_module_connect_with_history(mod->graph, -1, -1, m_our, c_our);
            for(int k=0;k<cnt;k++)
              if(!cerr)
                cerr = dt_module_connect_with_history(mod->graph, m_prev, c_prev, m_after[k], c_after[k]);
          }
        }

        if(disconnect == 2)
          dt_module_remove(mod->graph, mod - mod->graph->module);
      }

      if(ImGui::Button("insert block before this..", ImVec2(-1, 0)))
        ImGui::OpenPopup("insert block");
      ImGui::PopStyleVar();
      if(ImGui::BeginPopupModal("insert block", NULL, ImGuiWindowFlags_AlwaysAutoResize))
      {
        static char mod_inst[10] = "01";
        char filename[PATH_MAX];
        ImGui::InputText("instance", mod_inst, 8);
        static char filter[256] = "";
        int ok = filteredlist("%s/data/blocks", "%s/blocks", filter, filename, sizeof(filename), s_filteredlist_default);
        if(ok) ImGui::CloseCurrentPopup();
        if(ok == 1)
        {
          int err = 0;
          int c_prev, m_prev = dt_module_get_module_before(mod->graph, mod, &c_prev);
          if(m_prev != -1)
          {
            int c_our_in = dt_module_get_connector(mod, dt_token("input"));
            if(c_our_in != -1)
            {
              err |= dt_graph_read_block(mod->graph, filename,
                  dt_token(mod_inst),
                  mod->graph->module[m_prev].name,
                  mod->graph->module[m_prev].inst,
                  mod->graph->module[m_prev].connector[c_prev].name,
                  mod->name,
                  mod->inst,
                  mod->connector[c_our_in].name);
              if(!err) vkdt.graph_dev.runflags = s_graph_run_all;
            }
            else err = 3;
          }
          else err = 3;
          if(err == 3) dt_gui_notification("no clear input/output chain!");
          else if(err) dt_gui_notification("reading the block failed!");
        }
        ImGui::EndPopup();
      }
    }
  }

  if(ImGui::CollapsingHeader("settings"))
  {
    if(ImGui::Button("hotkeys"))
      ImGui::OpenPopup("edit hotkeys");
    ImHotKey::Edit(hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]), "edit hotkeys");
  }

  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
  if(ImGui::Button("add module..", ImVec2(-1, 0)))
    nodes.hotkey = s_hotkey_module_add;
  if(ImGui::Button("apply preset", ImVec2(-1, 0)))
    nodes.hotkey = s_hotkey_apply_preset;
  if(ImGui::Button("back to darkroom mode", ImVec2(-1, 0)))
    dt_view_switch(s_view_darkroom);
  ImGui::PopStyleVar();
  ImGui::End();
}

void render_nodes()
{
  nodes.hotkey = ImHotKey::GetHotKey(hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]));
  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_NoTitleBar;
  window_flags |= ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoResize;
  window_flags |= ImGuiWindowFlags_NoBackground;
  ImGui::SetNextWindowPos (ImVec2(vkdt.state.center_x,  vkdt.state.center_y),  ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.center_wd, vkdt.state.center_ht), ImGuiCond_Always);
  ImGui::Begin("nodes center", 0, window_flags);

  // make dpi independent:
  ImNodes::PushStyleVar(ImNodesStyleVar_LinkThickness, vkdt.state.center_wd*0.002);
  ImNodes::PushStyleVar(ImNodesStyleVar_PinCircleRadius, vkdt.state.center_wd*0.003);
  ImNodes::PushStyleVar(ImNodesStyleVar_NodePadding, ImVec2(
        vkdt.state.center_wd*0.004, vkdt.state.center_wd*0.004));
  // TODO: ImNodesStyleVar_PinHoverRadius
  // TODO: ImNodesStyleVar_LinkHoverDistance
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

  const float nodew = vkdt.state.center_wd * 0.09;
  const float nodey = vkdt.state.center_ht * 0.3;
  ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkCreationOnSnap);

#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(int m=cnt-1;m>=0;m--)
  { // draw the graph
    uint32_t curr = modid[m];
    pos2 = curr;
    while(mod_id[pos2] != curr) pos2 = mod_id[pos2];
    uint32_t tmp = mod_id[pos];
    mod_id[pos++] = mod_id[pos2];
    mod_id[pos2] = tmp;
    render_nodes_module(g, curr);
    if(nodes.do_layout == 1)
      ImNodes::SetNodeEditorSpacePos(curr, ImVec2(nodew*(m+0.25), nodey));
    else if(nodes.do_layout == 2)
      ImNodes::SetNodeEditorSpacePos(curr, nodes.mod_pos[curr]);
  }

  for(int m=pos;m<arr_cnt;m++)
  { // draw disconnected modules
    render_nodes_module(g, mod_id[m]);
    if(nodes.do_layout == 1)
      ImNodes::SetNodeEditorSpacePos(mod_id[m], ImVec2(nodew*(m+0.25-pos), 2*nodey));
    else if(nodes.do_layout == 2)
      ImNodes::SetNodeEditorSpacePos(mod_id[m], nodes.mod_pos[mod_id[m]]);
  }
  ImNodes::PopAttributeFlag();

  for(uint32_t m=0;m<g->num_modules;m++)
  {
    dt_module_t *mod = g->module+m;
    for(int c=0;c<mod->num_connectors;c++)
    {
      if(dt_connector_input(mod->connector+c) && dt_connected(mod->connector+c))
      {
        int id0 = (m<<5) + c;
        int id1 = (mod->connector[c].connected_mi<<5) + mod->connector[c].connected_mc;
        if(mod->connector[c].flags & s_conn_feedback)
          ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(210,10,210,255));
        if(id0 == nodes.node_hovered_link)
          ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(210,10,10,255));
        ImNodes::Link(id0, id0, id1); // id0 is the input connector which is unique
        if(id0 == nodes.node_hovered_link)
          ImNodes::PopColorStyle();
        if(mod->connector[c].flags & s_conn_feedback)
          ImNodes::PopColorStyle();
      }
    }
  }

  ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopRight);
  ImNodes::EndNodeEditor();
  ImNodes::PopStyleVar(3);

  int lid = 0, mid = -1;
  nodes.node_hovered_link = -1;
  if(ImNodes::NumSelectedNodes() == 1)
  {
    ImNodes::GetSelectedNodes(&mid);
    if(g->module[mid].so->has_inout_chain)
    {
      int mco = dt_module_get_connector(g->module+mid, dt_token("output"));
      int mci = dt_module_get_connector(g->module+mid, dt_token("input"));
      if(mco >= 0 && mci >= 0 &&
         !dt_connected(g->module[mid].connector+mco) &&
         !dt_connected(g->module[mid].connector+mci))
      {
        if(ImNodes::IsLinkNodeHovered(&lid))
          nodes.node_hovered_link = lid;
        if(ImNodes::IsLinkNodeDropped(&lid))
        {
          nodes.node_hovered_link = -1;
          int mi0 = lid>>5, mc0 = lid&0x1f; // lid encodes input connector
          int mi1 = g->module[mi0].connector[mc0].connected_mi;
          int mc1 = g->module[mi0].connector[mc0].connected_mc;
          dt_module_connect_with_history(g, mi1, mc1, mid, mci);
          dt_module_connect_with_history(g, mid, mco, mi0, mc0);
          g->runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
        }
      }
    }
  }

  int sid, eid;
  if(ImNodes::IsLinkCreated(&sid, &eid))
  { // handle new links
    int mi0 = sid>>5,   mi1 = eid>>5;
    int mc0 = sid&0x1f, mc1 = eid&0x1f;
    int err = dt_module_connect_with_history(g, mi0, mc0, mi1, mc1);
    if(err) dt_gui_notification(dt_connector_error_str(err));
    else vkdt.graph_dev.runflags = s_graph_run_all;
  }

  if(ImNodes::IsLinkDestroyed(&lid))
  { // handle deleted links
    int mi = lid>>5, mc = lid&0x1f;
    dt_module_connect_with_history(g, -1, -1, mi, mc);
    vkdt.graph_dev.runflags = s_graph_run_all;
  }

  ImGui::End(); // nodes center
  if(!dt_gui_imgui_input_blocked())
    if(ImGui::IsKeyPressed(ImGuiKey_Escape) ||
       ImGui::IsKeyPressed(ImGuiKey_CapsLock))
      dt_view_switch(s_view_darkroom);

  if(nodes.do_layout) nodes.do_layout = 0;

  render_nodes_right_panel();

  switch(nodes.hotkey)
  {
    case s_hotkey_apply_preset:
      dt_gui_dr_preset_apply();
      break;
    case s_hotkey_module_add:
      dt_gui_dr_module_add();
      break;
    default:;
  }
  dt_gui_dr_modals(); // draw modal windows for presets etc
}

void render_nodes_init()
{
  ImHotKey::Deserialise("nodes", hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]));
}

void render_nodes_cleanup()
{
  ImHotKey::Serialise("nodes", hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]));
}

extern "C" int nodes_enter()
{
  nodes.hotkey = -1;
  nodes.do_layout = 1; // assume bad initial auto layout
  nodes.node_hovered_link = -1;
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
  // make sure we process once:
  vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
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
        if(g->module[m].name == 0) continue; // don't write removed ones
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

extern "C" void nodes_process()
{
  vkdt.state.anim_playing = 0; // we don't animate in graph edit mode
  if(vkdt.graph_dev.runflags)
    vkdt.graph_res = dt_graph_run(&vkdt.graph_dev,
        vkdt.graph_dev.runflags | s_graph_run_wait_done);
}
