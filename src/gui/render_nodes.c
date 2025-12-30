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
#include "widget_resize_panel.h"
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
  dt_node_editor_t nedit;
}
gui_nodes_t;
gui_nodes_t nodes;

void render_nodes_right_panel()
{
  const int disabled = vkdt.wstate.popup;
  struct nk_context *ctx = &vkdt.ctx;
  struct nk_rect bounds = { vkdt.win.width - vkdt.state.panel_wd, 0, vkdt.state.panel_wd, vkdt.state.panel_ht };
  if(!nk_begin(ctx, "nodes panel", bounds, disabled ? NK_WINDOW_NO_INPUT : 0))
  {
    NK_UPDATE_ACTIVE;
    nk_end(ctx);
  }
  const int display_frame = vkdt.graph_dev.double_buffer % 2;
  dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
  if(out_hist && vkdt.graph_res[display_frame] == VK_SUCCESS && out_hist->dset[display_frame])
  {
    int wd = vkdt.state.panel_wd;
    int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
    nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
    struct nk_image img = nk_image_ptr(out_hist->dset[display_frame]);
    nk_image(ctx, img);
  }
  static dt_image_widget_t imgw[] = {
    { .look_at_x = 0, .look_at_y = 0, .scale=1.0 },
    { .look_at_x = 0, .look_at_y = 0, .scale=1.0 },
    { .look_at_x = 0, .look_at_y = 0, .scale=1.0 }};
  dt_token_t dsp[] = { dt_token("main"), dt_token("view0"), dt_token("view1") };
  for(uint32_t d = 0; d < sizeof(dsp)/sizeof(dsp[0]); d++)
  {
    dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dsp[d]);
    if(out && vkdt.graph_res[display_frame] == VK_SUCCESS)
    {
      const int popout = (dsp[d] == dt_token("main")) && vkdt.win1.window;
      char title[20] = {0};
      snprintf(title, sizeof(title), "nodes %" PRItkn, dt_token_str(dsp[d]));
      struct nk_context *ctx = &vkdt.ctx;
      int wd = vkdt.state.panel_wd;
      int visible = 1;
      if(popout)
      {
        ctx = &vkdt.ctx1;
        struct nk_rect bounds = {0, 0, vkdt.win1.width, vkdt.win1.height};
        wd = vkdt.win1.width;
        nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
        if(!nk_begin(ctx, "vkdt secondary", bounds, NK_WINDOW_NO_SCROLLBAR | (disabled ? NK_WINDOW_NO_INPUT : 0)))
          visible = 0;
      }
      if(visible)
      {
        int ht = wd * out->connector[0].roi.full_ht / (float)out->connector[0].roi.full_wd; // image aspect
        nk_layout_row_dynamic(ctx, ht, 1);
        if(dsp[d] == dt_token("main"))
          dt_image(ctx, &vkdt.wstate.img_widget, out, 1, popout ? 0 : 1);
        else
          dt_image(ctx, imgw+d, out, 1, 0);
      }
      if(popout)
      {
        // NK_UPDATE_ACTIVE; // does not work on ctx1
        nk_end(ctx);
        nk_style_pop_style_item(ctx);
      }
    }
  }
  // expanders for selection and individual nodes:
  int sel_node_cnt = dt_node_editor_selection(&nodes.nedit, &vkdt.graph_dev, 0);
  int *sel_node_id  = (int *)alloca(sizeof(int)*sel_node_cnt);
  dt_node_editor_selection(&nodes.nedit, &vkdt.graph_dev, sel_node_id);
  for(int i=0;i<sel_node_cnt;i++)
  {
    render_darkroom_widgets(&vkdt.graph_dev, sel_node_id[i]);
  }
  if(nk_tree_push(ctx, NK_TREE_TAB, "settings", NK_MINIMIZED))
  {
    nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
    if(nk_button_label(ctx, "hotkeys"))
      dt_gui_edit_hotkeys();
    if(vkdt.win1.window && nk_button_label(ctx, "single monitor"))
      dt_gui_win1_close();
    else if(!vkdt.win1.window && nk_button_label(ctx, "dual monitor"))
      dt_gui_win1_open();
    nk_style_pop_flags(ctx);
    nk_tree_pop(ctx);
  }

  nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
  if(nk_button_label(ctx, "add module.."))
    dt_gui_dr_module_add();
  if(nk_button_label(ctx, "apply preset.."))
  {
    dt_gui_preset_apply();
    nodes.do_layout = 1;           // presets may ship positions for newly added nodes
  }
  if(nk_button_label(ctx, "back to darkroom mode"))
    dt_view_switch(s_view_darkroom);
  nk_style_pop_flags(ctx);

  NK_UPDATE_ACTIVE;
  nk_end(ctx);
}

