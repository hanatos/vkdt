#include "view.h"
#include "gui.h"
#include "pipe/modules/api.h"
#include "pipe/asciiio.h"
#include "pipe/graph-history.h"
#include "pipe/graph-defaults.h"
#include "nodes.h"
#include "db/hash.h"
#include "core/fs.h"
#include "render_view.h"
#include "render.h"
#include "widget_nodes.h"
#include "hotkey.h"
#include "api_gui.h"
#include "widget_image.h"
#define KEYFRAME // empty define to disable hover/keyframe behaviour
#include "render_darkroom.h"
#include <stdint.h>

static hk_t hk_nodes[] = {
  {"apply preset",    "choose preset to apply",                     {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_P}},
  {"add module",      "add a new module to the graph",              {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_M}},
};

enum hotkey_names_t
{ // for sane access in code
  s_hotkey_apply_preset    = 0,
  s_hotkey_module_add      = 1,
};

typedef struct gui_nodes_t
{
  int do_layout;          // do initial auto layout
  int node_hovered_link;
  int dual_monitor;
  dt_node_editor_t nedit;
}
gui_nodes_t;
gui_nodes_t nodes;

void render_nodes_right_panel()
{
  struct nk_context *ctx = &vkdt.ctx;
  struct nk_rect bounds = { qvk.win_width - vkdt.state.panel_wd, 0, vkdt.state.panel_wd, vkdt.state.panel_ht };
  if(!nk_begin(ctx, "nodes panel", bounds, 0))
  {
    if(vkdt.ctx.current && vkdt.ctx.current->edit.active) vkdt.wstate.nk_active_next = 1;
    nk_end(ctx);
  }
  dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
  if(out_hist && vkdt.graph_res == VK_SUCCESS && out_hist->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES])
  {
    int wd = vkdt.state.panel_wd;
    int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
    nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
    struct nk_image img = nk_image_ptr(out_hist->dset[0]);
    nk_image(ctx, img);
  }
  static dt_image_widget_t imgw[] = {
    { .look_at_x = FLT_MAX, .look_at_y = FLT_MAX, .scale=-1.0 },
    { .look_at_x = FLT_MAX, .look_at_y = FLT_MAX, .scale=-1.0 },
    { .look_at_x = FLT_MAX, .look_at_y = FLT_MAX, .scale=-1.0 }};
  dt_token_t dsp[] = { dt_token("main"), dt_token("view0"), dt_token("view1") };
  for(uint32_t d = 0; d < sizeof(dsp)/sizeof(dsp[0]); d++)
  {
    dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dsp[d]);
    if(out && vkdt.graph_res == VK_SUCCESS)
    {
      // int popout = dsp[d] == dt_token("main") && nodes.dual_monitor;
      char title[20] = {0};
      snprintf(title, sizeof(title), "nodes %" PRItkn, dt_token_str(dsp[d]));
      // if(popout) // TODO use vkdt.ctx2 and decorate with some window around it
      int wd = vkdt.state.panel_wd;
      int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
      nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
      if(dsp[d] == dt_token("main"))
        dt_image(&vkdt.ctx, &vkdt.wstate.img_widget, out, 1, 1);
      else
        dt_image(&vkdt.ctx, imgw+d, out, 1, 0);
      // if(popout) // TODO: nk_end and stuff?
    }
  }
  // expanders for selection and individual nodes:
  int sel_node_cnt = dt_node_editor_selection(&nodes.nedit, &vkdt.graph_dev, 0);
  int *sel_node_id  = (int *)alloca(sizeof(int)*sel_node_cnt);
  dt_node_editor_selection(&nodes.nedit, &vkdt.graph_dev, sel_node_id);
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  if(sel_node_cnt && nk_tree_push(ctx, NK_TREE_TAB, "selection", NK_MINIMIZED))
  {
    nk_layout_row_dynamic(ctx, row_height, 1);
    if(nk_button_label(ctx, "ab compare"))
    {
      vkdt.graph_dev.runflags = s_graph_run_all;
      for(int i=0;i<sel_node_cnt;i++)
      {
        int m = sel_node_id[i];
        int modid = dt_module_add(&vkdt.graph_dev, vkdt.graph_dev.module[m].name, vkdt.graph_dev.module[m].inst+1);
        if(modid >= 0)
        {
          dt_graph_history_module(&vkdt.graph_dev, modid);
          vkdt.graph_dev.module[modid].gui_x = vkdt.graph_dev.module[i].gui_x;
          vkdt.graph_dev.module[modid].gui_y = vkdt.graph_dev.module[i].gui_y + row_height * 10;
        }
        else
        {
          dt_gui_notification("adding the module failed!");
          break;
        }
        for(int c=0;c<vkdt.graph_dev.module[m].num_connectors;c++)
        {
          dt_connector_t *cn = vkdt.graph_dev.module[m].connector + c;
          if(dt_connected(cn) && dt_connector_input(cn))
          { // for all input connectors, see where we are going
            int m0 = cn->connected_mi, c0 = cn->connected_mc;
            dt_module_connect_with_history(&vkdt.graph_dev, m0, c0, modid, c);
            for(int j=0;j<sel_node_cnt;j++) if(sel_node_id[j] == m0)
            { // if the module id is in the selection, connect to the copy instead
              int pmodid = dt_module_add(&vkdt.graph_dev, vkdt.graph_dev.module[m0].name, vkdt.graph_dev.module[m0].inst+1);
              if(pmodid >= 0)
                dt_module_connect_with_history(&vkdt.graph_dev, pmodid, c0, modid, c);
              break;
            }
          }
          else if(dt_connected(cn) && dt_connector_output(cn) && cn->name != dt_token("dspy"))
          { // is this the exit point? connect to new ab module
            int nm[10], nc[10];
            int ncnt = dt_module_get_module_after(
                &vkdt.graph_dev, vkdt.graph_dev.module+m, nm, nc, 10);
            for(int k=0;k<ncnt;k++)
            {
              int dup = 0; // is the following node in the dup selection too?
              for(int j=0;j<sel_node_cnt;j++)
                if(sel_node_id[j] == nm[k]) {dup = 1; break; }
              if(!dup)
              { // if not, we need to connect it through an ab/module
                int mab = dt_module_add(&vkdt.graph_dev, dt_token("ab"), dt_token("ab"));
                if(mab >= 0)
                {
                  dt_module_connect_with_history(&vkdt.graph_dev, mab, 2, nm[k], nc[k]);
                  dt_module_connect_with_history(&vkdt.graph_dev, m,     c, mab, 0);
                  dt_module_connect_with_history(&vkdt.graph_dev, modid, c, mab, 1);
                  dt_graph_history_module(&vkdt.graph_dev, mab);
                  vkdt.graph_dev.module[mab].gui_x = vkdt.graph_dev.module[i].gui_x - row_height * 30;
                  vkdt.graph_dev.module[mab].gui_y = vkdt.graph_dev.module[i].gui_y;
                }
              }
            }
          }
        }
      }
    }
    if(nk_button_label(ctx, "remove selected modules"))
    {
      dt_node_editor_clear_selection(&nodes.nedit);
      for(int i=0;i<sel_node_cnt;i++)
        dt_gui_dr_remove_module(sel_node_id[i]);
      sel_node_cnt = 0;
    }
    nk_tree_pop(ctx);
  }
  for(int i=0;i<sel_node_cnt;i++)
  {
    dt_module_t *mod = vkdt.graph_dev.module + sel_node_id[i];
    if(mod->name == 0) continue; // skip deleted
    char name[100];
    snprintf(name, sizeof(name), "manage %" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
    if(nk_tree_push(ctx, NK_TREE_TAB, name, NK_MINIMIZED))
    { // expander for individual module
      nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
      nk_layout_row_dynamic(&vkdt.ctx, 0, 1);
      if(mod->so->has_inout_chain)
        dt_tooltip(mod->disabled ? "re-enable this module" :
            "temporarily disable this module without disconnecting it from the graph.\n"
            "this is just a convenience A/B switch in the ui and will not affect your\n"
            "processing history, lighttable thumbnail, or export.");
      if(mod->so->has_inout_chain && !mod->disabled && nk_button_label(ctx, "temporarily disable"))
      {
        mod->disabled = 1;
        vkdt.graph_dev.runflags = s_graph_run_all;
      }
      else if(mod->so->has_inout_chain && mod->disabled && nk_button_label(ctx, "re-enable"))
      {
        mod->disabled = 0;
        vkdt.graph_dev.runflags = s_graph_run_all;
      }
      dt_tooltip("disconnect all connectors of this module, try to\n"
                 "establish links to the neighbours directly where possible");
      if(nk_button_label(ctx, "disconnect module"))
      {
        dt_node_editor_clear_selection(&nodes.nedit);
        dt_gui_dr_disconnect_module(sel_node_id[i]);
      }
      dt_tooltip("remove this module from the graph completely, try to\n"
                 "establish links to the neighbours directly where possible");
      if(nk_button_label(ctx, "remove module"))
      {
        dt_node_editor_clear_selection(&nodes.nedit);
        dt_gui_dr_remove_module(sel_node_id[i]);
      }
      nk_style_pop_flags(ctx);
      nk_tree_pop(ctx);
    } // end collapsing header
    static int32_t active_module = -1;
    static char open[1000] = {0};
    render_darkroom_widgets(&vkdt.graph_dev, sel_node_id[i], open, &active_module);
  }
  if(nk_tree_push(ctx, NK_TREE_TAB, "settings", NK_MINIMIZED))
  {
    nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
    if(nk_button_label(ctx, "hotkeys"))
      dt_gui_edit_hotkeys();
    if(nodes.dual_monitor && nk_button_label(ctx, "single monitor"))
      nodes.dual_monitor = 0;
    else if(!nodes.dual_monitor && nk_button_label(ctx, "dual monitor"))
      nodes.dual_monitor = 1;
    nk_style_pop_flags(ctx);
    nk_tree_pop(ctx);
  }

  nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
  if(nk_button_label(ctx, "add module.."))
    dt_gui_dr_module_add();
  if(nk_button_label(ctx, "apply preset.."))
  {
    dt_gui_dr_preset_apply();
    nodes.do_layout = 1;           // presets may ship positions for newly added nodes
  }
  if(nk_button_label(ctx, "back to darkroom mode"))
    dt_view_switch(s_view_darkroom);
  nk_style_pop_flags(ctx);

  if(vkdt.ctx.current && vkdt.ctx.current->edit.active) vkdt.wstate.nk_active_next = 1;
  nk_end(ctx);
}

