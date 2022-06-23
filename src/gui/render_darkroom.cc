// the imgui render functions for the darkroom view
extern "C"
{
#include "gui/view.h"
#include "gui/darkroom-util.h"
#include "pipe/modules/api.h"
#include "pipe/graph-history.h"
}
#include "gui/render_view.hh"
#include "gui/hotkey.hh"
#include "gui/api.hh"

namespace { // anonymous namespace

static ImHotKey::HotKey hk_darkroom[] = {
  {"create preset", "create new preset from image",     GLFW_KEY_LEFT_CONTROL, GLFW_KEY_O},
  {"apply preset",  "choose preset to apply",           GLFW_KEY_LEFT_CONTROL, GLFW_KEY_P},
  {"show history",  "toggle visibility of left panel",  GLFW_KEY_LEFT_CONTROL, GLFW_KEY_H},
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
} gui = {gui_state_data_t::s_gui_state_regular};


int dt_module_connect_with_history(
    dt_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
  int cerr = dt_module_connect(graph, m0, c0, m1, c1);
  if(cerr) return cerr;
  dt_graph_history_connection(graph, m1, c1); // history records only inputs
  return cerr;
}

void widget_end()
{
  if(!vkdt.wstate.grabbed)
  {
    if(vkdt.wstate.active_widget_modid < 0) return; // all good already
    // rerun all (roi could have changed, buttons are drastic)
    vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(s_graph_run_all);
    int modid = vkdt.wstate.active_widget_modid;
    int parid = vkdt.wstate.active_widget_parid;
    int parnm = vkdt.wstate.active_widget_parnm;
    int parsz = vkdt.wstate.active_widget_parsz;
    if(vkdt.wstate.mapped)
    {
      vkdt.wstate.mapped = 0;
    }
    else if(parsz)
    {
      const dt_ui_param_t *p = vkdt.graph_dev.module[modid].so->param[parid];
      float *v = (float*)(vkdt.graph_dev.module[modid].param + p->offset + parsz * parnm);
      memcpy(v, vkdt.wstate.state, parsz);
    }
  }
  dt_gui_ungrab_mouse();
  vkdt.wstate.active_widget_modid = -1;
  vkdt.wstate.selected = -1;
  vkdt.wstate.m_x = vkdt.wstate.m_y = -1.;
}

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
  darkroom_reset_zoom();
}

uint64_t render_module(dt_graph_t *graph, dt_module_t *module, int connected)
{
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

inline void draw_widget(int modid, int parid)
{
  const dt_ui_param_t *param = vkdt.graph_dev.module[modid].so->param[parid];
  if(!param) return;
  ImGuiIO& io = ImGui::GetIO();

  // skip if group mode does not match:
  if(param->widget.grpid != -1)
    if(dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.grpid)[0] != param->widget.mode)
      return;

  int axes_cnt = 0;
  const float *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt) : 0;
  static int gamepad_reset = 0;
  if(dt_gui_imgui_nav_button(12)) gamepad_reset = 1;
  // some state for double click detection for reset functionality
  static int doubleclick = 0;
  static double doubleclick_time = 0;
  double time_now = ImGui::GetTime();
#define RESETBLOCK \
  {\
    if(time_now - doubleclick_time > ImGui::GetIO().MouseDoubleClickTime) doubleclick = 0;\
    if(doubleclick) memcpy(vkdt.graph_dev.module[modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));\
    change = 1;\
  }\
  if((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) || \
     (ImGui::IsItemFocused() && gamepad_reset))\
  {\
    doubleclick_time = time_now;\
    gamepad_reset = 0;\
    doubleclick = 1;\
    memcpy(vkdt.graph_dev.module[modid].param + param->offset, param->val, dt_ui_param_size(param->type, param->cnt));\
    change = 1;\
  }\
  if(change)

  // common code block to insert a keyframe. currently only supports float (for interpolation)
  static double keyframe_time = glfwGetTime();
#define KEYFRAME\
  if(ImGui::IsItemHovered())\
  {\
    double now = glfwGetTime(); \
    if(glfwGetKey(qvk.window, GLFW_KEY_K) == GLFW_PRESS && now - keyframe_time > 1.0)\
    {\
      dt_graph_t *g = &vkdt.graph_dev;\
      uint32_t ki = -1u;\
      for(uint32_t i=0;ki<0&&i<g->module[modid].keyframe_cnt;i++)\
        if(g->module[modid].keyframe[i].param == param->name && \
           g->module[modid].keyframe[i].frame == g->frame)\
          ki = i;\
      if(ki == -1u)\
      {\
        ki = g->module[modid].keyframe_cnt++;\
        g->module[modid].keyframe = (dt_keyframe_t *)dt_realloc(g->module[modid].keyframe, &g->module[modid].keyframe_size, sizeof(dt_keyframe_t)*(ki+1));\
        g->module[modid].keyframe[ki].beg   = 0;\
        g->module[modid].keyframe[ki].end   = count;\
        g->module[modid].keyframe[ki].frame = g->frame;\
        g->module[modid].keyframe[ki].param = param->name;\
        g->module[modid].keyframe[ki].data  = g->params_pool + g->params_end;\
        g->params_end += dt_ui_param_size(param->type, count);\
        assert(g->params_end <= g->params_max);\
      }\
      memcpy(g->module[modid].keyframe[ki].data, g->module[modid].param + param->offset, dt_ui_param_size(param->type, count));\
      dt_gui_notification("added keyframe for frame %u %" PRItkn ":%" PRItkn ":%" PRItkn, \
          g->frame, dt_token_str(g->module[modid].name), dt_token_str(g->module[modid].inst), dt_token_str(param->name));\
      keyframe_time = now; \
      dt_graph_history_keyframe(&vkdt.graph_dev, modid, ki);\
    }\
  }
