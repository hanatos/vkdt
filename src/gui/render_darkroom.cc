// the imgui render functions for the darkroom view
extern "C"
{
#include "gui/view.h"
#include "pipe/modules/api.h"
#include "pipe/graph-history.h"
#include "pipe/graph-defaults.h"
}
#include "gui/render_view.hh"
#include "gui/hotkey.hh"
#include "gui/api.hh"
#include "gui/widget_draw.hh"
#include "gui/widget_thumbnail.hh"
#include "gui/widget_image.hh"

namespace { // anonymous namespace

static ImHotKey::HotKey hk_darkroom[] = {
  {"create preset",   "create new preset from image",               {ImGuiKey_LeftCtrl, ImGuiKey_O}},
  {"apply preset",    "choose preset to apply",                     {ImGuiKey_LeftCtrl, ImGuiKey_P}},
  {"show history",    "toggle visibility of left panel",            {ImGuiKey_LeftCtrl, ImGuiKey_H}},
  {"redo",            "go up in history stack one item",            {ImGuiKey_LeftCtrl, ImGuiKey_LeftShift, ImGuiKey_Z}}, // test before undo
  {"undo",            "go down in history stack one item",          {ImGuiKey_LeftCtrl, ImGuiKey_Z}},
  {"add module",      "add a new module to the graph",              {ImGuiKey_LeftCtrl, ImGuiKey_M}},
  {"add block",       "add a prefab block of modules to the graph", {ImGuiKey_LeftCtrl, ImGuiKey_B}},
  {"assign tag",      "assign a tag to the current image",          {ImGuiKey_LeftCtrl, ImGuiKey_T}},
  {"insert keyframe", "insert a keyframe for the active widget",    {ImGuiKey_LeftCtrl, ImGuiKey_K}},
  {"node editor",     "show node editor for the current image",     {ImGuiKey_LeftCtrl, ImGuiKey_N}},
  {"next image",      "switch to next image in folder",             {ImGuiKey_Space}},
  {"prev image",      "switch to previous image in folder",         {ImGuiKey_Backspace}},
  {"fullscreen",      "show/hide side panels for fullscreen",       {ImGuiKey_Tab}},
  {"rate 0",          "assign zero stars",                          {ImGuiKey_0}},
  {"rate 1",          "assign one star",                            {ImGuiKey_1}},
  {"rate 2",          "assign two stars",                           {ImGuiKey_2}},
  {"rate 3",          "assign three stars",                         {ImGuiKey_3}},
  {"rate 4",          "assign four stars",                          {ImGuiKey_4}},
  {"rate 5",          "assign five stars",                          {ImGuiKey_5}},
  {"label red",       "toggle red label",                           {ImGuiKey_F1}},
  {"label green",     "toggle green label",                         {ImGuiKey_F2}},
  {"label blue",      "toggle blue label",                          {ImGuiKey_F3}},
  {"label yellow",    "toggle yellow label",                        {ImGuiKey_F4}},
  {"label purple",    "toggle purple label",                        {ImGuiKey_F5}},
};

enum hotkey_names_t
{ // for sane access in code
  s_hotkey_create_preset   = 0,
  s_hotkey_apply_preset    = 1,
  s_hotkey_show_history    = 2,
  s_hotkey_redo            = 3,
  s_hotkey_undo            = 4,
  s_hotkey_module_add      = 5,
  s_hotkey_block_add       = 6,
  s_hotkey_assign_tag      = 7,
  s_hotkey_insert_keyframe = 8,
  s_hotkey_nodes_enter     = 9,
  s_hotkey_next_image      = 10,
  s_hotkey_prev_image      = 11,
  s_hotkey_fullscreen      = 12,
  s_hotkey_rate_0          = 13,
  s_hotkey_rate_1          = 14,
  s_hotkey_rate_2          = 15,
  s_hotkey_rate_3          = 16,
  s_hotkey_rate_4          = 17,
  s_hotkey_rate_5          = 18,
  s_hotkey_label_1         = 19,
  s_hotkey_label_2         = 20,
  s_hotkey_label_3         = 21,
  s_hotkey_label_4         = 22,
  s_hotkey_label_5         = 23,
};

// used to communictate between the gui helper functions
static struct gui_state_data_t
{
  enum gui_state_t
  {
    s_gui_state_regular      = 0,
    s_gui_state_insert_block = 1,
    s_gui_state_insert_mod   = 2,
  } state;
  char       block_filename[2*PATH_MAX+10];
  dt_token_t block_token[20];
  int        hotkey = -1;
} gui = {gui_state_data_t::s_gui_state_regular};

// goes here because the keyframe code depends on the above defines/hotkeys
// could probably pass a function pointer instead.
#include "gui/render_darkroom.hh"

void draw_arrow(float p[8], int feedback)
{
  float mark = vkdt.state.panel_wd * 0.02f;
  // begin and end markers
  float x[60] = {
     0.0f,              0.2f*mark*0.5f,
    -0.375f*mark*0.5f,  0.3f*mark*0.5f,
    -0.75f *mark*0.5f,  0.1f*mark*0.5f,
    -0.75f *mark*0.5f, -0.1f*mark*0.5f,
    -0.375f*mark*0.5f, -0.3f*mark*0.5f,
     0.0f,             -0.2f*mark*0.5f,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    -0.0f*mark,  0.4f*mark, -1.0f*mark, 0.0f,
    -0.0f*mark, -0.4f*mark, -0.0f*mark, 0.0f,
  };
  // a few bezier points:
  for(int i=0;i<7;i++)
  {
    x[2*i+0] += p[0];
    x[2*i+1] += p[1];
  }
  for(int i=25;i<30;i++)
  {
    x[2*i+0] += p[6];
    x[2*i+1] += p[7];
  }
  for(int i=7;i<25;i++)
  {
    const float t = (i-6.0f)/19.0f, tc = 1.0f-t;
    const float t2 = t*t, tc2 = tc*tc;
    const float t3 = t2*t, tc3 = tc2*tc;
    x[2*i+0] = p[0] * tc3 + p[2] * 3.0f*tc2*t + p[4] * 3.0f*t2*tc + p[6] * t3;
    x[2*i+1] = p[1] * tc3 + p[3] * 3.0f*tc2*t + p[5] * 3.0f*t2*tc + p[7] * t3;
  }
  ImGui::GetWindowDrawList()->AddPolyline(
      (ImVec2 *)x, 30,
      feedback ?
      0xffff8833:
      IM_COL32_WHITE,
      false, vkdt.state.center_ht/500.0f);
}

void dt_gui_set_lod(int lod)
{
  // set graph output scale factor and
  // trigger complete pipeline rebuild
  if(lod > 1)
  {
    vkdt.graph_dev.output_wd = vkdt.state.center_wd / (lod-1);
    vkdt.graph_dev.output_ht = vkdt.state.center_ht / (lod-1);
  }
  else
  {
    vkdt.graph_dev.output_wd = 0;
    vkdt.graph_dev.output_ht = 0;
  }
  vkdt.graph_dev.runflags = s_graph_run_all;
  // reset view? would need to set zoom, too
  dt_image_reset_zoom(&vkdt.wstate.img_widget);
}

uint64_t render_module(dt_graph_t *graph, dt_module_t *module, int connected)
{
  if(module->name == 0) return 0; // has been removed, only the zombie left
  static int mod[2] = {-1,-1}, con[2] = {-1,-1};
  uint64_t err = 0;
  static int insert_modid_before = -1;
  char name[30];
  snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn,
      dt_token_str(module->name), dt_token_str(module->inst));
  float lineht = ImGui::GetTextLineHeight();
  float wd = 0.6f, bwd = 0.16f;
  // buttons are unimpressed by this, they take a size argument:
  // ImGui::PushItemWidth(0.15f * vkdt.state.panel_wd);
  ImVec2 csize(bwd*vkdt.state.panel_wd, 1.6*lineht); // size of the connector buttons
  ImVec2 fsize(0.3*vkdt.state.panel_wd, 1.6*lineht); // size of the function buttons
  ImVec2 hp = ImGui::GetCursorScreenPos();
  int m_our = module - graph->module;
  ImGui::PushID(m_our);
  if(module->so->has_inout_chain)
  {
    ImGui::PushFont(dt_gui_imgui_get_font(3));
    if(ImGui::Button(module->disabled ? "\ue612" : "\ue836", ImVec2(1.6*vkdt.wstate.fontsize, 0)))
    {
      module->disabled ^= 1;
      vkdt.graph_dev.runflags = s_graph_run_all;
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered())
      ImGui::SetTooltip(module->disabled ? "re-enable this module" :
          "temporarily disable this module without disconnecting it from the graph.\n"
          "this is just a convenience A/B switch in the ui and will not affect your\n"
          "processing history, lighttable thumbnail, or export.");
    ImGui::SameLine();
  }
  else
  {
    ImGui::PushFont(dt_gui_imgui_get_font(3));
    ImGui::Button("\ue15b", ImVec2(1.6*vkdt.wstate.fontsize, 0));
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) ImGui::SetTooltip(
        "this module cannot be disabled automatically because\n"
        "it does not implement a simple input -> output chain");
    ImGui::SameLine();
  }
  if(!ImGui::CollapsingHeader(name))
  {
    for(int k=0;k<module->num_connectors;k++)
    {
      vkdt.wstate.connector[module - graph->module][k][0] = hp.x + vkdt.state.panel_wd * (wd + bwd);
      vkdt.wstate.connector[module - graph->module][k][1] = hp.y + 0.5f*lineht;
      // this switches off connections of collapsed modules
      // vkdt.wstate.connector[module - graph->module][k][0] = -1;
      // vkdt.wstate.connector[module - graph->module][k][1] = -1;
    }
    ImGui::PopID();
    return 0ul;
  }
  snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn "_col",
      dt_token_str(module->name), dt_token_str(module->inst));
  ImGui::Columns(2, name, false);
  ImGui::SetColumnWidth(0,       wd  * vkdt.state.panel_wd);
  ImGui::SetColumnWidth(1, (1.0f-wd) * vkdt.state.panel_wd);
  int m_after[5], c_after[5], max_after = 5, cerr = 0;
  if(gui.state == gui_state_data_t::s_gui_state_insert_block)
  {
    if(connected && ImGui::Button("before this", fsize))
    {
      int c_prev, m_prev = dt_module_get_module_before(graph, module, &c_prev);
      if(m_prev != -1)
      {
        int c_our_in = dt_module_get_connector(module, dt_token("input"));
        if(c_our_in != -1)
        {
          gui.block_token[1] = graph->module[m_prev].name; // output
          gui.block_token[2] = graph->module[m_prev].inst;
          gui.block_token[3] = graph->module[m_prev].connector[c_prev].name;
          gui.block_token[4] = module->name; // input
          gui.block_token[5] = module->inst;
          gui.block_token[6] = module->connector[c_our_in].name;
          cerr = dt_graph_read_block(graph, gui.block_filename,
              gui.block_token[0],
              gui.block_token[1], gui.block_token[2], gui.block_token[3],
              gui.block_token[4], gui.block_token[5], gui.block_token[6]);
          gui.state = gui_state_data_t::s_gui_state_regular;
          err = -1ul;
          if(cerr) err = (1ul<<32) | cerr;
          else vkdt.graph_dev.runflags = s_graph_run_all;
        }
        else err = 2ul<<32; // no input/output chain
      }
      else err = 2ul<<32; // no input/output chain
      insert_modid_before = -1;
    }
  }
  else if(insert_modid_before >= 0 && insert_modid_before != m_our)
  {
    if(connected && ImGui::Button("before this", fsize))
    {
      int m_new = insert_modid_before;
      int c_prev, m_prev = dt_module_get_module_before(graph, module, &c_prev);
      if(m_prev != -1)
      {
        int c_our_in  = dt_module_get_connector(module, dt_token("input"));
        int c_new_out = dt_module_get_connector(graph->module+m_new, dt_token("output"));
        int c_new_in  = dt_module_get_connector(graph->module+m_new, dt_token("input"));
        if(c_our_in != -1 && c_new_out != -1 && c_new_in != -1)
        {
          cerr = dt_module_connect_with_history(graph, m_prev, c_prev, m_new, c_new_in);
          if(!cerr)
            cerr = dt_module_connect_with_history(graph, m_new, c_new_out, m_our, c_our_in);
          err = -1ul;
          if(cerr) err = (1ul<<32) | cerr;
          else vkdt.graph_dev.runflags = s_graph_run_all;
        }
        else err = 2ul<<32; // no input/output chain
      }
      else err = 2ul<<32; // no input/output chain
      insert_modid_before = -1;
    }
  }
  else if(connected)
  {
    if(ImGui::Button("move up", fsize))
    {
      int c_prev, m_prev = dt_module_get_module_before(graph, module, &c_prev);
      int cnt = dt_module_get_module_after(graph, module, m_after, c_after, max_after);
      if(m_prev != -1 && cnt == 1)
      {
        int c_sscc, m_sscc;
        int c2 = dt_module_get_module_after(graph, graph->module+m_after[0], &m_sscc, &c_sscc, 1);
        if(c2 == 1)
        {
          int c_our_out  = dt_module_get_connector(module, dt_token("output"));
          int c_our_in   = dt_module_get_connector(module, dt_token("input"));
          if(c_our_out != -1 && c_our_in != -1)
          {
            cerr = dt_module_connect_with_history(graph, m_prev, c_prev, m_after[0], c_after[0]);
            if(!cerr)
              cerr = dt_module_connect_with_history(graph, m_after[0], c_after[0], m_our, c_our_in);
            if(!cerr)
              cerr = dt_module_connect_with_history(graph, m_our, c_our_out, m_sscc, c_sscc);
            err = -1ul;
            if(cerr) err = (1ul<<32) | cerr;
            else vkdt.graph_dev.runflags = s_graph_run_all;
          }
          else err = 2ul<<32; // no input/output
        }
        else err = 3ul<<32; // no unique after
      }
      else if(m_prev == -1) err = 2ul<<32;
      else if(cnt != 1) err = 3ul<<32;
    }
    ImGui::SameLine();
    if(ImGui::Button("move down", fsize))
    {
      int c_prev, m_prev = dt_module_get_module_before(graph, module, &c_prev);
      int cnt = dt_module_get_module_after (graph, module, m_after, c_after, max_after);
      if(m_prev != -1 && cnt > 0)
      {
        int c_pprv, m_pprv = dt_module_get_module_before(graph, graph->module+m_prev, &c_pprv);
        if(m_pprv != -1)
        {
          int c_our_out = dt_module_get_connector(module, dt_token("output"));
          int c_our_in  = dt_module_get_connector(module, dt_token("input"));
          int c_prev_in = dt_module_get_connector(graph->module+m_prev, dt_token("input"));
          if(c_our_out != -1 && c_our_in != -1 && c_prev_in != -1)
          {
            cerr = dt_module_connect_with_history(graph, m_pprv, c_pprv, m_our, c_our_in);
            if(!cerr)
              cerr = dt_module_connect_with_history(graph, m_our, c_our_out, m_prev, c_prev_in);
            for(int k=0;k<cnt;k++)
              if(!cerr)
                cerr = dt_module_connect_with_history(graph, m_prev, c_prev, m_after[k], c_after[k]);
            err = -1ul;
            if(cerr) err = (1ul<<32) | cerr;
            else vkdt.graph_dev.runflags = s_graph_run_all;
          }
          else err = 2ul<<32; // no input/output chain
        }
        else err = 2ul<<32;
      }
      else err = 2ul<<32;
    }
    if(ImGui::Button("disconnect", fsize))
    {
      int id_dspy = dt_module_get(graph, dt_token("display"), dt_token("dspy"));
      if(id_dspy >= 0)
      {
        dt_module_connect(graph, -1, -1, id_dspy, 0); // disconnect dspy entry point
        vkdt.graph_dev.runflags = s_graph_run_all;
      }
      int c_prev, m_prev = dt_module_get_module_before(graph, module, &c_prev);
      int cnt = dt_module_get_module_after(graph, module, m_after, c_after, max_after);
      if(m_prev != -1 && cnt > 0)
      {
        int c_our = dt_module_get_connector(module, dt_token("input"));
        cerr = dt_module_connect_with_history(graph, -1, -1, m_our, c_our);
        for(int k=0;k<cnt;k++)
          if(!cerr)
            cerr = dt_module_connect_with_history(graph, m_prev, c_prev, m_after[k], c_after[k]);
        err = -1ul;
        if(cerr) err = (1ul<<32) | cerr;
        else vkdt.graph_dev.runflags = s_graph_run_all;
      }
      else err = 2ul<<32; // no unique input/output chain
    }
  }
  else if(!connected)
  {
    const int red = insert_modid_before == m_our;
    if(red)
    {
      ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    }
    if(ImGui::Button("insert before", fsize))
    {
      if(red) insert_modid_before = -1;
      else    insert_modid_before = m_our;
    }
    if(ImGui::IsItemHovered())
      ImGui::SetTooltip("click and then select a module before which to insert this one");
    if(red) ImGui::PopStyleColor(3);
  }
  ImGui::NextColumn();

  for(int j=0;j<2;j++) for(int k=0;k<module->num_connectors;k++)
  {
    if((j == 0 && dt_connector_output(module->connector+k)) ||
       (j == 1 && dt_connector_input (module->connector+k)))
    {
      const int selected = (mod[j] == m_our) && (con[j] == k);
      ImVec2 p = ImGui::GetCursorScreenPos();
      vkdt.wstate.connector[m_our][k][0] = hp.x + vkdt.state.panel_wd * (wd + bwd);
      vkdt.wstate.connector[m_our][k][1] = p.y + 0.8f*lineht;

      if(selected)
      {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      }
      snprintf(name, sizeof(name), "%" PRItkn, dt_token_str(module->connector[k].name));
      if(ImGui::Button(name, csize))
      {
        if(mod[j] == m_our && con[j] == k)
        { // deselect
          mod[j] = con[j] = -1;
        }
        else
        { // select
          mod[j] = m_our;
          con[j] = k;
          if(mod[1-j] >= 0 && con[1-j] >= 0)
          {
            // if already connected disconnect
            if(vkdt.graph_dev.module[mod[1]].connector[con[1]].connected_mc == con[0] &&
               vkdt.graph_dev.module[mod[1]].connector[con[1]].connected_mi == mod[0])
              cerr = dt_module_connect_with_history(graph, -1, -1, mod[1], con[1]);
            else
              cerr = dt_module_connect_with_history(graph, mod[0], con[0], mod[1], con[1]);
            if(cerr) err = (1ul<<32) | cerr;
            else vkdt.graph_dev.runflags = s_graph_run_all;
            con[0] = con[1] = mod[0] = mod[1] = -1;
          }
        }
      }
      if(ImGui::IsItemHovered())
      {
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(vkdt.state.panel_wd);
        ImGui::Text("click to connect, format: %" PRItkn ":%" PRItkn,
            dt_token_str(module->connector[k].chan),
            dt_token_str(module->connector[k].format));
        if(module->connector[k].tooltip)
          ImGui::TextUnformatted(module->connector[k].tooltip);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
      }
      if(selected)
        ImGui::PopStyleColor(3);
    }
  }
  ImGui::Columns(1);
  ImGui::PopID();
  return err;
}