void render_nodes()
{
  static int resize_panel = 0;
  resize_panel = dt_resize_panel(resize_panel);

  const int disabled = vkdt.wstate.popup;
  struct nk_context *ctx = &vkdt.ctx;
  int num_modules = vkdt.graph_dev.num_modules;
  render_darkroom_modals(); // comes first so we can add a module popup from the context menu
  if(num_modules < vkdt.graph_dev.num_modules)
  { // module has been added, move and select it. does not work if a module reclaims the slot of a previously deleted one. do we care?
    vkdt.graph_dev.module[num_modules].gui_x = nodes.nedit.add_pos_x;
    vkdt.graph_dev.module[num_modules].gui_y = nodes.nedit.add_pos_y;
    dt_node_editor_clear_selection(&nodes.nedit);
    if(num_modules < NK_LEN(nodes.nedit.selected_mid)) nodes.nedit.selected_mid[num_modules] = 1;
    nodes.nedit.selected = vkdt.graph_dev.module + num_modules;
  }
  struct nk_rect bounds = { vkdt.state.center_x, vkdt.state.center_y, vkdt.state.center_wd, vkdt.state.center_ht };
  nk_style_push_style_item(&vkdt.ctx, &vkdt.ctx.style.window.fixed_background, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_BACKGROUND]));
  if(!nk_begin(ctx, "nodes center", bounds, NK_WINDOW_NO_SCROLLBAR | (disabled ? NK_WINDOW_NO_INPUT : 0)))
  {
    nk_style_pop_style_item(&vkdt.ctx);
    NK_UPDATE_ACTIVE;
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

    float scalex, scaley;
    dt_gui_content_scale(vkdt.win.window, &scalex, &scaley);
    const float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
    const float nodew = 10*nk_glfw3_font(0)->height/(scaley*dpi_scale);
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

  NK_UPDATE_ACTIVE;
  nk_end(ctx); // end center nodes view

  render_nodes_right_panel();
  bounds = nk_rect(vkdt.state.center_x+0.2*vkdt.state.center_wd, vkdt.state.center_y+0.2*vkdt.state.center_ht,
    0.6*vkdt.state.center_wd, 0.6*vkdt.state.center_ht);
  if(vkdt.wstate.popup == s_popup_edit_hotkeys)
  {
    if(nk_begin(&vkdt.ctx, "edit nodes hotkeys", bounds, NK_WINDOW_NO_SCROLLBAR | (disabled ? NK_WINDOW_NO_INPUT : 0)))
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
  darkroom_process();    // the rest stays the same though.
}

void nodes_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
  // XXX this is not a good idea because middle mouse means pan
  // if(button == GLFW_MOUSE_BUTTON_MIDDLE) nodes.nedit.zoom = 1;
}

void nodes_mouse_scrolled(GLFWwindow *window, double xoff, double yoff)
{
  if (nk_input_is_mouse_hovering_rect(&vkdt.ctx.input, nk_rect(vkdt.state.center_x, vkdt.state.center_y, vkdt.state.center_wd, vkdt.state.center_ht)))
  {
    double mx, my;
    dt_view_get_cursor_pos(window, &mx, &my);
    const struct nk_vec2 mouse = nk_vec2(mx-vkdt.state.center_x, my-vkdt.state.center_y);
    struct nk_vec2 center_ws = dt_node_view_to_world(&nodes.nedit, mouse);

    nodes.nedit.zoom *= powf(1.2f, yoff);
    nodes.nedit.zoom = CLAMP(nodes.nedit.zoom, 0.1, 10.0);

    struct nk_vec2 center2_ws = dt_node_view_to_world(&nodes.nedit, mouse);
    nodes.nedit.scroll.x += center_ws.x - center2_ws.x;
    nodes.nedit.scroll.y += center_ws.y - center2_ws.y;
  }
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
      dt_gui_preset_apply();
      nodes.do_layout = 1;           // presets may ship positions for newly added nodes
      break;
    case s_hotkey_module_add:
      dt_gui_dr_module_add();
      break;
    default:;
  }
}
