// the imgui render functions for the darkroom view
#include "gui/view.h"
#include "pipe/modules/api.h"
#include "pipe/graph-history.h"
#include "pipe/graph-defaults.h"
#include "gui/render_view.h"
#include "gui/hotkey.h"
#include "gui/keyaccel.h"
#include "gui/api_gui.h"
// #include "gui/widget_dopesheet.h"
#include "gui/widget_draw.h"
#include "gui/widget_thumbnail.h"
#include "gui/widget_image.h"

static dt_keyaccel_t keyaccel;

enum hotkey_names_t
{ // for sane access in code
  s_hotkey_create_preset   = 0,
  s_hotkey_apply_preset    = 1,
  s_hotkey_show_history    = 2,
  s_hotkey_redo            = 3,
  s_hotkey_undo            = 4,
  s_hotkey_assign_tag      = 5,
  s_hotkey_insert_keyframe = 6,
  s_hotkey_nodes_enter     = 7,
  s_hotkey_next_image      = 8,
  s_hotkey_prev_image      = 9,
  s_hotkey_fullscreen      = 10,
  s_hotkey_dopesheet       = 11,
  s_hotkey_rate_0          = 12,
  s_hotkey_rate_1          = 13,
  s_hotkey_rate_2          = 14,
  s_hotkey_rate_3          = 15,
  s_hotkey_rate_4          = 16,
  s_hotkey_rate_5          = 17,
  s_hotkey_label_1         = 18,
  s_hotkey_label_2         = 19,
  s_hotkey_label_3         = 20,
  s_hotkey_label_4         = 21,
  s_hotkey_label_5         = 22,
  s_hotkey_reload_shaders  = 23,
  s_hotkey_count           = 24,
};

static const int hk_darkroom_size = 128;
static int hk_darkroom_cnt = s_hotkey_count;
static hk_t hk_darkroom[128] = {
  {"create preset",   "create new preset from image",               {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_O}},
  {"apply preset",    "choose preset to apply",                     {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_P}},
  {"show history",    "toggle visibility of left panel",            {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_H}},
  {"redo",            "go up in history stack one item",            {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_Z}},
  {"undo",            "go down in history stack one item",          {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_Z}},
  {"assign tag",      "assign a tag to the current image",          {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_T}},
  {"insert keyframe", "insert a keyframe for the active widget",    {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_K}},
  {"node editor",     "show node editor for the current image",     {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N}},
  {"next image",      "switch to next image in folder",             {GLFW_KEY_SPACE}},
  {"prev image",      "switch to previous image in folder",         {GLFW_KEY_BACKSPACE}},
  {"fullscreen",      "show/hide side panels for fullscreen",       {GLFW_KEY_TAB}},
  {"dopesheet",       "show/hide keyframe overview",                {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_D}},
  {"rate 0",          "assign zero stars",                          {GLFW_KEY_0}},
  {"rate 1",          "assign one star",                            {GLFW_KEY_1}},
  {"rate 2",          "assign two stars",                           {GLFW_KEY_2}},
  {"rate 3",          "assign three stars",                         {GLFW_KEY_3}},
  {"rate 4",          "assign four stars",                          {GLFW_KEY_4}},
  {"rate 5",          "assign five stars",                          {GLFW_KEY_5}},
  {"label red",       "toggle red label",                           {GLFW_KEY_F1}},
  {"label green",     "toggle green label",                         {GLFW_KEY_F2}},
  {"label blue",      "toggle blue label",                          {GLFW_KEY_F3}},
  {"label yellow",    "toggle yellow label",                        {GLFW_KEY_F4}},
  {"label purple",    "toggle purple label",                        {GLFW_KEY_F5}},
  {"reload shaders",  "debug: reload shader code while running",    {}},
};

// used to communictate between the gui helper functions
static struct gui_state_data_t
{
  int hotkey;
} gui;

// goes here because the keyframe code depends on the above defines/hotkeys
// could probably pass a function pointer instead.
#include "gui/render_darkroom.h"