void render_darkroom_favourite()
{ // streamlined "favourite" ui
  dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  int32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < (int32_t)(sizeof(modid)/sizeof(modid[0])));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(int i=0;i<vkdt.fav_cnt;i++)
  { // arg. can we please do that without n^2 every redraw?
    for(int32_t m=0;m<cnt;m++)
    {
      if(modid[m] == vkdt.fav_modid[i])
      {
        render_darkroom_widget(vkdt.fav_modid[i], vkdt.fav_parid[i]);
        break;
      }
    }
  }
}

void render_darkroom_full()
{
  static char open[100] = {0};
  static int32_t active_module = -1;
  dt_graph_t *graph = &vkdt.graph_dev;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint32_t modid[100], cnt = 0;
#define TRAVERSE_POST \
  assert(cnt < sizeof(modid)/sizeof(modid[0]));\
  modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
  for(int m=cnt-1;m>=0;m--)
    render_darkroom_widgets(&vkdt.graph_dev, modid[m], open, active_module);
}

void render_darkroom_pipeline()
{
  // full featured module + connection ui
  uint32_t mod_id[100];       // module id, including disconnected modules
  uint32_t mod_in[100] = {0}; // module indentation level
  dt_graph_t *graph = &vkdt.graph_dev;
  assert(graph->num_modules < sizeof(mod_id)/sizeof(mod_id[0]));
  for(uint32_t k=0;k<graph->num_modules;k++) mod_id[k] = k;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules; // use this because buttons may add modules
  static uint64_t last_err = 0;
  uint64_t err = 0;
  int pos = 0, pos2 = 0; // find pos2 as the swapping position, where mod_id[pos2] = curr
  uint32_t modid[100], cnt = 0;
  for(int m=0;m<arr_cnt;m++)
    modid[m] = m; // init as identity mapping

  if(last_err)
  {
    uint32_t e = last_err >> 32;
    if(e == 1)
      ImGui::Text("connection failed: %s", dt_connector_error_str(last_err & 0xffffffffu));
    else if(e == 2)
      ImGui::Text("no input/output chain");
    else if(e == 3)
      ImGui::Text("no unique module after");
    else if(e == 16)
      ImGui::Text("module could not be added");
    else
      ImGui::Text("unknown error %lu %lu", last_err >> 32, last_err & -1u);
  }
  else ImGui::Text("no error");

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
    err = render_module(graph, arr+curr, 1);
    if(err == -1ul) last_err = 0;
    else if(err) last_err = err;
  }

  if(arr_cnt > pos) ImGui::Text("disconnected:");
  // now draw the disconnected modules
  for(int m=pos;m<arr_cnt;m++)
  {
    err = render_module(graph, arr+mod_id[m], 0);
    if(err == -1ul) last_err = 0;
    else if(err) last_err = err;
  }

  // draw connectors outside of clipping region of individual widgets, on top.
  // also go through list in reverse order such that the first connector will
  // pick up the largest indentation to avoid most crossovers
  for(int mi=arr_cnt-1;mi>=0;mi--)
  {
    int m = mod_id[mi];
    if(graph->module[m].name == 0) continue;
    for(int k=graph->module[m].num_connectors-1;k>=0;k--)
    {
      if(dt_connector_input(graph->module[m].connector+k))
      {
        const float *p = vkdt.wstate.connector[m][k];
        uint32_t nid = graph->module[m].connector[k].connected_mi;
        uint32_t cid = graph->module[m].connector[k].connected_mc;
        if(nid == -1u) continue; // disconnected
        const float *q = vkdt.wstate.connector[nid][cid];
        float b = vkdt.state.panel_wd * 0.03;
        int rev;// = nid; // TODO: store reverse list?
        // this works mostly but seems to have edge cases where it doesn't:
        // if(nid < pos) while(mod_id[rev] != nid) rev = mod_id[rev];
        // else for(rev=pos;rev<arr_cnt;rev++) if(mod_id[rev] == nid) break;
        for(rev=0;rev<arr_cnt;rev++) if(mod_id[rev] == nid) break;
        if(rev == arr_cnt+1 || mod_id[rev] != nid) continue;
        // traverse mod_id list between mi and rev nid and get indentation level
        uint32_t ident = 0;
        if(mi < rev) for(int i=mi+1;i<rev;i++)
        {
          mod_in[i] ++;
          ident = MAX(mod_in[i], ident);
        }
        else for(int i=rev+1;i<mi;i++)
        {
          mod_in[i] ++;
          ident = MAX(mod_in[i], ident);
        }
        float b2 = b * (ident + 2);
        if(p[0] == -1 || q[0] == -1) continue;
        float x[8] = {
          q[0]+b , q[1], q[0]+b2, q[1],
          p[0]+b2, p[1], p[0]+b , p[1],
        };
        draw_arrow(x, graph->module[m].connector[k].flags & s_conn_feedback);
      }
    }
  }

  // add new module to the graph (unconnected)
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
  if(ImGui::Button("add module..", ImVec2(-1, 0)))
    gui.hotkey = s_hotkey_module_add;

  // add block (read cfg snipped)
  if(gui.state == gui_state_data_t::s_gui_state_insert_block)
  {
    if(ImGui::Button("insert disconnected"))
    {
      dt_graph_read_block(&vkdt.graph_dev, gui.block_filename,
          gui.block_token[0],
          dt_token(""), dt_token(""), dt_token(""),
          dt_token(""), dt_token(""), dt_token(""));
      gui.state = gui_state_data_t::s_gui_state_regular;
    }
    ImGui::SameLine();
    if(ImGui::Button("abort"))
      gui.state = gui_state_data_t::s_gui_state_regular;
  }
  else if(ImGui::Button("insert block..", ImVec2(-1, 0)))
    gui.hotkey = s_hotkey_block_add;

  if(ImGui::Button("open node editor", ImVec2(-1, 0)))
    gui.hotkey = s_hotkey_nodes_enter;
  ImGui::PopStyleVar();
}

} // end anonymous namespace