#define TOOLTIP \
  if(param->tooltip && ImGui::IsItemHovered())\
  {\
    ImGui::BeginTooltip();\
    ImGui::PushTextWrapPos(vkdt.state.panel_wd);\
    ImGui::TextUnformatted(param->tooltip);\
    ImGui::PopTextWrapPos();\
    ImGui::EndTooltip();\
  }

  const double throttle = 2.0; // min delay for same param in history, in seconds
  // distinguish by count:
  // get count by param cnt or explicit multiplicity from ui file
  int count = 1, change = 0;
  if(param->name == dt_token("draw")) // TODO: wire through named count too?
    count = 2*dt_module_param_int(vkdt.graph_dev.module + modid, parid)[0]+1; // vertex count + list of 2d vertices
  else if(param->widget.cntid == -1)
    count = param->cnt; // if we know nothing else, we use all elements
  else
    count = CLAMP(dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.cntid)[0], 0, param->cnt);
  for(int num=0;num<count;num++)
  {
  ImGui::PushID(2000*modid + 200*parid + num);
  char string[256];
  // distinguish by type:
  switch(param->widget.type)
  {
    case dt_token("slider"):
    {
      if(param->type == dt_token("float"))
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        float oldval = *val;
        char str[10] = {0};
        memcpy(str, &param->name, 8);
        if(ImGui::SliderFloat(str, val, param->widget.min, param->widget.max, "%2.5f"))
        RESETBLOCK {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              s_graph_run_record_cmd_buf | s_graph_run_wait_done | flags);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        KEYFRAME
        TOOLTIP
      }
      else if(param->type == dt_token("int"))
      {
        int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        int32_t oldval = *val;
        char str[10] = {0};
        memcpy(str, &param->name, 8);
        if(ImGui::SliderInt(str, val, param->widget.min, param->widget.max, "%d"))
        RESETBLOCK {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              flags | s_graph_run_record_cmd_buf | s_graph_run_wait_done);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        TOOLTIP
      }
      break;
    }
    case dt_token("vslider"):
    {
      if(param->type == dt_token("float"))
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        float oldval = *val;
        char str[10] = {0};
        memcpy(str, &param->name, 8);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.6f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.7f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, (ImVec4)ImColor::HSV(2*num / 7.0f, 0.9f, 0.9f));
        if(ImGui::VSliderFloat("##v",
              ImVec2(vkdt.state.panel_wd / 10.0, vkdt.state.panel_ht * 0.2), val,
              param->widget.min, param->widget.max, ""))
        RESETBLOCK {
          if(io.KeyShift) // lockstep all three if shift is pressed
            for(int k=3*(num/3);k<3*(num/3)+3;k++) val[k-num] = val[0];
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              s_graph_run_record_cmd_buf | s_graph_run_wait_done | flags);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        KEYFRAME
        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
          ImGui::SetTooltip("%s %.3f\nhold shift to lockstep rgb", str, val[0]);

        ImGui::PopStyleColor(4);
        if(parid < vkdt.graph_dev.module[modid].so->num_params - 1 ||
            num < count - 1) ImGui::SameLine();
      }
      break;
    }
    case dt_token("callback"):
    { // special callback button
      if(num == 0)
      {
        char str[10] = {0};
        memcpy(str, &param->name, 8);
        ImVec2 size(0.5*vkdt.state.panel_wd, 0);
        if(ImGui::Button(str, size))
        {
          dt_module_t *m = vkdt.graph_dev.module+modid;
          if(m->so->ui_callback) m->so->ui_callback(m, param->name);
          // TODO: probably needs a history item appended. for all parameters?
        }
        TOOLTIP
        if(param->type == dt_token("string"))
        {
          ImGui::SameLine();
          char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
          ImGui::InputText("", v, count);
        }
      }
      break;
    }
    case dt_token("combo"):
    { // combo box
      if(param->type == dt_token("int"))
      {
        int32_t *val = (int32_t*)(vkdt.graph_dev.module[modid].param + param->offset) + num;
        int32_t oldval = *val;
        char str[10] = {0};
        memcpy(str, &param->name, 8);
        if(ImGui::Combo(str, val, (const char *)param->widget.data))
        {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              flags | s_graph_run_record_cmd_buf | s_graph_run_wait_done);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
          vkdt.wstate.busy += 2;
        }
        TOOLTIP
      }
      break;
    }
    case dt_token("colour"):
    {
      char str[21] = {0};
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
      snprintf(str, sizeof(str), "%" PRItkn " %d", dt_token_str(param->name), num);
      ImVec4 col(val[0], val[1], val[2], 1.0f);
      ImVec2 size(0.1*vkdt.state.panel_wd, 0.1*vkdt.state.panel_wd);
      ImGui::ColorButton(str, col, ImGuiColorEditFlags_HDR, size);
      TOOLTIP
      if((num < count - 1) && ((num % 6) != 5))
        ImGui::SameLine();
      break;
    }
    case dt_token("pers"):
    {
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        int accept = 0;
        if(dt_gui_imgui_nav_input(ImGuiNavInput_TweakFast) > 0.0f)
        {
          vkdt.wstate.selected ++;
          if(vkdt.wstate.selected == 4) vkdt.wstate.selected = 0;
        }
        if(dt_gui_imgui_nav_input(ImGuiNavInput_Activate) > 0.0f)
          accept = 1;
        const float scale = vkdt.state.scale > 0.0f ? vkdt.state.scale : 1.0f;
        if(vkdt.wstate.selected >= 0 && axes)
        {
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
          float inc[2] = {
              0.002f/scale * SMOOTH(axes[3]),
              0.002f/scale * SMOOTH(axes[4])};
#undef SMOOTH
          dt_gui_dr_pers_adjust(inc, 1);
        }
        snprintf(string, sizeof(string), "%" PRItkn":%" PRItkn" done",
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        if(ImGui::Button(string) || accept)
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
      }
      else
      {
        snprintf(string, sizeof(string), "%" PRItkn":%" PRItkn" start",
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        if(ImGui::Button(string))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = 0;
          vkdt.wstate.active_widget_parsz = dt_ui_param_size(param->type, param->cnt);
          // copy to quad state
          memcpy(vkdt.wstate.state, v, sizeof(float)*8);
          // reset module params so the image will not appear distorted:
          float def[] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
          memcpy(v, def, sizeof(float)*8);
          vkdt.graph_dev.runflags = s_graph_run_all;
          darkroom_reset_zoom();
        }
        if(0) RESETBLOCK {
          vkdt.graph_dev.runflags = s_graph_run_all;
          darkroom_reset_zoom();
        }
        TOOLTIP
      }
      num = count;
      break;
    }
    case dt_token("crop"):
    {
      ImGui::InputFloat("aspect ratio", &vkdt.wstate.aspect, 0.0f, 4.0f, "%.3f");
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      const float iwd = vkdt.graph_dev.module[modid].connector[0].roi.wd;
      const float iht = vkdt.graph_dev.module[modid].connector[0].roi.ht;
      const float aspect = iwd/iht;
      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        int accept = 0;
        if(dt_gui_imgui_nav_input(ImGuiNavInput_TweakFast) > 0.0f)
        {
          vkdt.wstate.selected ++;
          if(vkdt.wstate.selected == 4) vkdt.wstate.selected = 0;
        }
        if(dt_gui_imgui_nav_input(ImGuiNavInput_Activate) > 0.0f)
          accept = 1;

        int axes_cnt = 0;
        const float* axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt) : 0;
        const float scale = vkdt.state.scale > 0.0f ? vkdt.state.scale : 1.0f;
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
        if(vkdt.wstate.selected >= 0 && axes)
          dt_gui_dr_crop_adjust(0.002f/scale * SMOOTH(axes[vkdt.wstate.selected < 2 ? 3 : 4]), 1);