void
darkroom_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(vkdt.wstate.grabbed)
  {
    dt_module_input_event_t p = {
      .type     = 4,
      .key      = key,
      .scancode = scancode,
      .action   = action,
      .mods     = mods,
    };
    dt_module_t *mod = vkdt.graph_dev.module + vkdt.wstate.active_widget_modid;
    if(action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_CAPS_LOCK))
    {
      dt_gui_ungrab_mouse();
      p.type = -1; // disconnect event
      // dt_gui_dr_toggle_fullscreen_view(); // bring panels back
    }
    if(vkdt.wstate.active_widget_modid >= 0)
    {
      if(mod->so->input) mod->so->input(mod, &p);
      if(p.type == -1) vkdt.wstate.active_widget_modid = -1;
    }
    return; // grabbed, don't execute hotkeys
  }

  if(vkdt.wstate.active_widget_modid >= 0) return; // active widget grabs controls
  if(action != GLFW_PRESS) return; // only handle key down events

  gui.hotkey = -1; // XXX or if no popup is open:
  gui.hotkey = hk_get_hotkey(hk_darkroom, hk_darkroom_cnt, key);
  switch(gui.hotkey)
  { // these are "destructive" hotkeys, they change the image and invalidate the dset.
    // this has to happen this frame *before* the dset is sent to imgui for display.
    case s_hotkey_next_image:
      dt_gui_dr_next();
      break;
    case s_hotkey_prev_image:
      dt_gui_dr_prev();
      break;
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
    case s_hotkey_assign_tag:
      dt_gui_dr_assign_tag();
      break;
    case s_hotkey_nodes_enter:
      dt_view_switch(s_view_nodes);
      break;
    case s_hotkey_fullscreen:
      dt_gui_dr_toggle_fullscreen_view();
      break;
    case s_hotkey_dopesheet:
      dt_gui_dr_toggle_dopesheet();
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
    case s_hotkey_reload_shaders: dt_gui_dr_reload_shaders(); break;
    default:
     if(gui.hotkey >= s_hotkey_count && gui.hotkey < hk_darkroom_cnt) dt_keyaccel_exec(hk_darkroom[gui.hotkey].name);
     break;
  }

  // XXX only if no popup is open!
  // XXX gamepad!
  if(!dt_gui_input_blocked())
  {
    if(key == GLFW_KEY_ESCAPE)
    {
      dt_view_switch(s_view_lighttable);
      vkdt.wstate.set_nav_focus = 2; // introduce some delay because gui nav has it too
    }
  // ?? if(key == GLFW_KEY_ENTER) dt_view_switch(s_view_nodes);
  }

#if 0 // TODO: port this!
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
#endif
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
  static char filter_name[10] = {0};
  static char filter_inst[10] = {0};
#if 0 // TODO:port
  ImGui::PushItemWidth(int(vkdt.state.panel_wd * 0.495));
  ImGui::InputText("##filter name", filter_name, sizeof(filter_name));
  if(ImGui::IsItemHovered()) dt_gui_set_tooltip("filter by module name");
  ImGui::SameLine();
  ImGui::InputText("##filter instance", filter_inst, sizeof(filter_inst));
  if(ImGui::IsItemHovered()) dt_gui_set_tooltip("filter by module instance");
  ImGui::PopItemWidth();
#endif
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
    if(filter_name[0])
    {
      char name[10] = {0};
      memcpy(name, dt_token_str(vkdt.graph_dev.module[modid[m]].name), 8);
      if(!strstr(name, filter_name)) continue;
    }
    if(filter_inst[0])
    {
      char inst[10] = {0};
      memcpy(inst, dt_token_str(vkdt.graph_dev.module[modid[m]].inst), 8);
      if(!strstr(inst, filter_inst)) continue;
    }
    render_darkroom_widgets(&vkdt.graph_dev, modid[m], open, &active_module);
  }
}