void render_darkroom()
{
  gui.hotkey = ImHotKey::GetHotKey(hk_darkroom, sizeof(hk_darkroom)/sizeof(hk_darkroom[0]));
  switch(gui.hotkey)
  { // these are "destructive" hotkeys, they change the image and invalidate the dset.
    // this has to happen this frame *before* the dset is sent to imgui for display.
    case s_hotkey_next_image:
      dt_gui_dr_next();
      break;
    case s_hotkey_prev_image:
      dt_gui_dr_prev();
      break;
  }
  int axes_cnt = 0;
  const float *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt)    : 0;
  { // center image view
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    { // draw image properties
      ImGui::SetNextWindowPos (ImGui::GetMainViewport()->Pos, ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(win_w+2*win_x, win_y), ImGuiCond_Always);
      ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
      ImGui::Begin("darkroom statusbar", 0, ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoResize |
          ImGuiWindowFlags_NoMouseInputs |
          ImGuiWindowFlags_NoBackground);
      float wd = 0.5*vkdt.style.border_frac * qvk.win_width;
      const uint32_t ci = dt_db_current_imgid(&vkdt.db);
      if(ci != -1u)
      { // this should *always* be the case
        const uint16_t labels = vkdt.db.image[ci].labels;
        const uint16_t rating = vkdt.db.image[ci].rating;
        dt_draw_rating(win_x-wd,   win_y-wd, wd, rating);
        dt_draw_labels(win_x+4*wd, win_y-wd, wd, labels);
      }
      ImGui::End();
    }
    int border = 0;
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + win_x - border,
          ImGui::GetMainViewport()->Pos.y + win_y - border), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+2*border, win_h+2*border), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("darkroom center", 0, ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBackground);

    ImGuiIO& io = ImGui::GetIO();
    if(vkdt.wstate.active_widget_modid < 0 && // active widget grabs controls
      !dt_gui_imgui_input_blocked() &&
      !vkdt.wstate.grabbed)
    {
      static int fs_state = 0;
      if(!ImGui::IsKeyDown(ImGuiKey_GamepadL2) &&
         !ImGui::IsKeyDown(ImGuiKey_GamepadR2)) fs_state = 0;
      else if(fs_state == 0 && ImGui::IsKeyPressed(ImGuiKey_GamepadL2)) fs_state = 1;
      else if(fs_state == 1 && ImGui::IsKeyDown(ImGuiKey_GamepadL2) && ImGui::IsKeyPressed(ImGuiKey_GamepadR2))
      {
        fs_state = 2;
        dt_gui_dr_toggle_fullscreen_view();
      }

      if(gui.hotkey == s_hotkey_nodes_enter)
      {
        dt_view_switch(s_view_nodes);
        goto abort;
      }
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)||
         ImGui::IsKeyPressed(ImGuiKey_Escape)||
         ImGui::IsKeyPressed(ImGuiKey_CapsLock)||
         (ImGui::IsWindowHovered() && ImGui::IsMouseDoubleClicked(0)))
      {
        dt_view_switch(s_view_lighttable);
        vkdt.wstate.set_nav_focus = 2; // introduce some delay because imgui nav has it too
        goto abort;
      }
      // disable keyboard nav ctrl + shift to change images:
      else if(!ImGui::IsKeyDown(ImGuiKey_GamepadFaceLeft) && !io.KeyCtrl &&
               ImGui::IsKeyPressed(ImGuiKey_GamepadL1))
        dt_gui_dr_prev();
      else if(!ImGui::IsKeyDown(ImGuiKey_GamepadFaceLeft) && !io.KeyShift &&
               ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
        dt_gui_dr_next();
      else if(0)
      {
abort:
        ImGui::End();
        ImGui::PopStyleColor();
        return;
      }
    }

    // draw center view image:
    dt_node_t *out_main = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(out_main)
    {
      if(ImGui::IsKeyPressed(ImGuiKey_GamepadL3)) // left stick pressed
        dt_image_reset_zoom(&vkdt.wstate.img_widget);
      if(axes)
      {
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
        float wd  = (float)out_main->connector[0].roi.wd;
        float ht  = (float)out_main->connector[0].roi.ht;
        float imwd = win_w, imht = win_h;
        float scale = MIN(imwd/wd, imht/ht);
        if(vkdt.wstate.img_widget.scale > 0.0f) scale = vkdt.wstate.img_widget.scale;
        if(axes[2] > -1.0f) scale *= powf(2.0, -0.04*SMOOTH(axes[2]+1.0f)); 
        if(axes[5] > -1.0f) scale *= powf(2.0,  0.04*SMOOTH(axes[5]+1.0f)); 
        // scale *= powf(2.0, -0.1*SMOOTH(axes[4])); 
        vkdt.wstate.img_widget.look_at_x += SMOOTH(axes[0]) * wd * 0.01 / scale;
        vkdt.wstate.img_widget.look_at_y += SMOOTH(axes[1]) * ht * 0.01 / scale;
        vkdt.wstate.img_widget.scale = scale;
#undef SMOOTH
      }
      if(vkdt.graph_res == VK_SUCCESS)
      {
        int events = !vkdt.wstate.grabbed;
        // center view has on-canvas widgets (but only if there *is* an image):
        dt_image(&vkdt.wstate.img_widget, out_main, events, out_main != 0);
      }
    }

    // draw context sensitive help overlay
    if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();

    ImGui::End();
    ImGui::PopStyleColor();
  } // end center view

  if(!vkdt.wstate.fullscreen_view && vkdt.wstate.history_view)
  { // left panel
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x,
          ImGui::GetMainViewport()->Pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::Begin("panel-left", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    int action = 0;
    if(ImGui::Button("compress",  ImVec2(vkdt.state.panel_wd/3.1, 0.0))) action = 1; // compress
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("rewrite history in a compact way");
    ImGui::SameLine();
    if(ImGui::Button("roll back", ImVec2(vkdt.state.panel_wd/3.1, 0.0))) action = 2; // load previously stored cfg from disk
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("roll back to the state when entered darkroom mode");
    ImGui::SameLine();
    if(ImGui::Button("reset",     ImVec2(vkdt.state.panel_wd/3.1, 0.0))) action = 3; // load factory defaults
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("reset everything to factory defaults");
    if(action)
    {
      uint32_t imgid = dt_db_current_imgid(&vkdt.db);
      char graph_cfg[PATH_MAX+100];
      char realimg[PATH_MAX];
      dt_token_t input_module = dt_token("i-raw");

      if(action >= 2) dt_db_image_path(&vkdt.db, imgid, graph_cfg, sizeof(graph_cfg));
      if(action == 2)
      {
        struct stat statbuf;
        if(stat(graph_cfg, &statbuf)) action = 3;
      }
      if(action == 3)
      {
        realpath(graph_cfg, realimg);
        int len = strlen(realimg);
        assert(len > 4);
        realimg[len-4] = 0; // cut away ".cfg"
        input_module = dt_graph_default_input_module(realimg);
        snprintf(graph_cfg, sizeof(graph_cfg), "default-darkroom.%" PRItkn, dt_token_str(input_module));
      }

      if(action >= 2)
      { // read previous cfg or factory default cfg, first init modules to their default state:
        for(uint32_t m=0;m<vkdt.graph_dev.num_modules;m++) dt_module_reset_params(vkdt.graph_dev.module+m);
        dt_graph_read_config_ascii(&vkdt.graph_dev, graph_cfg);
      }

      if(action == 3)
      { // default needs to update input filename and search path
        dt_graph_set_searchpath(&vkdt.graph_dev, realimg);
        char *basen = fs_basename(realimg); // cut away path so we can relocate more easily
        int modid = dt_module_get(&vkdt.graph_dev, input_module, dt_token("main"));
        if(modid >= 0)
          dt_module_set_param_string(vkdt.graph_dev.module + modid, dt_token("filename"),
              basen);
      }

      dt_graph_history_reset(&vkdt.graph_dev);
      vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
    }

    ImGui::BeginChild("history-scrollpane");

    for(int i=vkdt.graph_dev.history_item_end-1;i>=0;i--)
    {
      int pop = 0;
      if(i >= (int)vkdt.graph_dev.history_item_cur)
      { // inactive
        pop = 3;
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(.01f, .01f, .01f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
      }
      else if(i+1 == (int)vkdt.graph_dev.history_item_cur)
      { // last active item
        pop = 3;
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      }
      if(ImGui::Button(vkdt.graph_dev.history_item[i]))
      {
        dt_graph_history_set(&vkdt.graph_dev, i);
        vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
      }
      if(pop) ImGui::PopStyleColor(pop);
    }
    ImGui::EndChild();
    ImGui::End();
  } // end left panel

  if(!vkdt.wstate.fullscreen_view)
  { // right panel
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + qvk.win_width - vkdt.state.panel_wd,
          ImGui::GetMainViewport()->Pos.y + 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::Begin("panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    // draw histogram image:
    dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
    if(out_hist && vkdt.graph_res == VK_SUCCESS)
    {
      int wd = vkdt.state.panel_wd * 0.975;
      // int ht = wd * 2.0f/3.0f; // force 2/3 aspect ratio
      int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
      ImGui::Image(out_hist->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES],
          ImVec2(wd, ht),
          ImVec2(0,0), ImVec2(1,1),
          ImVec4(1.0f,1.0f,1.0f,1.0f), ImVec4(1.0f,1.0f,1.0f,0.5f));
    }

    dt_node_t *out_view0 = dt_graph_get_display(&vkdt.graph_dev, dt_token("view0"));
    if(out_view0 && vkdt.graph_res == VK_SUCCESS)
    {
      float iwd = out_view0->connector[0].roi.wd;
      float iht = out_view0->connector[0].roi.ht;
      float scale = MIN(vkdt.state.panel_wd / iwd, 2.0f/3.0f*vkdt.state.panel_wd / iht);
      int ht = scale * iht;
      int wd = scale * iwd;
      ImGui::NewLine(); // end expander
      ImGui::SameLine((vkdt.state.panel_wd - wd)/2);
      ImGui::Image(out_view0->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES],
          ImVec2(wd, ht),
          ImVec2(0,0), ImVec2(1,1),
          ImVec4(1.0f,1.0f,1.0f,1.0f), ImVec4(1.0f,1.0f,1.0f,0.5f));
    }


    { // print some basic exif if we have
      const dt_image_params_t *ip = &vkdt.graph_dev.module[0].img_param;
      if(ip->exposure != 0.0f)
      {
        if(ip->exposure >= 1.0f)
          if(nearbyintf(ip->exposure) == ip->exposure)
            ImGui::Text("%s %s %.0f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
                (int)ip->focal_length, (int)ip->iso);
          else
            ImGui::Text("%s %s %.1f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
                (int)ip->focal_length, (int)ip->iso);
        /* want to catch everything below 0.3 seconds */
        else if(ip->exposure < 0.29f)
          ImGui::Text("%s %s 1/%.0f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        /* catch 1/2, 1/3 */
        else if(nearbyintf(1.0f / ip->exposure) == 1.0f / ip->exposure)
          ImGui::Text("%s %s 1/%.0f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        /* catch 1/1.3, 1/1.6, etc. */
        else if(10 * nearbyintf(10.0f / ip->exposure) == nearbyintf(100.0f / ip->exposure))
          ImGui::Text("%s %s 1/%.1f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        else
          ImGui::Text("%s %s %.1f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
      }
    }

    // tabs for module/params controls:
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if(ImGui::BeginTabBar("layer", tab_bar_flags))
    {
      if(ImGui::BeginTabItem("favourites"))
      {
        render_darkroom_favourite();
        ImGui::EndTabItem();
      }
      if(ImGui::BeginTabItem("tweak all"))
      {
        render_darkroom_full();
        ImGui::EndTabItem();
      }
      if(ImGui::BeginTabItem("pipeline config"))
      {
        render_darkroom_pipeline();
        ImGui::EndTabItem();
      }
      if(ImGui::BeginTabItem("esoteric"))
      {
        if(ImGui::CollapsingHeader("settings"))
        {
          if(ImGui::Button("hotkeys"))
            ImGui::OpenPopup("edit hotkeys");
          ImHotKey::Edit(hk_darkroom, sizeof(hk_darkroom)/sizeof(hk_darkroom[0]), "edit hotkeys");

          if(ImGui::SliderInt("LOD", &vkdt.wstate.lod, 1, 16, "%d"))
          { // LOD switcher
            dt_gui_set_lod(vkdt.wstate.lod);
          }
        }

        if(ImGui::CollapsingHeader("animation"))
        { // animation controls
          float bwd = 0.12f;
          ImVec2 size(bwd*vkdt.state.panel_wd, 0);
          if(vkdt.state.anim_playing)
          {
            if(ImGui::Button("stop", size))
              dt_gui_dr_anim_stop();
          }
          else if(ImGui::Button("play", size))
            dt_gui_dr_anim_start();
          ImGui::SameLine();
          if(ImGui::SliderInt("frame", &vkdt.state.anim_frame, 0, vkdt.state.anim_max_frame))
          {
            vkdt.graph_dev.frame = vkdt.state.anim_frame;
            vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
          }
          if(ImGui::SliderInt("last frame", &vkdt.state.anim_max_frame, 0, 10000))
          {
            vkdt.graph_dev.frame_cnt = vkdt.state.anim_max_frame+1;
            dt_graph_history_global(&vkdt.graph_dev);
          }
          float frame_rate = vkdt.graph_dev.frame_rate;
          if(ImGui::SliderFloat("frame rate", &frame_rate, 0, 200))
          {
            vkdt.graph_dev.frame_rate = frame_rate; // conv to double
            dt_graph_history_global(&vkdt.graph_dev);
          }
          if(ImGui::Button("force downloading all outputs", ImVec2(-1, 0)))
          {
            vkdt.graph_dev.runflags = s_graph_run_download_sink;
          }
          if(ImGui::IsItemHovered())
            ImGui::SetTooltip("this is useful if an animated graph has output modules\n"
                              "attached to it. for instance this allows you to trigger\n"
                              "writing of intermediate results of an optimisation from the gui.\n"
                              "only works when the animation is stopped.");
        }

        if(ImGui::CollapsingHeader("presets"))
        {
          ImVec2 size((vkdt.state.panel_wd-4)/2, 0);
          if(ImGui::Button("create preset", size))
            gui.hotkey = s_hotkey_create_preset;
          ImGui::SameLine();
          if(ImGui::Button("apply preset", size))
            gui.hotkey = s_hotkey_apply_preset;
        }

        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    ImGui::End();
  } // end right panel

  switch(gui.hotkey)
  {
    case s_hotkey_create_preset:
      dt_gui_dr_preset_create();
      break;
    case s_hotkey_apply_preset:
      dt_gui_dr_preset_apply();
      break;
    case s_hotkey_show_history:
      dt_gui_dr_toggle_history();
      break;
    case s_hotkey_redo:
      dt_gui_dr_show_history();
      dt_gui_dr_history_redo();
      break;
    case s_hotkey_undo:
      dt_gui_dr_show_history();
      dt_gui_dr_history_undo();
      break;
    case s_hotkey_module_add:
      dt_gui_dr_module_add();
      break;
    case s_hotkey_block_add:
      ImGui::OpenPopup("insert block");
      break;
    case s_hotkey_assign_tag:
      dt_gui_dr_assign_tag();
      break;
    case s_hotkey_nodes_enter:
      dt_view_switch(s_view_nodes);
      break;
    case s_hotkey_fullscreen:
      dt_gui_dr_toggle_fullscreen_view();
      break;
    case s_hotkey_rate_0: dt_gui_rate_0(); break;
    case s_hotkey_rate_1: dt_gui_rate_1(); break;
    case s_hotkey_rate_2: dt_gui_rate_2(); break;
    case s_hotkey_rate_3: dt_gui_rate_3(); break;
    case s_hotkey_rate_4: dt_gui_rate_4(); break;
    case s_hotkey_rate_5: dt_gui_rate_5(); break;
    case s_hotkey_label_1: dt_gui_label_1(); break;
    case s_hotkey_label_2: dt_gui_label_2(); break;
    case s_hotkey_label_3: dt_gui_label_3(); break;
    case s_hotkey_label_4: dt_gui_label_4(); break;
    case s_hotkey_label_5: dt_gui_label_5(); break;
    default:;
  }
  dt_gui_dr_modals(); // draw modal windows for presets etc
  // this one uses state that is local to our file, so it's kept here:
  if(ImGui::BeginPopupModal("insert block", NULL, ImGuiWindowFlags_AlwaysAutoResize))
  {
    static char mod_inst[10] = "01";
    ImGui::InputText("instance", mod_inst, 8);
    gui.block_token[0] = dt_token(mod_inst);
    static char filter[256] = "";
    int ok = filteredlist("%s/data/blocks", "%s/blocks", filter, gui.block_filename, sizeof(gui.block_filename), s_filteredlist_default);
    if(ok) ImGui::CloseCurrentPopup();
    if(ok == 1)
      gui.state = gui_state_data_t::s_gui_state_insert_block;
    // .. and render_module() will continue adding it using the data in gui.block* when the "insert before this" button is pressed.
    ImGui::EndPopup();
  }
}

void render_darkroom_init()
{
  ImHotKey::Deserialise("darkroom", hk_darkroom, sizeof(hk_darkroom)/sizeof(hk_darkroom[0]));
}

void render_darkroom_cleanup()
{
  ImHotKey::Serialise("darkroom", hk_darkroom, sizeof(hk_darkroom)/sizeof(hk_darkroom[0]));
  widget_end(); // commit params if still ongoing
}