void render_nodes()
{
  struct nk_context *ctx = &vkdt.ctx;
  int num_modules = vkdt.graph_dev.num_modules;
  render_darkroom_modals(); // comes first so we can add a module popup from the context menu
  if(num_modules < vkdt.graph_dev.num_modules)
  { // module has been added
    vkdt.graph_dev.module[num_modules].gui_x = nodes.nedit.add_pos_x;
    vkdt.graph_dev.module[num_modules].gui_y = nodes.nedit.add_pos_y;
  }
  struct nk_rect bounds = { vkdt.state.center_x, vkdt.state.center_y, vkdt.state.center_wd, vkdt.state.center_ht };
  nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
  if(!nk_begin(ctx, "nodes center", bounds, NK_WINDOW_NO_SCROLLBAR))
  {
    nk_style_pop_style_item(&vkdt.ctx);
    if(vkdt.ctx.current && vkdt.ctx.current->edit.active) vkdt.wstate.nk_active_next = 1;
    nk_end(ctx);
  }
  nk_style_pop_style_item(&vkdt.ctx);

  dt_graph_t *g = &vkdt.graph_dev;

  if(nodes.do_layout)
  {
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

    int vpos = nodey;
#define TRAVERSE_POST \
    assert(cnt < sizeof(modid)/sizeof(modid[0]));\
    modid[cnt++] = curr;
#include "pipe/graph-traverse.inc"
    for(int m=cnt-1;m>=0;m--)
    { // position connected modules
      uint32_t curr = modid[m];
      pos2 = curr;
      while(mod_id[pos2] != curr) pos2 = mod_id[pos2];
      uint32_t tmp = mod_id[pos];
      mod_id[pos++] = mod_id[pos2];
      mod_id[pos2] = tmp;
      if(!g->module[curr].name) continue;
      if(g->module[curr].gui_x == 0 && g->module[curr].gui_y == 0)
      {
        if(strncmp(dt_token_str(g->module[curr].name), "i-", 2))
        {
          g->module[curr].gui_x = nodew*(m+0.25);
          g->module[curr].gui_y = nodey;
        }
        else // input nodes get their own vertical alignment
        {
          g->module[curr].gui_x = 0;
          g->module[curr].gui_y = vpos;
          vpos += nodew;
        }
      }
    }

    for(int m=pos;m<arr_cnt;m++)
    { // position disconnected modules
      if(!g->module[mod_id[m]].name) continue;
      if(g->module[mod_id[m]].gui_x == 0 && g->module[mod_id[m]].gui_y == 0)
      {
        g->module[mod_id[m]].gui_x = nodew*(m+0.25-pos);
        g->module[mod_id[m]].gui_x = 2*nodey;
      }
    }
  }

  dt_node_editor(ctx, &nodes.nedit, g);

  if(vkdt.ctx.current && vkdt.ctx.current->edit.active) vkdt.wstate.nk_active_next = 1;
  nk_end(ctx); // end center nodes view

  render_nodes_right_panel();
  bounds = nk_rect(vkdt.state.center_x+0.2*vkdt.state.center_wd, vkdt.state.center_y+0.2*vkdt.state.center_ht,
    0.6*vkdt.state.center_wd, 0.6*vkdt.state.center_ht);
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
  {
    if(nk_begin(&vkdt.ctx, "edit nodes hotkeys", bounds, NK_WINDOW_NO_SCROLLBAR))
    {
      int ok = hk_edit(hk_nodes, NK_LEN(hk_nodes));
      if(ok) vkdt.wstate.popup = 0;
    }
    else vkdt.wstate.popup = 0;
    nk_end(&vkdt.ctx);
  }
}