void render_darkroom()
{
  // int axes_cnt = 0;
  // const float *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt)    : 0;
  int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
  int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
  struct nk_rect bounds = {win_x, win_y, win_w, win_h};
  if(!dt_gui_input_blocked() && nk_input_is_mouse_click_in_rect(&vkdt.ctx.input, NK_BUTTON_DOUBLE, bounds))
  {
    dt_view_switch(s_view_lighttable);
    vkdt.wstate.set_nav_focus = 2; // introduce some delay because gui nav has it too
    return;
  }

  const int disabled = vkdt.wstate.popup;
  if(nk_begin(&vkdt.ctx, "darkroom center", bounds, disabled ? NK_WINDOW_NO_INPUT : 0))
  { // draw center view image:
    if(disabled) nk_widget_disable_begin(&vkdt.ctx);
    dt_node_t *out_main = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
    if(out_main)
    {
#if 0 // TODO: port gamepad stuff
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
#endif
      if(vkdt.graph_res == VK_SUCCESS)
      {
        int events = !vkdt.wstate.grabbed && !disabled;
        // center view has on-canvas widgets (but only if there *is* an image):
        dt_image(&vkdt.ctx, &vkdt.wstate.img_widget, out_main, events, out_main != 0);
      }
    }
    float wd = 0.5*vkdt.style.border_frac * qvk.win_width;
    const uint32_t ci = dt_db_current_imgid(&vkdt.db);
    if(ci != -1u)
    { // this should *always* be the case
      const uint16_t labels = vkdt.db.image[ci].labels;
      const uint16_t rating = vkdt.db.image[ci].rating;
      dt_draw_rating(win_x+wd,   win_y+wd, wd, rating);
      dt_draw_labels(win_x+5*wd, win_y+wd, wd, labels);
    }

    // draw context sensitive help overlay
    if(vkdt.wstate.show_gamepadhelp) dt_gamepadhelp();
    if(vkdt.wstate.show_perf_overlay) render_perf_overlay();
    if(disabled) nk_widget_disable_end(&vkdt.ctx);
  } // end center view
  if(vkdt.ctx.current && vkdt.ctx.current->edit.active) vkdt.wstate.nk_active_next = 1;
  nk_end(&vkdt.ctx);

#if 0 // XXX port me!
  if(vkdt.wstate.dopesheet_view > 0.0f)
  { // draw dopesheet
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y + vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
    int win_w = vkdt.state.center_wd, win_h = vkdt.wstate.dopesheet_view;
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + win_x,
          ImGui::GetMainViewport()->Pos.y + win_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("darkroom dopesheet", 0, ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBackground);
    dt_dopesheet();
    ImGui::End();
    ImGui::PopStyleColor();
  }
#endif

#if 0 // XXX port history view!
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
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip("rewrite history in a compact way");
    ImGui::SameLine();
    if(ImGui::Button("roll back", ImVec2(vkdt.state.panel_wd/3.1, 0.0))) action = 2; // load previously stored cfg from disk
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip("roll back to the state when entered darkroom mode");
    ImGui::SameLine();
    if(ImGui::Button("reset",     ImVec2(vkdt.state.panel_wd/3.1, 0.0))) action = 3; // load factory defaults
    if(ImGui::IsItemHovered()) dt_gui_set_tooltip("reset everything to factory defaults");
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
        fs_realpath(graph_cfg, realimg);
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
        pop = 2;
        ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_PlotHistogramHovered]);
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
#endif

  struct nk_context *ctx = &vkdt.ctx;
  const float row_height = ctx->style.font->height + 2 * ctx->style.tab.padding.y;
  bounds = (struct nk_rect){ qvk.win_width - vkdt.state.panel_wd, 0, vkdt.state.panel_wd, vkdt.state.panel_ht };
  if(!vkdt.wstate.fullscreen_view && nk_begin(ctx, "darkroom panel right", bounds, 0))
  { // right panel
    // draw histogram image:
    dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
    if(out_hist && vkdt.graph_res == VK_SUCCESS && out_hist->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES])
    {
      int wd = vkdt.state.panel_wd;
      int ht = wd * out_hist->connector[0].roi.full_ht / (float)out_hist->connector[0].roi.full_wd; // image aspect
      nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
      struct nk_image img = nk_image_ptr(out_hist->dset[0]);
      nk_image(ctx, img);
    }

    dt_node_t *out_view0 = dt_graph_get_display(&vkdt.graph_dev, dt_token("view0"));
    if(out_view0 && vkdt.graph_res == VK_SUCCESS && out_view0->dset[vkdt.graph_dev.frame % DT_GRAPH_MAX_FRAMES])
    {
      float iwd = out_view0->connector[0].roi.wd;
      float iht = out_view0->connector[0].roi.ht;
      float scale = MIN(vkdt.state.panel_wd / iwd, 2.0f/3.0f*vkdt.state.panel_wd / iht);
      int ht = scale * iht; // wd = scale * iwd;
      nk_layout_row_dynamic(&vkdt.ctx, ht, 1);
      struct nk_image img = nk_image_ptr(out_view0->dset[0]);
      nk_image(ctx, img);
    }

    { // print some basic exif if we have
      const dt_image_params_t *ip = &vkdt.graph_dev.module[0].img_param;
      if(ip->exposure != 0.0f)
      {
        nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
        if(ip->exposure >= 1.0f)
          if(nearbyintf(ip->exposure) == ip->exposure)
            nk_labelf(ctx, NK_TEXT_LEFT, "%s %s %.0f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
                (int)ip->focal_length, (int)ip->iso);
          else
            nk_labelf(ctx, NK_TEXT_LEFT, "%s %s %.1f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
                (int)ip->focal_length, (int)ip->iso);
        /* want to catch everything below 0.3 seconds */
        else if(ip->exposure < 0.29f)
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s 1/%.0f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        /* catch 1/2, 1/3 */
        else if(nearbyintf(1.0f / ip->exposure) == 1.0f / ip->exposure)
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s 1/%.0f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        /* catch 1/1.3, 1/1.6, etc. */
        else if(10 * nearbyintf(10.0f / ip->exposure) == nearbyintf(100.0f / ip->exposure))
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s 1/%.1f f/%.1f %dmm ISO %d", ip->maker, ip->model, 1.0 / ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
        else
          nk_labelf(ctx, NK_TEXT_LEFT, "%s %s %.1f″ f/%.1f %dmm ISO %d", ip->maker, ip->model, ip->exposure, ip->aperture,
              (int)ip->focal_length, (int)ip->iso);
      }
    }
