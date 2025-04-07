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
  float zoom;             // magnifies world space in view if > 1
  float dpi_scale;        // extra scale to account for font size (moves nodes apart to leave space in between when they become larger)
  struct nk_vec2 scroll;  // world space offset
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

static inline struct nk_vec2
dt_node_world_to_view(
    dt_node_editor_t *nedit,
    struct nk_vec2 world)
{
  return nk_vec2(
      (world.x - nedit->scroll.x)*nedit->zoom,
      (world.y - nedit->scroll.y)*nedit->zoom);
}

static inline struct nk_vec2
dt_node_view_to_world(
    dt_node_editor_t *nedit,
    struct nk_vec2 view)
{
  return nk_vec2(
      view.x/nedit->zoom + nedit->scroll.x,
      view.y/nedit->zoom + nedit->scroll.y);
}

static inline int
dt_node_editor_context_menu(
    struct nk_context *ctx,
    dt_node_editor_t  *nedit)
{
  const struct nk_input *in = &ctx->input;
  struct nk_rect size = nk_layout_space_bounds(ctx);
  int ret = 0;
  // TODO: scale size of popup
  if(nk_contextual_begin(ctx, 0, nk_vec2(vkdt.state.panel_wd, 220), nk_window_get_bounds(ctx)))
  { // right click over node/selection to open context menu
    int sel_node_cnt = dt_node_editor_selection(nedit, &vkdt.graph_dev, 0);
    int *sel_node_id  = (int *)alloca(sizeof(int)*sel_node_cnt);
    dt_node_editor_selection(nedit, &vkdt.graph_dev, sel_node_id);
    const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
    if(sel_node_cnt)
    {
      nk_layout_row_dynamic(ctx, row_height, 1);
      if(nk_contextual_item_label(ctx, "ab compare selection", NK_TEXT_LEFT))
      {
        vkdt.graph_dev.runflags = s_graph_run_all;
        for(int i=0;i<sel_node_cnt;i++)
        {
          int m = sel_node_id[i];
          int modid = dt_module_add(&vkdt.graph_dev, vkdt.graph_dev.module[m].name, vkdt.graph_dev.module[m].inst+1);
          if(modid >= 0)
          {
            dt_graph_history_module(&vkdt.graph_dev, modid);
            vkdt.graph_dev.module[modid].gui_x = vkdt.graph_dev.module[m].gui_x;
            vkdt.graph_dev.module[modid].gui_y = vkdt.graph_dev.module[m].gui_y + row_height * 10;
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
                    vkdt.graph_dev.module[mab].gui_x = vkdt.graph_dev.module[nm[k]].gui_x - row_height * 8;
                    vkdt.graph_dev.module[mab].gui_y = vkdt.graph_dev.module[nm[k]].gui_y;
                  }
                }
              }
            }
          }
        }
        ret = 1;
      }
      dt_tooltip("disconnect all connectors of selected modules, try to\n"
                 "establish links to the neighbours directly where possible");
      if(nk_contextual_item_label(ctx, "disconnect selected module", NK_TEXT_LEFT))
      {
        dt_node_editor_clear_selection(nedit);
        for(int i=0;i<sel_node_cnt;i++)
          dt_gui_dr_disconnect_module(sel_node_id[i]);
        sel_node_cnt = 0;
        ret = 1;
      }
      dt_tooltip("remove selected modules from the graph completely, try to\n"
                 "establish links to the neighbours directly where possible");
      if(nk_contextual_item_label(ctx, "remove selected modules", NK_TEXT_LEFT))
      {
        dt_node_editor_clear_selection(nedit);
        for(int i=0;i<sel_node_cnt;i++)
          dt_gui_dr_remove_module(sel_node_id[i]);
        sel_node_cnt = 0;
        ret = 1;
      }
    }
    if(sel_node_cnt)
    {
      const int i = sel_node_cnt-1;
      dt_module_t *mod = vkdt.graph_dev.module + sel_node_id[i];
      char name[100];
      if(mod->name != 0) // skip deleted
      { // expander for individual module
        nk_style_push_flags(&vkdt.ctx, &vkdt.ctx.style.button.text_alignment, NK_TEXT_LEFT);
        nk_layout_row_dynamic(&vkdt.ctx, 0, 1);
        if(mod->so->has_inout_chain)
          dt_tooltip(mod->disabled ? "re-enable this module" :
              "temporarily disable this module without disconnecting it from the graph.\n"
              "this is just a convenience A/B switch in the ui and will not affect your\n"
              "processing history, lighttable thumbnail, or export.");
        if(mod->so->has_inout_chain && !mod->disabled)
        {
          snprintf(name, sizeof(name), "temporarily disable %" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
          if(nk_contextual_item_label(ctx, name, NK_TEXT_LEFT))
          {
            mod->disabled = 1;
            vkdt.graph_dev.runflags = s_graph_run_all;
            ret = 1;
          }
        }
        else if(mod->so->has_inout_chain && mod->disabled)
        {
          snprintf(name, sizeof(name), "re-enable %" PRItkn " %" PRItkn, dt_token_str(mod->name), dt_token_str(mod->inst));
          if(nk_contextual_item_label(ctx, name, NK_TEXT_LEFT))
          {
            mod->disabled = 0;
            vkdt.graph_dev.runflags = s_graph_run_all;
            ret = 1;
          }
        }
        nk_style_pop_flags(ctx);
      }
    }

    if(!sel_node_cnt)
    {
      struct nk_vec2 pos = dt_node_view_to_world(nedit, nk_vec2(in->mouse.pos.x - size.x, in->mouse.pos.y - size.y));
      nedit->add_pos_x = pos.x / nedit->dpi_scale;
      nedit->add_pos_y = pos.y / nedit->dpi_scale;
      nk_layout_row_dynamic(ctx, row_height, 1);
      if (nk_contextual_item_label(ctx, "add module..", NK_TEXT_LEFT))
      {
        dt_gui_dr_module_add();
        ret = 1;
      }
      // XXX probably also presets or something based on selected module
    }
    nk_contextual_end(ctx);
  }
  return ret;
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
    nedit->zoom = 1.0f;
    nedit->inited = 1;
    float scalex, scaley;
    dt_gui_content_scale(vkdt.win.window, &scalex, &scaley);
    const float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
    nedit->dpi_scale = CLAMP(scalex * dpi_scale, 0.01f, 10.0f);
  }

  canvas = nk_window_get_canvas(ctx);
  total_space = nk_window_get_content_region(ctx);
  nk_layout_space_begin(ctx, NK_STATIC, total_space.h, graph->num_modules);
  struct nk_rect size = nk_layout_space_bounds(ctx);

  static int clicked_circle = 0;
  // right click context menues:
  int context_menu_clicked = 0;
  if(!clicked_circle) // don't open context menu if we right clicked on a circle/disconnect before. this comes with one frame delay.
    context_menu_clicked = dt_node_editor_context_menu(ctx, nedit);
  clicked_circle = 0;

  if(nedit->zoom > 0.3)
  { // render a grid
    float x, y;
    const float grid_size_ws = 32.0f;
    const struct nk_vec2 size_ws = dt_node_view_to_world(nedit, nk_vec2(size.x,size.y));
    struct nk_vec2 off_ws = nk_vec2(-fmod(size_ws.x, grid_size_ws), -fmod(size_ws.y, grid_size_ws));
    if(off_ws.x < 0) off_ws.x += grid_size_ws;
    if(off_ws.y < 0) off_ws.y += grid_size_ws;
    const struct nk_vec2 off_vs = nk_vec2(off_ws.x * nedit->zoom, off_ws.y * nedit->zoom);
    const struct nk_color grid_color = nk_rgb(30, 30, 30);
    for (x = off_vs.x; x < size.w; x += nedit->zoom * grid_size_ws)
      nk_stroke_line(canvas, x+size.x, size.y, x+size.x, size.y+size.h, 1.0f, grid_color);
    for (y = off_vs.y; y < size.h; y += nedit->zoom * grid_size_ws)
      nk_stroke_line(canvas, size.x, y+size.y, size.x+size.w, y+size.y, 1.0f, grid_color);
  }
  // set font size to something scaled by zoom:
  static struct nk_user_font font;
  font = *nk_glfw3_font(0);
  font.height *= nedit->zoom;
  nk_style_push_font(ctx, &font);
  nk_style_push_vec2(ctx, &vkdt.ctx.style.tab.padding, nk_vec2(vkdt.ctx.style.tab.padding.x*nedit->zoom, vkdt.ctx.style.tab.padding.y*nedit->zoom));
  nk_style_push_vec2(ctx, &vkdt.ctx.style.window.header.padding, nk_vec2(vkdt.ctx.style.window.header.padding.x*nedit->zoom, vkdt.ctx.style.window.header.padding.y*nedit->zoom));
  nk_style_push_vec2(ctx, &vkdt.ctx.style.window.header.spacing, nk_vec2(vkdt.ctx.style.window.header.spacing.x*nedit->zoom, vkdt.ctx.style.window.header.spacing.y*nedit->zoom));
  nk_style_push_vec2(ctx, &vkdt.ctx.style.window.padding, nk_vec2(vkdt.ctx.style.window.padding.x*nedit->zoom, vkdt.ctx.style.window.padding.y*nedit->zoom));
  nk_style_push_vec2(ctx, &vkdt.ctx.style.window.spacing, nk_vec2(vkdt.ctx.style.window.spacing.x*nedit->zoom, vkdt.ctx.style.window.spacing.y*nedit->zoom));

  // row height is view space, i.e. already scaled for display
  float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;
  float lineskip = row_height + vkdt.ctx.style.window.spacing.y;
  float header_offset = 1.5*lineskip; // whatever the fuck. i will not find out what it is.

  const struct nk_color col_feedback = nk_rgb(200, 30, 200);
  static struct nk_rect selected_rect;
  static struct nk_rect drag_rect;
  static float drag_x, drag_y;
  static int drag_selection = 0; // drag the selection box
  static int move_nodes = 0;     // move selected nodes with the mouse
  int mouse_over_something = 0;
  const int disabled = context_menu_clicked | dt_gui_input_blocked(); // io is going to popup window or something

  for(int mid2=0;mid2<=(int)graph->num_modules;mid2++)
  { // draw all modules, connected or not
    dt_module_t *module = mid2 == graph->num_modules ? nedit->selected : graph->module + mid2;
    if(!module) continue;                   // no module selected
    if(module->name == 0) continue;         // module previously deleted
    if(mid2 < graph->num_modules && module == nedit->selected) continue; // skip 1st time we iterate over this module
    const int mid = module - graph->module; // fix index

    const float mod_wd = 7*nk_glfw3_font(0)->height;
    struct nk_rect module_bounds = nk_rect(nedit->dpi_scale*module->gui_x, nedit->dpi_scale*module->gui_y, mod_wd, 0);
    struct nk_vec2 lbb_min = dt_node_world_to_view(nedit, nk_vec2(nedit->dpi_scale*module->gui_x, nedit->dpi_scale*module->gui_y));
    struct nk_vec2 lbb_max = dt_node_world_to_view(nedit, nk_vec2(nedit->dpi_scale*module->gui_x + module_bounds.w, nedit->dpi_scale*module->gui_y));
    struct nk_rect lbb = nk_rect(lbb_min.x, lbb_min.y, lbb_max.x-lbb_min.x, row_height * (module->num_connectors + 3));
    nk_layout_space_push(ctx, lbb);
    if(nk_input_is_mouse_hovering_rect(in, lbb)) mouse_over_something = 1;

    if(drag_selection)
    {
      if(size.x + lbb.x >= drag_rect.x && size.x + lbb.x+lbb.w <= drag_rect.x+drag_rect.w &&
         size.y + lbb.y >= drag_rect.y && size.y + lbb.y+lbb.h <= drag_rect.y+drag_rect.h)
      {
        if(mid < NK_LEN(nedit->selected_mid)) nedit->selected_mid[mid] = 1;
      }
      else if(mid < NK_LEN(nedit->selected_mid)) nedit->selected_mid[mid] = 0;
    }

    const float pin_radius = 0.3*row_height;
    const float link_thickness = 0.35*pin_radius;

    for(int c=0;c<module->num_connectors;c++)
    {
      if(dt_connector_output(module->connector + c))
      {
        struct nk_vec2 p = nk_layout_space_to_screen(ctx,
            dt_node_world_to_view(nedit,
            nk_vec2(
              module_bounds.x + module_bounds.w,
              module_bounds.y)));
        struct nk_rect circle = {
          .x = p.x - pin_radius,
          .y = p.y + lineskip * c + header_offset,
          .w = 2*pin_radius,
          .h = 2*pin_radius,
        };
        const int hovering_circle = nk_input_is_mouse_hovering_rect(in, circle);
        nk_fill_circle(canvas, circle, hovering_circle ? vkdt.style.colour[NK_COLOR_DT_ACCENT] : nk_rgb(100, 100, 100));

        if (!disabled && hovering_circle) mouse_over_something = 1;
        if (!disabled && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, circle, nk_true))
        { // start link
          nedit->connection.active = nk_true;
          nedit->connection.mid = mid;
          nedit->connection.cid = c;
        }

        if (nedit->connection.active && nedit->connection.mid == mid && nedit->connection.cid == c)
        { // draw dangling link to mouse if it belongs to us
          struct nk_vec2 l0 = nk_vec2(circle.x + pin_radius, circle.y + pin_radius);
          struct nk_vec2 l1 = in->mouse.pos;
          struct nk_color col = nk_rgb(100,100,100);
          if(glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
             glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
            col = col_feedback;
          nk_stroke_curve(canvas, l0.x, l0.y, (l0.x + l1.x)*0.5f, l0.y,
              (l0.x + l1.x)*0.5f, l1.y, l1.x, l1.y, link_thickness, col);
        }
      }
      else if(dt_connector_input(module->connector + c))
      {
        struct nk_vec2 p = nk_layout_space_to_screen(ctx, 
            dt_node_world_to_view(nedit,
              nk_vec2(
                module_bounds.x,
                module_bounds.y)));
        struct nk_rect circle = {
          .x = p.x - pin_radius,
          .y = p.y + lineskip * c + header_offset,
          .w = 2*pin_radius,
          .h = 2*pin_radius,
        };
        const int hovering_circle = nk_input_is_mouse_hovering_rect(in, circle);
        if (!disabled && hovering_circle) mouse_over_something = 2;
        if (!disabled &&
            nk_input_has_mouse_click(in, NK_BUTTON_RIGHT) &&
            nk_input_has_mouse_click_in_rect(in, NK_BUTTON_RIGHT, circle))
        { // if right clicked on the connector disconnect
          clicked_circle = 1;
          dt_module_connect_with_history(graph, -1, -1, mid, c);
          vkdt.graph_dev.runflags = s_graph_run_all;
        }
        nk_fill_circle(canvas, circle, hovering_circle ? vkdt.style.colour[NK_COLOR_DT_ACCENT] : nk_rgb(100, 100, 100));
        const int feedback = glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS || glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        if (!disabled && nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
            nk_input_is_mouse_hovering_rect(in, circle) &&
            nedit->connection.active && (feedback || nedit->connection.mid != mid))
        {
          nedit->connection.active = nk_false;
          int err = 0;
          if(feedback)
            err = dt_module_feedback_with_history(graph, nedit->connection.mid, nedit->connection.cid, mid, c);
          else
            err = dt_module_connect_with_history(graph, nedit->connection.mid, nedit->connection.cid, mid, c);
          if(err) dt_gui_notification(dt_connector_error_str(err));
          else vkdt.graph_dev.runflags = s_graph_run_all;
        }
        int mido = module->connector[c].connected_mi;
        int cido = module->connector[c].connected_mc;
        if(mido >= 0)
        { // draw link to output if we are connected
          dt_module_t *mo = graph->module + mido;
          const float mod_wd = 7*nk_glfw3_font(0)->height;
          struct nk_rect mo_bounds = nk_rect(nedit->dpi_scale*mo->gui_x, nedit->dpi_scale*mo->gui_y, mod_wd, row_height * (mo->num_connectors + 3));
          struct nk_vec2 l0 = nk_layout_space_to_screen(ctx,
              dt_node_world_to_view(nedit,
                nk_vec2(
                  mo_bounds.x + mo_bounds.w,
                  mo_bounds.y)));
          l0.y += lineskip * cido + header_offset + pin_radius;
          p.x += pin_radius;
          p.y += lineskip * c + header_offset + pin_radius;
          struct nk_color col = nk_rgb(100,100,100);
          if(module->connector[c].flags & s_conn_feedback)
            col = col_feedback;
          if(!disabled && nedit->selected &&
             !drag_selection && !nedit->connection.active &&
              MAX(l0.x, p.x) >= selected_rect.x && MIN(l0.x, p.x) <= selected_rect.x + selected_rect.w && 
              MAX(l0.y, p.y) >= selected_rect.y && MIN(l0.y, p.y) <= selected_rect.y + selected_rect.h)
          { // selected / active node overlaps here and is dropped connect it in between!
            if(nedit->selected->so->has_inout_chain)
            { // if selected module is disconnected and has inout chain
              int mco = dt_module_get_connector(nedit->selected, dt_token("output"));
              int mci = dt_module_get_connector(nedit->selected, dt_token("input"));
              if(mco >= 0 && mci >= 0 &&
                  !dt_connected(nedit->selected->connector+mco) &&
                  !dt_connected(nedit->selected->connector+mci))
              {
                if(nk_input_is_mouse_down(in, NK_BUTTON_LEFT))
                  col = vkdt.style.colour[NK_COLOR_DT_ACCENT];
                if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
                {
                  dt_module_connect_with_history(&vkdt.graph_dev, mido, cido, nedit->selected - vkdt.graph_dev.module, mci);
                  dt_module_connect_with_history(&vkdt.graph_dev, nedit->selected - vkdt.graph_dev.module, mco, mid, c);
                  vkdt.graph_dev.runflags = s_graph_run_all;
                }
              }
            }
          }
          if(module->connector[c].flags & s_conn_feedback)
            nk_stroke_curve(canvas, l0.x, l0.y,
                l0.x+5*pin_radius*nedit->zoom, l0.y+10*pin_radius*nedit->zoom,
                p.x -5*pin_radius*nedit->zoom, p.y +10*pin_radius*nedit->zoom,
                p.x, p.y, link_thickness, col);
          else
            nk_stroke_curve(canvas, l0.x, l0.y, (l0.x + p.x)*0.5f, l0.y,
                (l0.x + p.x)*0.5f, p.y, p.x, p.y, link_thickness, col);
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
    struct nk_rect bb = nk_widget_bounds(ctx);
    if(nk_group_begin(ctx, str, NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
      if(!disabled && nk_input_is_mouse_hovering_rect(in, bb)) mouse_over_something = 1;
      if(!disabled && nk_input_is_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, bb, nk_true))
      {
        if((mid < NK_LEN(nedit->selected_mid)) && nedit->selected_mid[mid] == 0 &&
            glfwGetKey(vkdt.win.window, GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS &&
            glfwGetKey(vkdt.win.window, GLFW_KEY_RIGHT_CONTROL) != GLFW_PRESS)
          dt_node_editor_clear_selection(nedit); // only clear selection if we haven't been selected before
        if(mid < NK_LEN(nedit->selected_mid)) nedit->selected_mid[mid] = 1;
        nedit->selected = module;
        move_nodes = 1;
        vkdt.graph_dev.active_module = -1;
      }
      if(nedit->selected == module)
        selected_rect = bb; // remember for connector overlap test

      nk_layout_row_dynamic(ctx, row_height, 1);
      for(int c=0;c<module->num_connectors;c++)
      {
        nk_style_push_font(ctx, nk_glfw3_font(0));
        dt_tooltip("format: %" PRItkn ":%" PRItkn "\n%s%s\n%d x %d %s",
            dt_token_str(module->connector[c].chan),
            dt_token_str(module->connector[c].format),
            module->connector[c].tooltip && module->connector[c].tooltip[0] ? module->connector[c].tooltip : "",
            dt_connector_output(module->connector+c) ? "": "\nright click on circle to delete link",
            module->connector[c].roi.wd,
            module->connector[c].roi.ht,
            module->connector[c].associated_i >= 0 &&
            graph->node[module->connector[c].associated_i].connector[module->connector[c].associated_c].frames > 1 ?
            "double buffered" : "");
        snprintf(str, sizeof(str), "%"PRItkn, dt_token_str(module->connector[c].name));
        nk_style_pop_font(ctx);
        nk_label(ctx, str, dt_connector_output(module->connector+c) ? NK_TEXT_RIGHT : NK_TEXT_LEFT);
      }
      nk_group_end(ctx);
      // update module position if group has been dragged
      // TODO: do this before draw to minimise lag
      if(!disabled && move_nodes && !drag_selection && !nedit->connection.active &&
          nk_input_is_mouse_down(in, NK_BUTTON_LEFT) &&
          (mid < NK_LEN(nedit->selected_mid)) && nedit->selected_mid[mid])
      {
        module->gui_x += in->mouse.delta.x/nedit->zoom/nedit->dpi_scale;
        module->gui_y += in->mouse.delta.y/nedit->zoom/nedit->dpi_scale;
      }
    }
    nk_style_pop_style_item(ctx);
    nk_style_pop_style_item(ctx);
  } // for all modules

  if(!disabled && !nedit->connection.active)
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
  }

  if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
  {
    drag_selection = 0;
    move_nodes = 0;
    nedit->connection.active = nk_false;
    nedit->connection.mid = -1;
    nedit->connection.cid = -1;
  }

  struct nk_vec2 pos = dt_node_view_to_world(nedit, nk_vec2(
        size.x + size.w/2,
        size.y + size.h/2));
  nedit->add_pos_x = pos.x/nedit->dpi_scale;
  nedit->add_pos_y = pos.y/nedit->dpi_scale;

  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_font(ctx);
  row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;

  nk_layout_space_end(ctx);

  if (!disabled &&
      nk_input_is_mouse_hovering_rect(in, nk_window_get_bounds(ctx)) &&
      nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
  { // panning by middle mouse drag
    nedit->scroll.x -= in->mouse.delta.x/nedit->zoom;
    nedit->scroll.y -= in->mouse.delta.y/nedit->zoom;
  }
}