#undef SMOOTH

        snprintf(string, sizeof(string), "%" PRItkn":%" PRItkn" done",
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        if(ImGui::Button(string) || accept)
        {
          vkdt.wstate.state[0] = .5f + MAX(1.0f, 1.0f/aspect) * (vkdt.wstate.state[0] - .5f);
          vkdt.wstate.state[1] = .5f + MAX(1.0f, 1.0f/aspect) * (vkdt.wstate.state[1] - .5f);
          vkdt.wstate.state[2] = .5f + MAX(1.0f,      aspect) * (vkdt.wstate.state[2] - .5f);
          vkdt.wstate.state[3] = .5f + MAX(1.0f,      aspect) * (vkdt.wstate.state[3] - .5f);
          widget_end();
          darkroom_reset_zoom();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
      }
      else
      {
        snprintf(string, sizeof(string), "%" PRItkn":%" PRItkn" start",
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        if(ImGui::Button(string))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = 0;
          vkdt.wstate.active_widget_parsz = dt_ui_param_size(param->type, param->cnt);
          // copy to quad state
          memcpy(vkdt.wstate.state, v, sizeof(float)*4);

          // the values we draw are relative to output of the whole pipeline,
          // but the coordinates of crop are relative to the *input*
          // coordinates of the module!
          // the output is the anticipated output while we switched off crop, i.e
          // using the default values to result in a square of max(iwd, iht)^2
          // first convert these v[] from input w/h to output w/h of the module:
          const float iwd = vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].connector[0].roi.wd;
          const float iht = vkdt.graph_dev.module[vkdt.wstate.active_widget_modid].connector[0].roi.ht;
          const float owd = MAX(iwd, iht);
          const float oht = MAX(iwd, iht);
          vkdt.wstate.state[0] = .5f +  iwd/owd * (vkdt.wstate.state[0] - .5f);
          vkdt.wstate.state[1] = .5f +  iwd/owd * (vkdt.wstate.state[1] - .5f);
          vkdt.wstate.state[2] = .5f +  iht/oht * (vkdt.wstate.state[2] - .5f);
          vkdt.wstate.state[3] = .5f +  iht/oht * (vkdt.wstate.state[3] - .5f);

          // reset module params so the image will not appear cropped:
          float def[] = {
            .5f + MAX(1.0f, 1.0f/aspect) * (0.0f - .5f),
            .5f + MAX(1.0f, 1.0f/aspect) * (1.0f - .5f),
            .5f + MAX(1.0f,      aspect) * (0.0f - .5f),
            .5f + MAX(1.0f,      aspect) * (1.0f - .5f)};
          memcpy(v, def, sizeof(float)*4);
          vkdt.graph_dev.runflags = s_graph_run_all;
          darkroom_reset_zoom();
        }
        if(0) RESETBLOCK {
          vkdt.graph_dev.runflags = s_graph_run_all;
          darkroom_reset_zoom();
        }
        KEYFRAME
        TOOLTIP
      }
      num = count;
      break;
    }
    case dt_token("pick"):  // simple aabb for selection, no distortion transform
    {
      int sz = dt_ui_param_size(param->type, 4);
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid &&
         vkdt.wstate.active_widget_parnm == num)
      {
        snprintf(string, sizeof(string), "done");
        if(ImGui::Button(string))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
      }
      else
      {
        snprintf(string, sizeof(string), "%02d", num);
        if(ImGui::Button(string))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = num;
          vkdt.wstate.active_widget_parsz = sz;
          // copy to quad state
          memcpy(vkdt.wstate.state, v, sz);
        }
        TOOLTIP
      }
      if((num < count - 1) && ((num % 6) != 5))
        ImGui::SameLine();
      break;
    }
    case dt_token("rbmap"): // red/blue chromaticity mapping via src/target coordinates
    {
      if(num == 0) ImGui::Dummy(ImVec2(0, 0.01*vkdt.state.panel_wd));
      ImVec2 size = ImVec2(vkdt.state.panel_wd * 0.14, 0);
      int sz = dt_ui_param_size(param->type, 4); // src rb -> tgt rb is four floats
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset + num*sz);
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(v[0], 1.0-v[0]-v[1], v[1], 1.0));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(v[2], 1.0-v[2]-v[3], v[3], 1.0));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, vkdt.state.panel_wd*0.02);
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid &&
         vkdt.wstate.active_widget_parnm == num)
      {
        snprintf(string, sizeof(string), "done");
        if(ImGui::Button(string, size))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
      }
      else
      {
        snprintf(string, sizeof(string), "%02d", num);
        if(ImGui::Button(string, size))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = num;
          vkdt.wstate.active_widget_parsz = 0;
        }
        count *= 4; // keyframe needs to know it's 4 floats too
        KEYFRAME
        count /= 4;
      }
      ImGui::PopStyleVar(1);
      ImGui::PopStyleColor(2);
      if((num < count - 1) && ((num % 6) != 5))
      {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(0.01*vkdt.state.panel_wd, 0));
        ImGui::SameLine();
      }
      else if(num == count - 1)
      { // edit specific colour patch below list of patches:
        ImGui::Dummy(ImVec2(0, 0.01*vkdt.state.panel_wd));
        if(vkdt.wstate.active_widget_modid == modid &&
           vkdt.wstate.active_widget_parid == parid)
        { // now add ability to change target colour coordinate
          int active_num = vkdt.wstate.active_widget_parnm;
          for(int i=0;i<2;i++)
          {
            float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset + active_num*sz) + 2 + i;
            float oldval = *val;
            if(ImGui::SliderFloat(i ? "blue" : "red", val, 0.0, 1.0, "%2.5f"))
            { // custom resetblock: set only this colour spot to identity mapping
              if(time_now - doubleclick_time > ImGui::GetIO().MouseDoubleClickTime) doubleclick = 0;
              if(doubleclick) memcpy(val-i, val-i-2, sizeof(float)*2);
              change = 1;
            }
            if((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                (ImGui::IsItemFocused() && gamepad_reset))
            {
              doubleclick_time = time_now;
              gamepad_reset = 0;
              doubleclick = 1;
              memcpy(val-i, val-i-2, sizeof(float)*2);
              change = 1;
            }
            if(change)
            {
              dt_graph_run_t flags = s_graph_run_none;
              if(vkdt.graph_dev.module[modid].so->check_params)
                flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
              vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
                  s_graph_run_record_cmd_buf | s_graph_run_wait_done | flags);
              vkdt.graph_dev.active_module = modid;
              dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
            }
          }
        }
      }
      else ImGui::Dummy(ImVec2(0, 0.02*vkdt.state.panel_wd));
      break;
    }
    case dt_token("grab"):  // grab all input
    {
      if(num != 0) break;
      if(vkdt.wstate.active_widget_modid == modid &&
         vkdt.wstate.active_widget_parid == parid)
      {
        if(ImGui::Button("stop [esc]"))
        {
          // dt_gui_dr_toggle_fullscreen_view();
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_module_input_event_t p = { .type = -1 };
          if(modid >= 0 && mod->so->input) mod->so->input(mod, &p);
          widget_end();
        }
      }
      else
      {
        if(ImGui::Button("grab input"))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.state.anim_no_keyframes = 1; // switch off animation, we will be moving ourselves
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          dt_module_input_event_t p = { 0 };
          dt_module_t *mod = vkdt.graph_dev.module + modid;
          dt_gui_grab_mouse();
          if(modid >= 0)
            if(mod->so->input) mod->so->input(mod, &p);
        }
        KEYFRAME
        TOOLTIP
      }
      break;
    }
    case dt_token("draw"):
    {
      float *v = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      if(vkdt.wstate.active_widget_modid == modid && vkdt.wstate.active_widget_parid == parid)
      {
        snprintf(string, sizeof(string), "%" PRItkn":%" PRItkn" done",
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        if(ImGui::Button(string))
        {
          widget_end();
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
      }
      else
      {
        snprintf(string, sizeof(string), "%" PRItkn":%" PRItkn" start",
            dt_token_str(vkdt.graph_dev.module[modid].name),
            dt_token_str(param->name));
        if(ImGui::Button(string))
        {
          widget_end(); // if another one is still in progress, end that now
          vkdt.wstate.state[0] = 1.0f; // abuse for radius
          vkdt.wstate.state[1] = 1.0f; // abuse for opacity
          vkdt.wstate.state[2] = 1.0f; // abuse for hardness
          // get base radius from other param
          int pid = dt_module_get_param(vkdt.graph_dev.module[modid].so, dt_token("radius"));
          if(pid >= 0) vkdt.wstate.state[3] = dt_module_param_float(
              vkdt.graph_dev.module+modid, pid)[0];
          else vkdt.wstate.state[3] = 1.0;
          vkdt.wstate.state[4] = vkdt.graph_dev.module[modid].connector[0].roi.wd;
          vkdt.wstate.active_widget_modid = modid;
          vkdt.wstate.active_widget_parid = parid;
          vkdt.wstate.active_widget_parnm = 0;
          // TODO: how to crop this to smaller size in case it's not required?
          vkdt.wstate.active_widget_parsz = dt_ui_param_size(param->type, param->cnt);
          // for sanity also keep mapped_size to make clear that it belongs to the mapping, not the copy
          vkdt.wstate.mapped_size = dt_ui_param_size(param->type, param->cnt);
          vkdt.wstate.mapped = v; // map state
        }
        KEYFRAME
        if(ImGui::IsItemHovered())
          ImGui::SetTooltip("start drawing brush strokes with the mouse\n"
              "scroll - fine tune radius\n"
              "ctrl scroll - fine tune hardness\n"
              "shift scroll - fine tune opacity\n"
              "right click - discard last stroke");
      }
      if(vkdt.wstate.mapped)
      {
        ImGui::SameLine();
        ImGui::Text("%d/10000 verts", ((uint32_t *)vkdt.wstate.mapped)[0]);
      }
      num = count;
      break;
    }
    case dt_token("filename"):
    {
      if(num == 0)
      { // only show first, cnt refers to allocation length of string param
        char *v = (char *)(vkdt.graph_dev.module[modid].param + param->offset);
        if(ImGui::InputText("filename", v, count))
        {
          vkdt.graph_dev.runflags = s_graph_run_all; // kinda grave change, rerun all
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
      }
      break;
    }
    case dt_token("rgb"):
    {
      float *col = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num;
      ImGui::PushStyleColor(ImGuiCol_FrameBg, gamma(ImVec4(col[0], col[1], col[2], 1.0)));
      for(int comp=0;comp<3;comp++)
      {
        float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset) + 3*num + comp;
        float oldval = *val;
        char str[32] = {0};
        snprintf(str, sizeof(str), "%" PRItkn " %s",
            dt_token_str(param->name),
            comp == 0 ? "red" : (comp == 1 ? "green" : "blue"));
        if(ImGui::SliderFloat(str, val,
              param->widget.min, param->widget.max, "%2.5f"))
        RESETBLOCK
        {
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              s_graph_run_record_cmd_buf | s_graph_run_wait_done | flags);
          vkdt.graph_dev.active_module = modid;
          dt_graph_history_append(&vkdt.graph_dev, modid, parid, throttle);
        }
        KEYFRAME
        TOOLTIP
      }
      ImGui::PopStyleColor();
      if(param->cnt == count && count <= 4) num = 4; // non-array rgb controls
      break;
    }
    case dt_token("print"):
    {
      float *val = (float*)(vkdt.graph_dev.module[modid].param + param->offset);
      ImGui::Text("%g | %g | %g   %" PRItkn, val[0], val[1], val[2], dt_token_str(param->name));
      num = count; // we've done it all at once
      break;
    }
    default:;
  }
  ImGui::PopID();
  } // end for multiple widgets
