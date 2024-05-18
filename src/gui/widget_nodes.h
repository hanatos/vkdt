#pragma once
// simple node editor for dt_graph_t, based on the draft that comes with nuklear

typedef struct nk_node_connection_t
{
  int active;
  int mid;
  int cid;
}
nk_node_connection_t;

typedef struct nk_node_editor_t
{
  int inited;
  dt_module_t *selected;
  int show_grid;
  struct nk_vec2 scrolling;
  nk_node_connection_t connection;
}
nk_node_editor_t;

static inline void
nk_node_editor(
    struct nk_context *ctx,
    nk_node_editor_t  *nedit,
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

  // TODO: do this outside as customary for all our other widgets
  // if (nk_begin(ctx, "node edit", nk_rect(0, 0, 800, 600), NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE))
  /* allocate complete window space */
  canvas = nk_window_get_canvas(ctx);
  total_space = nk_window_get_content_region(ctx);
  nk_layout_space_begin(ctx, NK_STATIC, total_space.h, graph->num_modules);  // XXX what does this do?nodedit->node_count);
  struct nk_rect size = nk_layout_space_bounds(ctx);

#if 1
  // if (nedit->show_grid)
  { // render a grid
    float x, y;
    const float grid_size = 32.0f;
    const struct nk_color grid_color = nk_rgb(0, 0, 0);
    for (x = (float)fmod(size.x - nedit->scrolling.x, grid_size); x < size.w; x += grid_size)
      nk_stroke_line(canvas, x+size.x, size.y, x+size.x, size.y+size.h, 1.0f, grid_color);
    for (y = (float)fmod(size.y - nedit->scrolling.y, grid_size); y < size.h; y += grid_size)
      nk_stroke_line(canvas, size.x, y+size.y, size.x+size.w, y+size.y, 1.0f, grid_color);
  }
#endif

  const float row_height = vkdt.ctx.style.font->height + 2 * vkdt.ctx.style.tab.padding.y;

  for(int mid=-1;mid<(int)graph->num_modules;mid++)
  { // draw all modules, connected or not
    dt_module_t *module = mid < 0 ? nedit->selected : graph->module + mid;
    if(!module) continue;                               // no module selected
    if(module->name == 0) continue;                     // module previously deleted
    if(mid >= 0 && module == nedit->selected) continue; // 2nd time we iterate over this module

    struct nk_rect module_bounds = nk_rect(module->gui_x, module->gui_y, vkdt.state.center_wd * 0.1, row_height * (module->num_connectors + 3));
    nk_layout_space_push(ctx, nk_rect(module_bounds.x - nedit->scrolling.x, module_bounds.y - nedit->scrolling.y, module_bounds.w, module_bounds.h));

    const float pin_radius = 0.01*vkdt.state.center_ht;

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
          // XXX curvature and stroke thickness will need work here
          nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
              l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
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
          // XXX  dt_connector_error_str(res) and something dt_gui_notification
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

          // XXX again, stroke width and curvature need work
          nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
              p.x - 50.0f, p.y, p.x, p.y, 1.0f, nk_rgb(100, 100, 100));
        }
      }
    }
   
    char str[32];
    snprintf(str, sizeof(str), "%"PRItkn":%"PRItkn, dt_token_str(module->name), dt_token_str(module->inst));
    if(nk_group_begin(ctx, str, NK_WINDOW_MOVABLE|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
      struct nk_panel * node_panel = nk_window_get_panel(ctx);
      if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, node_panel->bounds))
        // wot??
         // (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, nk_layout_space_rect_to_screen(ctx, node_panel->bounds)))
        nedit->selected = module;

      // TODO: if module->disabled draw it somehow in different colour
      nk_layout_row_dynamic(ctx, row_height, 1);
      for(int c=0;c<module->num_connectors;c++)
      {
#if 0 // TODO: format tooltip
        // TODO: while at it extend by primaries and trc
      ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(vkdt.state.panel_wd);
      ImGui::Text("format: %" PRItkn ":%" PRItkn,
          dt_token_str(mod->connector[c].chan),
          dt_token_str(mod->connector[c].format));
      if(mod->connector[c].tooltip)
        ImGui::TextUnformatted(mod->connector[c].tooltip);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
#endif
        snprintf(str, sizeof(str), "%"PRItkn, dt_token_str(module->connector[c].name));
        nk_label(ctx, str, dt_connector_output(module->connector+c) ? NK_TEXT_RIGHT : NK_TEXT_LEFT);
      }
      nk_group_end(ctx);
      // update module position if group has been dragged
      struct nk_rect bounds;
      bounds = nk_layout_space_rect_to_local(ctx, node_panel->bounds);
      bounds.x += nedit->scrolling.x;
      bounds.y += nedit->scrolling.y;
      module->gui_x = bounds.x;
      module->gui_y = bounds.y;
    }
  } // for all modules

  if (nedit->connection.active && nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
  { // reset connection
    // TODO disconnect with history all connected inputs?
    nedit->connection.active = nk_false;
    nedit->connection.mid = -1;
    nedit->connection.cid = -1;
  }

#if 0 // i think this should already have been done, modulo coordinate space madness?
  /* node selection */
  if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, nk_layout_space_bounds(ctx)))
  {
    it = nodedit->begin;
    nodedit->selected = NULL;
    // XXX wtf is this?
    // nodedit->bounds = nk_rect(in->mouse.pos.x, in->mouse.pos.y, 100, 200);
    while (it) {
      // XXX TODO: do this inline above
      struct nk_rect b = nk_layout_space_rect_to_screen(ctx, it->bounds);
      b.x -= nodedit->scrolling.x;
      b.y -= nodedit->scrolling.y;
      if (nk_input_is_mouse_hovering_rect(in, b))
        nodedit->selected = it;
      it = it->next;
    }
  }
#endif

  // TODO: only if mouse button hasn't been handled because it's on a node
  /* contextual menu */
  // TODO: scale size of popup
  if (nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx)))
  {
    const char *grid_option[] = {"Show Grid", "Hide Grid"};
    nk_layout_row_dynamic(ctx, 25, 1);
    if (nk_contextual_item_label(ctx, "add module", NK_TEXT_CENTERED))
    {
      // XXX TODO: set add module popup id
      // TODO: and of course also put the dr mode modals at the end of render
    }
    // XXX probably also presets or something based on selected module
    // if (nk_contextual_item_label(ctx, grid_option[nodedit->show_grid],NK_TEXT_CENTERED))
      // nodedit->show_grid = !nodedit->show_grid;
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