#if 0 // XXX port animation controls!
    if(vkdt.graph_dev.frame_cnt != 1 && vkdt.wstate.dopesheet_view == 0)
    { // print timeline/navigation only if not a still
      float bwd = 0.12f;
      ImVec2 size(bwd*vkdt.state.panel_wd, 0);
      if(vkdt.state.anim_playing)
      {
        if(ImGui::Button("stop", size))
          dt_gui_dr_anim_stop();
      }
      else if(ImGui::Button("play", size))
        dt_gui_dr_anim_start();
      if(ImGui::IsItemHovered())
        dt_gui_set_tooltip("play/pause the animation");
      ImGui::SameLine();
      if(ImGui::SliderInt("frame", &vkdt.state.anim_frame, 0, vkdt.state.anim_max_frame))
      {
        vkdt.graph_dev.frame = vkdt.state.anim_frame;
        vkdt.state.anim_no_keyframes = 0;  // (re-)enable keyframes
        dt_graph_apply_keyframes(&vkdt.graph_dev); // rerun once
        vkdt.graph_dev.runflags = s_graph_run_record_cmd_buf | s_graph_run_wait_done;
      }
      if(ImGui::IsItemHovered())
        dt_gui_set_tooltip("timeline navigation: set current frame.\n"
            "press space to play/pause and backspace to reset to beginning.\n"
            "hint: you can hover over many controls and press the keyframe hotkey (default ctrl-k)");
    }
