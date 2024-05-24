#pragma once
// simple node editor for dt_graph_t, based on the draft that comes with nuklear
#include "widget_filteredlist.h"
#include "api_gui.h"

typedef struct dt_node_connection_t
{ // temp struct for connection started by gui interaction
  int active;
  int mid;
  int cid;
}
dt_node_connection_t;

typedef struct dt_node_editor_t
{
  int inited;
  dt_module_t *selected;  // last selected module, active
  struct nk_vec2 scrolling;
  dt_node_connection_t connection;
  float add_pos_x, add_pos_y;
  uint8_t selected_mid[1000]; // flag whether the corresponding module id is selected
}
dt_node_editor_t;

// returns the number of selected nodes.
// if mid is not null, it has to be large enough to accomodate all module ids of the selection
static inline int
dt_node_editor_selection(
    dt_node_editor_t *nedit,
    dt_graph_t       *graph,
    int              *mid)
{
  int cnt = 0;
  for(int m=0;m<graph->num_modules;m++)
  {
    if(graph->module[m].name == 0) continue;
    if(m >= NK_LEN(nedit->selected_mid)) break;
    if(nedit->selected_mid[m])
    {
      if(mid) mid[cnt] = m;
      cnt++;
    }
  }
  return cnt;
}

static inline void
dt_node_editor_clear_selection(
    dt_node_editor_t *nedit)
{
  nedit->selected = 0;
  memset(nedit->selected_mid, 0, sizeof(nedit->selected_mid));
}