#undef RESETBLOCK
#undef KEYFRAME
#undef TOOLTIP
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
        draw_widget(vkdt.fav_modid[i], vkdt.fav_parid[i]);
        break;
      }
    }
  }
}

void render_darkroom_full()
{
  char name[30];
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
  {
    int curr = modid[m];
    if(arr[curr].so->num_params)
    {
      snprintf(name, sizeof(name), "%" PRItkn " %" PRItkn,
          dt_token_str(arr[curr].name), dt_token_str(arr[curr].inst));
      if(ImGui::CollapsingHeader(name))
      {
        if(!open[m])
        { // just opened, now this is the 'active module'.
          // fprintf(stderr, "module %" PRItkn " got focus!\n", dt_token_str(arr[curr].name));
          active_module = curr;
          int cid = dt_module_get_connector(arr+curr, dt_token("dspy"));
          if(cid >= 0)
          { // if 'dspy' output exists, connect to 'dspy' display
            int mid = dt_module_add(graph, dt_token("display"), dt_token("dspy"));
            if(graph->module[mid].connector[0].connected_mi != curr ||
               graph->module[mid].connector[0].connected_mc != cid)
            { // only if not connected yet, don't record in history
              CONN(dt_module_connect(graph, curr, cid, mid, 0));
              vkdt.graph_dev.runflags = s_graph_run_all;
            }
          }
          open[m] = 1;
        }
        if(active_module == curr &&
          dt_module_get_connector(arr+curr, dt_token("dspy")) >= 0)
        {
          dt_node_t *out_dspy = dt_graph_get_display(graph, dt_token("dspy"));
          if(out_dspy && vkdt.graph_res == VK_SUCCESS)
          {
            float iwd = out_dspy->connector[0].roi.wd;
            float iht = out_dspy->connector[0].roi.ht;
            float scale = MIN(vkdt.state.panel_wd / iwd, 2.0f/3.0f*vkdt.state.panel_wd / iht);
            int ht = scale * iht;
            int wd = scale * iwd;
            ImGui::NewLine(); // end expander
            ImGui::SameLine((vkdt.state.panel_wd - wd)/2);
            ImGui::Image(out_dspy->dset[graph->frame % DT_GRAPH_MAX_FRAMES],
                ImVec2(wd, ht),
                ImVec2(0,0), ImVec2(1,1),
                ImVec4(1.0f,1.0f,1.0f,1.0f), ImVec4(1.0f,1.0f,1.0f,0.5f));
          }
        }
        for(int i=0;i<arr[curr].so->num_params;i++)
          draw_widget(curr, i);
      }
      else open[m] = 0;
    }
  }
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
  static int add_modid = 0; ImGui::Combo("module", &add_modid, vkdt.wstate.module_names_buf);
  static char mod_inst[10] = "01"; ImGui::InputText("instance", mod_inst, 8);
  if(ImGui::Button("add module"))
  {
    int new_modid = dt_module_add(graph, dt_token(vkdt.wstate.module_names[add_modid]), dt_token(mod_inst));
    if(new_modid == -1) last_err = 16ul<<32;
    else dt_graph_history_module(graph, new_modid);
  }

  // add block (read cfg snipped)
  if(gui.state == gui_state_data_t::s_gui_state_insert_block)
  {
    if(ImGui::Button("insert disconnected"))
    {
      dt_graph_read_block(&vkdt.graph_dev, gui.block_filename,
          dt_token(mod_inst),
          dt_token(""), dt_token(""), dt_token(""),
          dt_token(""), dt_token(""), dt_token(""));
      gui.state = gui_state_data_t::s_gui_state_regular;
    }
    ImGui::SameLine();
    if(ImGui::Button("abort"))
      gui.state = gui_state_data_t::s_gui_state_regular;
  }
  else
  {
    if(ImGui::BeginPopupModal("insert block", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    { // "ok" pressed
      static char filter[256] = "";
      int ok = filteredlist("%s/data/blocks", "%s/blocks", filter, gui.block_filename, sizeof(gui.block_filename), 0);
      if(ok) ImGui::CloseCurrentPopup();
      if(ok == 1)
        gui.state = gui_state_data_t::s_gui_state_insert_block;
      // .. and render_module() will continue adding it using the data in gui.block* when the "insert before this" button is pressed.
      ImGui::EndPopup();
    }
    if(ImGui::Button("insert block.."))
    {
      gui.block_token[0] = dt_token(mod_inst);
      ImGui::OpenPopup("insert block");
    }
  }
}

} // end anonymous namespace


void render_darkroom()
{
  int axes_cnt = 0, butt_cnt = 0;
  const uint8_t *butt = vkdt.wstate.have_joystick ? glfwGetJoystickButtons(GLFW_JOYSTICK_1, &butt_cnt) : 0;
  const float   *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt)    : 0;
  { // center image view
    int border = vkdt.style.border_frac * qvk.win_width;
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    // draw background over the full thing:
    ImGui::SetNextWindowPos (ImVec2(win_x-  border, win_y-  border), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+2*border, win_h+2*border), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("darkroom center", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGuiIO& io = ImGui::GetIO();
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
      static int fs_state = 0;
      if(butt && !butt[6] && !butt[7]) fs_state = 0;
      else if(fs_state == 0 && butt && butt[6]) fs_state = 1;
      else if(fs_state == 1 && butt && butt[6] && butt[7])
      {
        fs_state = 2;
        dt_gui_dr_toggle_fullscreen_view();
      }

      if(dt_gui_imgui_nav_input(ImGuiNavInput_Cancel) > 0.0f)
      {
        dt_view_switch(s_view_lighttable);
        vkdt.wstate.set_nav_focus = 2; // introduce some delay because imgui nav has it too
        goto abort;
      }
      // disable keyboard nav ctrl + shift to change images:
      else if(io.NavInputs[ImGuiNavInput_Menu] == 0.0f &&
              io.NavInputs[ImGuiNavInput_TweakSlow] > 0.0f && !io.KeyCtrl)
        dt_gui_dr_prev();
      else if(io.NavInputs[ImGuiNavInput_Menu] == 0.0f &&
              io.NavInputs[ImGuiNavInput_TweakFast] > 0.0f && !io.KeyShift)
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
      if(dt_gui_imgui_nav_button(11)) // left stick pressed
        darkroom_reset_zoom();
      if(axes)
      {
#define SMOOTH(X) copysignf(MAX(0.0f, fabsf(X) - 0.05f), X)
        float wd  = (float)out_main->connector[0].roi.wd;
        float ht  = (float)out_main->connector[0].roi.ht;
        float imwd = win_w, imht = win_h;
        float scale = MIN(imwd/wd, imht/ht);
        if(vkdt.state.scale > 0.0f) scale = vkdt.state.scale;
        if(axes[2] > -1.0f) scale *= powf(2.0, -0.04*SMOOTH(axes[2]+1.0f)); 
        if(axes[5] > -1.0f) scale *= powf(2.0,  0.04*SMOOTH(axes[5]+1.0f)); 
        // scale *= powf(2.0, -0.1*SMOOTH(axes[4])); 
        vkdt.state.look_at_x += SMOOTH(axes[0]) * wd * 0.01 / scale;
        vkdt.state.look_at_y += SMOOTH(axes[1]) * ht * 0.01 / scale;
        vkdt.state.scale = scale;
#undef SMOOTH
      }
      if(vkdt.graph_res == VK_SUCCESS)
      {
        ImTextureID imgid = out_main->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES];
        float im0[2], im1[2];
        float v0[2] = {(float)win_x, (float)win_y};
        float v1[2] = {(float)win_x+win_w, (float)win_y+win_h};
        dt_view_to_image(v0, im0);
        dt_view_to_image(v1, im1);
        im0[0] = CLAMP(im0[0], 0.0f, 1.0f);
        im0[1] = CLAMP(im0[1], 0.0f, 1.0f);
        im1[0] = CLAMP(im1[0], 0.0f, 1.0f);
        im1[1] = CLAMP(im1[1], 0.0f, 1.0f);
        dt_image_to_view(im0, v0);
        dt_image_to_view(im1, v1);
        ImGui::GetWindowDrawList()->AddImage(
            imgid, ImVec2(v0[0], v0[1]), ImVec2(v1[0], v1[1]),
            ImVec2(im0[0], im0[1]), ImVec2(im1[0], im1[1]), IM_COL32_WHITE);
      }
      if(vkdt.wstate.fullscreen_view) goto abort; // no panel
    }
    // center view has on-canvas widgets:
    if(vkdt.wstate.active_widget_modid >= 0)
    {
      // distinguish by type:
      switch(vkdt.graph_dev.module[
          vkdt.wstate.active_widget_modid].so->param[
          vkdt.wstate.active_widget_parid]->widget.type)
      {
        case dt_token("pers"):
        {
          float *v = vkdt.wstate.state;
          float p[8];
          for(int k=0;k<4;k++)
            dt_image_to_view(v+2*k, p+2*k);
          ImGui::GetWindowDrawList()->AddPolyline(
              (ImVec2 *)p, 4, IM_COL32_WHITE, true, 1.0);
          if(vkdt.wstate.selected >= 0)
          {
            float q[2] = {
              p[2*vkdt.wstate.selected],
              p[2*vkdt.wstate.selected+1]};
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(q[0],q[1]), 0.02 * vkdt.state.center_wd,
                0x77777777u, 20);
          }
          break;
        }
        case dt_token("crop"):
        {
          float v[8] = {
            vkdt.wstate.state[0], vkdt.wstate.state[2], vkdt.wstate.state[1], vkdt.wstate.state[2], 
            vkdt.wstate.state[1], vkdt.wstate.state[3], vkdt.wstate.state[0], vkdt.wstate.state[3]
          };
          float p[8];
          for(int k=0;k<4;k++) dt_image_to_view(v+2*k, p+2*k);
          ImGui::GetWindowDrawList()->AddPolyline(
              (ImVec2 *)p, 4, IM_COL32_WHITE, true, 1.0);
          if(vkdt.wstate.selected >= 0)
          {
            float o = vkdt.state.center_wd * 0.02;
            float q0[8] = { p[0], p[1], p[2], p[3], p[2],   p[3]-o, p[0],   p[1]-o};
            float q1[8] = { p[2], p[3], p[4], p[5], p[4]+o, p[5],   p[2]+o, p[3]};
            float q2[8] = { p[4], p[5], p[6], p[7], p[6],   p[7]+o, p[4],   p[5]+o};
            float q3[8] = { p[6], p[7], p[0], p[1], p[0]-o, p[1],   p[6]-o, p[7]};
            float *q = q0;
            if(vkdt.wstate.selected == 0) q = q3;
            if(vkdt.wstate.selected == 1) q = q1;
            if(vkdt.wstate.selected == 2) q = q0;
            if(vkdt.wstate.selected == 3) q = q2;
            ImGui::GetWindowDrawList()->AddQuadFilled(
                ImVec2(q[0],q[1]), ImVec2(q[2],q[3]),
                ImVec2(q[4],q[5]), ImVec2(q[6],q[7]), 0x77777777u);
          }
          break;
        }
        case dt_token("pick"):
        {
          float v[8] = {
            vkdt.wstate.state[0], vkdt.wstate.state[2], vkdt.wstate.state[1], vkdt.wstate.state[2], 
            vkdt.wstate.state[1], vkdt.wstate.state[3], vkdt.wstate.state[0], vkdt.wstate.state[3]
          };
          float p[8];
          for(int k=0;k<4;k++)
            dt_image_to_view(v+2*k, p+2*k);
          ImGui::GetWindowDrawList()->AddPolyline(
              (ImVec2 *)p, 4, IM_COL32_WHITE, true, 1.0);
          break;
        }
        case dt_token("draw"):
        { // draw indicators for opacity/hardness/radius for each stroke
          ImVec2 pos = ImGui::GetMousePos();
          float radius   = vkdt.wstate.state[0];
          float opacity  = vkdt.wstate.state[1];
          float hardness = vkdt.wstate.state[2];
          float p[100];
          int cnt = sizeof(p)/sizeof(p[0])/2;

          const int modid = vkdt.wstate.active_widget_modid;
          const float scale = vkdt.state.scale <= 0.0f ?
            MIN(
                vkdt.state.center_wd / (float)
                vkdt.graph_dev.module[modid].connector[0].roi.wd,
                vkdt.state.center_ht / (float)
                vkdt.graph_dev.module[modid].connector[0].roi.ht) :
            vkdt.state.scale;
          for(int i=0;i<2;i++)
          {
            // ui scaled roi wd * radius * stroke radius
            float r = vkdt.wstate.state[4] * vkdt.wstate.state[3] * radius;
            if(i >= 1) r *= hardness;
            for(int k=0;k<cnt;k++)
            {
              p[2*k+0] = pos.x + scale * r*sin(k/(cnt-1.0)*M_PI*2.0);
              p[2*k+1] = pos.y + scale * r*cos(k/(cnt-1.0)*M_PI*2.0);
            }
            ImGui::GetWindowDrawList()->AddPolyline(
                (ImVec2 *)p, cnt, IM_COL32_WHITE, false, 4.0f/(i+1.0f));
          }
          // opacity is a quad
          float sz = 30.0f;
          p[0] = pos.x; p[1] = pos.y;
          p[4] = pos.x+sz; p[5] = pos.y+sz;
          p[2] = opacity * (pos.x+sz) + (1.0f-opacity)*(pos.x+sz/2.0f);
          p[3] = opacity *  pos.y     + (1.0f-opacity)*(pos.y+sz/2.0f);
          p[6] = opacity *  pos.x     + (1.0f-opacity)*(pos.x+sz/2.0f);
          p[7] = opacity * (pos.y+sz) + (1.0f-opacity)*(pos.y+sz/2.0f);
          ImGui::GetWindowDrawList()->AddPolyline(
              (ImVec2 *)p, 4, IM_COL32_WHITE, true, 3.0);
          break;
        }
        default:;
      }
    }
    ImGui::End();
    ImGui::PopStyleColor();
  } // end center view

  if(vkdt.wstate.history_view)
  { // left panel
    ImGui::SetNextWindowPos (ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::Begin("panel-left", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

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
    ImGui::End();
  } // end left panel

  { // right panel
    int hotkey = -1;
    static double hotkey_time = ImGui::GetTime();
    if(ImGui::GetTime() - hotkey_time > 0.1)
    {
      hotkey = ImHotKey::GetHotKey(hk_darkroom, sizeof(hk_darkroom)/sizeof(hk_darkroom[0]));
      if(hotkey > -1) hotkey_time = ImGui::GetTime();
    }
    ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::Begin("panel-right", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    // draw histogram image:
    dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
    if(out_hist && vkdt.graph_res == VK_SUCCESS)
    {
      int wd = vkdt.state.panel_wd;
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
              vkdt.state.anim_playing = 0;
          }
          else if(ImGui::Button("play", size))
            vkdt.state.anim_playing = 1;
          ImGui::SameLine();
          if(ImGui::SliderInt("frame", &vkdt.state.anim_frame, 0, vkdt.state.anim_max_frame))
          {
            vkdt.graph_dev.frame = vkdt.state.anim_frame;
            vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
          }
          if(ImGui::SliderInt("frame count", &vkdt.state.anim_max_frame, 0, 10000))
          {
            vkdt.graph_dev.frame_cnt = vkdt.state.anim_max_frame;
            dt_graph_history_global(&vkdt.graph_dev);
          }
        }

        if(ImGui::CollapsingHeader("presets"))
        {
          ImVec2 size((vkdt.state.panel_wd-4)/2, 0);
          if(ImGui::Button("create preset", size))
            hotkey = 0;
          ImGui::SameLine();
          if(ImGui::Button("apply preset", size))
            hotkey = 1;
        }

        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();

      switch(hotkey)
      {
        case 0:
          dt_gui_dr_preset_create();
          break;
        case 1:
          dt_gui_dr_preset_apply();
          break;
        case 2:
          dt_gui_dr_toggle_history();
        default:;
      }
      dt_gui_dr_modals(); // draw modal window for presets
    }

    ImGui::End();
  } // end right panel
}

void render_darkroom_cleanup()
{
  widget_end(); // commit params if still ongoing
}