void render_nodes_init()
{
  hk_deserialise("nodes", hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]));
}

void render_nodes_cleanup()
{
  hk_serialise("nodes", hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]));
}

int nodes_enter()
{
  nodes.dual_monitor = 0; // XXX TODO: get from rc and write on leave
  nodes.node_hovered_link = -1;
  nodes.do_layout = 1; // maybe overwrite uninited node positions
  // make sure we process once:
  vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf;
  return 0;
}

int nodes_leave()
{
  dt_node_editor_clear_selection(&nodes.nedit); // don't leave stray selection. leads to problems re-entering with another graph.
  return 0;
}

void nodes_process()
{
  dt_gui_dr_anim_stop(); // we don't animate in graph edit mode
  if(vkdt.graph_dev.runflags)
    vkdt.graph_res = dt_graph_run(&vkdt.graph_dev,
        vkdt.graph_dev.runflags | s_graph_run_wait_done);
}

void nodes_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
}

void nodes_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
    return hk_keyboard(hk_nodes, window, key, scancode, action, mods);
  if(dt_gui_input_blocked()) return;
  if(action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
  { // escape to go back to darkroom
    dt_view_switch(s_view_darkroom);
  }
  int hotkey = action == GLFW_PRESS ? hk_get_hotkey(hk_nodes, sizeof(hk_nodes)/sizeof(hk_nodes[0]), key) : -1;
  switch(hotkey)
  {
    case s_hotkey_apply_preset:
      dt_gui_dr_preset_apply();
      nodes.do_layout = 1;           // presets may ship positions for newly added nodes
      break;
    case s_hotkey_module_add:
      dt_gui_dr_module_add();
      break;
    default:;
  }
}