#endif

    // tabs for module/params controls:
    nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 0));
    nk_style_push_float(ctx, &ctx->style.button.rounding, 0);
    nk_layout_row_begin(ctx, NK_STATIC, row_height, 3);
    const char *names[] = {"favourites", "tweak all", "esoteric"};
    static int current_tab = 0;
    for(int i = 0; i < 3; i++)
    {
      const struct nk_user_font *f = ctx->style.font;
      float text_width = f->width(f->userdata, f->height, names[i], nk_strlen(names[i]));
      float widget_width = text_width + 3 * ctx->style.button.padding.x;
      nk_layout_row_push(ctx, widget_width+5);
      if (current_tab == i) 
      {
        struct nk_style_item button_color = ctx->style.button.normal;
        ctx->style.button.normal = ctx->style.button.active;
        current_tab = nk_button_label(ctx, names[i]) ? i: current_tab;
        ctx->style.button.normal = button_color;
      } else current_tab = nk_button_label(ctx, names[i]) ? i: current_tab;
    }
    nk_style_pop_float(ctx);
    nk_style_pop_vec2(ctx);
    nk_layout_row_dynamic(ctx, 2, 1);
    nk_rule_horizontal(ctx, (struct nk_color){0x77,0x77,0x77,0xff}, nk_true);

    if(current_tab == 0)
    {
      render_darkroom_favourite();
    }
    else if(current_tab == 1)
    {
      render_darkroom_full();
      nk_layout_row_dynamic(&vkdt.ctx, row_height, 1);
      if(nk_button_label(ctx, "open node editor"))
        gui.hotkey = s_hotkey_nodes_enter;
    }
    else if(current_tab == 2)
    {
#if 0
        if(ImGui::CollapsingHeader("settings"))
        {
          ImGui::Indent();
          ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
          if(ImGui::Button("hotkeys", ImVec2(-1, 0)))
            ImGui::OpenPopup("edit hotkeys");
          ImHotKey::Edit(hk_darkroom, hk_darkroom_cnt, "edit hotkeys");
          if(ImGui::Button("toggle perf overlay", ImVec2(-1, 0)))
            vkdt.wstate.show_perf_overlay ^= 1;

          if(ImGui::SliderInt("LOD", &vkdt.wstate.lod, 1, 16, "%d"))
          { // LOD switcher
            dt_gui_set_lod(vkdt.wstate.lod);
          }
          ImGui::PopStyleVar();
          ImGui::Unindent();
        }

        if(ImGui::CollapsingHeader("animation"))
        { // animation controls
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
          if(vkdt.graph_dev.frame_cnt != 1)
          {
            if(vkdt.wstate.dopesheet_view == 0.0f && ImGui::Button("show dopesheet", ImVec2(-1, 0)))
            {
              dt_gui_dr_show_dopesheet();
            }
            else if(vkdt.wstate.dopesheet_view > 0.0f && ImGui::Button("hide dopesheet", ImVec2(-1, 0)))
            {
              dt_gui_dr_hide_dopesheet();
            }
            if(ImGui::Button("force downloading all outputs", ImVec2(-1, 0)))
            {
              vkdt.graph_dev.runflags = s_graph_run_download_sink;
            }
            if(ImGui::IsItemHovered())
              dt_gui_set_tooltip("this is useful if an animated graph has output modules\n"
                                "attached to it. for instance this allows you to trigger\n"
                                "writing of intermediate results of an optimisation from the gui.\n"
                                "only works when the animation is stopped.");
          }
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
#endif
    }
    if(vkdt.ctx.current && vkdt.ctx.current->edit.active) vkdt.wstate.nk_active_next = 1;
    nk_end(ctx);
  } // end right panel
  render_darkroom_modals();
}

void render_darkroom_init()
{
  hk_darkroom_cnt = dt_keyaccel_init(&keyaccel, hk_darkroom, hk_darkroom_cnt, hk_darkroom_size);
  hk_deserialise("darkroom", hk_darkroom, hk_darkroom_cnt);
}

void render_darkroom_cleanup()
{
  hk_serialise("darkroom", hk_darkroom, hk_darkroom_cnt);
  dt_keyaccel_cleanup(&keyaccel);
  widget_end(); // commit params if still ongoing
}