static inline void
dt_node_editor(
    struct nk_context *ctx,
    dt_node_editor_t  *nedit,
    dt_graph_t        *graph)
{
  struct nk_rect total_space;
  const struct nk_input *in = &ctx->input;
  struct nk_command_buffer *canvas;

  if(!nedit->inited)
  {
    memset(nedit, 0, sizeof(*nedit));
    nedit->inited = 1;
  }

  canvas = nk_window_get_canvas(ctx);
  total_space = nk_window_get_content_region(ctx);
  nk_layout_space_begin(ctx, NK_STATIC, total_space.h, graph->num_modules);
  struct nk_rect size = nk_layout_space_bounds(ctx);

  { // render a grid
    float x, y;
    const float grid_size = 32.0f;
    const struct nk_color grid_color = nk_rgb(30, 30, 30);
    for (x = (float)fmod(size.x - nedit->scrolling.x, grid_size); x < size.w; x += grid_size)
      nk_stroke_line(canvas, x+size.x, size.y, x+size.x, size.y+size.h, 1.0f, grid_color);
    for (y = (float)fmod(size.y - nedit->scrolling.y, grid_size); y < size.h; y += grid_size)
      nk_stroke_line(canvas, size.x, y+size.y, size.x+size.w, y+size.y, 1.0f, grid_color);
  }

  static struct nk_rect drag_rect;
  static float drag_x, drag_y;
  static int drag_selection = 0;
  int mouse_over_something = 0;
  const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;

  for(int mid2=0;mid2<=(int)graph->num_modules;mid2++)
  { // draw all modules, connected or not
    dt_module_t *module = mid2 == graph->num_modules ? nedit->selected : graph->module + mid2;
    if(!module) continue;                   // no module selected
    if(module->name == 0) continue;         // module previously deleted
    if(mid2 < graph->num_modules && module == nedit->selected) continue; // skip 1st time we iterate over this module
    const int mid = module - graph->module; // fix index

    struct nk_rect module_bounds = nk_rect(module->gui_x, module->gui_y, vkdt.state.center_wd * 0.1, row_height * (module->num_connectors + 3));
    struct nk_rect lbb = nk_rect(module_bounds.x - nedit->scrolling.x, module_bounds.y - nedit->scrolling.y, module_bounds.w, module_bounds.h);
    nk_layout_space_push(ctx, lbb);
    if(nk_input_is_mouse_hovering_rect(in, lbb)) mouse_over_something = 1;

    if(drag_selection)
    {
      if(lbb.x >= drag_rect.x && lbb.x+lbb.w <= drag_rect.x+drag_rect.w &&
         lbb.y >= drag_rect.y && lbb.y+lbb.h <= drag_rect.y+drag_rect.h)
      {
        if(mid < NK_LEN(nedit->selected_mid)) nedit->selected_mid[mid] = 1;
      }
      else if(mid < NK_LEN(nedit->selected_mid)) nedit->selected_mid[mid] = 0;
    }

    const float pin_radius = 0.01*vkdt.state.center_ht;
    const float link_thickness = 0.4*pin_radius;

    for(int c=0;c<module->num_connectors;c++)
    {
      if(dt_connector_output(module->connector + c))
      {
        struct nk_vec2 p = nk_layout_space_to_screen(ctx, nk_vec2(
              module_bounds.x + module_bounds.w-pin_radius - nedit->scrolling.x,
              module_bounds.y + row_height * (c+2) - nedit->scrolling.y));
        struct nk_rect circle = {
          .x = p.x,
          .y = p.y,
          .w = 2*pin_radius,
          .h = 2*pin_radius,
        };
        nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));

        if (nk_input_is_mouse_hovering_rect(in, circle)) mouse_over_something = 1;
        if (nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, circle, nk_true))
        { // start link
          nedit->connection.active = nk_true;
          nedit->connection.mid = mid;
          nedit->connection.cid = c;
        }

        if (nedit->connection.active && nedit->connection.mid == mid && nedit->connection.cid == c)
        { // draw dangling link to mouse if it belongs to us
          struct nk_vec2 l0 = nk_vec2(circle.x + pin_radius, circle.y + pin_radius);
          struct nk_vec2 l1 = in->mouse.pos;
          nk_stroke_curve(canvas, l0.x, l0.y, (l0.x + l1.x)*0.5f, l0.y,
              (l0.x + l1.x)*0.5f, l1.y, l1.x, l1.y, link_thickness, nk_rgb(100, 100, 100));
        }
      }
      else if(dt_connector_input(module->connector + c))
      {
        struct nk_vec2 p = nk_layout_space_to_screen(ctx, nk_vec2(
              module_bounds.x - pin_radius - nedit->scrolling.x,
              module_bounds.y + row_height * (c+2) - nedit->scrolling.y));
        struct nk_rect circle = {
          .x = p.x,
          .y = p.y,
          .w = 2*pin_radius,
          .h = 2*pin_radius,
        };
        if (nk_input_is_mouse_hovering_rect(in, circle)) mouse_over_something = 1;
        if (nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_RIGHT, circle, nk_true))
        { // TODO if right clicked on the connector disconnect
          dt_module_connect_with_history(graph, -1, -1, mid, c);
          vkdt.graph_dev.runflags = s_graph_run_all;
        }
        nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));
        if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
            nk_input_is_mouse_hovering_rect(in, circle) &&
            nedit->connection.active && nedit->connection.mid != mid)
        {
          nedit->connection.active = nk_false;
          // TODO: if shift held down do a feedback instead connect
          int err = dt_module_connect_with_history(graph, nedit->connection.mid, nedit->connection.cid, mid, c);
          if(err) dt_gui_notification(dt_connector_error_str(err));
          else vkdt.graph_dev.runflags = s_graph_run_all;
        }
        int mido = module->connector[c].connected_mi;
        int cido = module->connector[c].connected_mc;
        if(mido >= 0)
        { // draw link to output if we are connected
          dt_module_t *mo = graph->module + mido;
          struct nk_rect mo_bounds = nk_rect(mo->gui_x, mo->gui_y, vkdt.state.center_wd * 0.1, row_height * (mo->num_connectors + 3));
          struct nk_vec2 l0 = nk_layout_space_to_screen(ctx, nk_vec2(
              mo_bounds.x + mo_bounds.w - nedit->scrolling.x,
              mo_bounds.y + row_height * (cido+2) + pin_radius - nedit->scrolling.y));
          p.x += pin_radius;
          p.y += pin_radius;
          nk_stroke_curve(canvas, l0.x, l0.y, (l0.x + p.x)*0.5f, l0.y,
              (l0.x + p.x)*0.5f, p.y, p.x, p.y, link_thickness, nk_rgb(100, 100, 100));
        }
      }
    }
   
    char str[32];
    snprintf(str, sizeof(str), "%"PRItkn":%"PRItkn, dt_token_str(module->name), dt_token_str(module->inst));
    if((mid < NK_LEN(nedit->selected_mid)) && nedit->selected_mid[mid])
    {
      nk_style_push_style_item(ctx, &ctx->style.window.header.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT]));
      nk_style_push_style_item(ctx, &ctx->style.window.header.hover,  nk_style_item_color(vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER]));
    }
    else if(module->disabled)
    {
      nk_style_push_style_item(ctx, &ctx->style.window.header.normal, nk_style_item_color(nk_rgb(0,0,0)));
      nk_style_push_style_item(ctx, &ctx->style.window.header.hover,  nk_style_item_color(nk_rgb(0,0,0)));
    }
    else
    {
      nk_style_push_style_item(ctx, &ctx->style.window.header.normal, nk_style_item_color(vkdt.style.colour[NK_COLOR_HEADER]));
      nk_style_push_style_item(ctx, &ctx->style.window.header.hover,  nk_style_item_color(vkdt.style.colour[NK_COLOR_BUTTON_HOVER]));
    }
    // the movable thing is buggy!
    struct nk_rect bb = nk_widget_bounds(ctx);
    if(nk_group_begin(ctx, str, NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
      if(nk_input_is_mouse_hovering_rect(in, bb)) mouse_over_something = 1;
      if(nk_input_is_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, bb, nk_true))
      {
        if((mid < NK_LEN(nedit->selected_mid)) && nedit->selected_mid[mid] == 0 &&
            glfwGetKey(qvk.window, GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS &&
            glfwGetKey(qvk.window, GLFW_KEY_RIGHT_CONTROL) != GLFW_PRESS)
          dt_node_editor_clear_selection(nedit); // only clear selection if we haven't been selected before
        if(mid < NK_LEN(nedit->selected_mid)) nedit->selected_mid[mid] = 1;
        nedit->selected = module;
      }

      nk_layout_row_dynamic(ctx, row_height, 1);
      for(int c=0;c<module->num_connectors;c++)
      {
        if(module->connector[c].tooltip)
          dt_tooltip("format: %" PRItkn ":%" PRItkn "\n%s",
              dt_token_str(module->connector[c].chan),
              dt_token_str(module->connector[c].format),
              module->connector[c].tooltip);
        else
          dt_tooltip("format: %" PRItkn ":%" PRItkn,
              dt_token_str(module->connector[c].chan),
              dt_token_str(module->connector[c].format));
        snprintf(str, sizeof(str), "%"PRItkn, dt_token_str(module->connector[c].name));
        nk_label(ctx, str, dt_connector_output(module->connector+c) ? NK_TEXT_RIGHT : NK_TEXT_LEFT);
      }
      nk_group_end(ctx);
      // update module position if group has been dragged
      if(!drag_selection)
      if(!nedit->connection.active)
      if(nk_input_is_mouse_down(in, NK_BUTTON_LEFT))
      if((mid < NK_LEN(nedit->selected_mid)) && nedit->selected_mid[mid])
      {
        module->gui_x += in->mouse.delta.x;
        module->gui_y += in->mouse.delta.y;
      }
    }
    nk_style_pop_style_item(ctx);
    nk_style_pop_style_item(ctx);
  } // for all modules

  if(!nedit->connection.active)
  { // drag selection box
    if(!mouse_over_something && nk_input_is_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, total_space, nk_true))
    {
      dt_node_editor_clear_selection(nedit);
      drag_selection = 1;
      drag_x = in->mouse.pos.x;
      drag_y = in->mouse.pos.y;
    }
    if(drag_selection)
    {
      drag_rect = nk_rect(
          MIN(drag_x, in->mouse.pos.x), MIN(drag_y, in->mouse.pos.y),
          MAX(drag_x, in->mouse.pos.x), MAX(drag_y, in->mouse.pos.y));
      drag_rect.w -= drag_rect.x;
      drag_rect.h -= drag_rect.y;
      struct nk_color bg = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER];
      bg.a = 128;
      nk_fill_rect(canvas, drag_rect, 0, bg);
      nk_stroke_rect(canvas, drag_rect, 0, 0.002*vkdt.state.center_ht, vkdt.style.colour[NK_COLOR_DT_ACCENT]);
    }
    if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
    {
      drag_selection = 0;
    }
  }

  if (nedit->connection.active && nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
  { // reset connection
    // TODO disconnect with history all connected inputs?
    nedit->connection.active = nk_false;
    nedit->connection.mid = -1;
    nedit->connection.cid = -1;
  }

  nedit->add_pos_x = vkdt.state.center_x + vkdt.state.center_wd/2 + nedit->scrolling.x;
  nedit->add_pos_y = vkdt.state.center_y + vkdt.state.center_ht/2 + nedit->scrolling.y;

  // right click context menu
  // TODO: scale size of popup
  if(!mouse_over_something && nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx)))
  {
    nedit->add_pos_x = in->mouse.pos.x + nedit->scrolling.x;
    nedit->add_pos_y = in->mouse.pos.y + nedit->scrolling.y;
    nk_layout_row_dynamic(ctx, row_height, 1);
    if (nk_contextual_item_label(ctx, "add module", NK_TEXT_CENTERED))
    {
      dt_gui_dr_module_add();
    }
    // XXX probably also presets or something based on selected module
    nk_contextual_end(ctx);
  }
  nk_layout_space_end(ctx);

  if (nk_input_is_mouse_hovering_rect(in, nk_window_get_bounds(ctx)) &&
      nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
  { // scrolling
    nedit->scrolling.x -= in->mouse.delta.x;
    nedit->scrolling.y -= in->mouse.delta.y;
  }
}
