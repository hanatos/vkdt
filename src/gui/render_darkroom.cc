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
#include "gui/keyaccel.hh"
#include "gui/api.hh"
#include "gui/widget_dopesheet.hh"
#include "gui/widget_draw.hh"
#include "gui/widget_thumbnail.hh"
#include "gui/widget_image.hh"

namespace { // anonymous namespace

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
static ImHotKey::HotKey hk_darkroom[hk_darkroom_size] = {
  {"create preset",   "create new preset from image",               {ImGuiKey_LeftCtrl, ImGuiKey_O}},
  {"apply preset",    "choose preset to apply",                     {ImGuiKey_LeftCtrl, ImGuiKey_P}},
  {"show history",    "toggle visibility of left panel",            {ImGuiKey_LeftCtrl, ImGuiKey_H}},
  {"redo",            "go up in history stack one item",            {ImGuiKey_LeftCtrl, ImGuiKey_LeftShift, ImGuiKey_Z}}, // test before undo
  {"undo",            "go down in history stack one item",          {ImGuiKey_LeftCtrl, ImGuiKey_Z}},
  {"assign tag",      "assign a tag to the current image",          {ImGuiKey_LeftCtrl, ImGuiKey_T}},
  {"insert keyframe", "insert a keyframe for the active widget",    {ImGuiKey_LeftCtrl, ImGuiKey_K}},
  {"node editor",     "show node editor for the current image",     {ImGuiKey_LeftCtrl, ImGuiKey_N}},
  {"next image",      "switch to next image in folder",             {ImGuiKey_Space}},
  {"prev image",      "switch to previous image in folder",         {ImGuiKey_Backspace}},
  {"fullscreen",      "show/hide side panels for fullscreen",       {ImGuiKey_Tab}},
  {"dopesheet",       "show/hide keyframe overview",                {ImGuiKey_LeftCtrl, ImGuiKey_D}},
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
  {"reload shaders",  "debug: reload shader code while running",    {}},
};

// used to communictate between the gui helper functions
static struct gui_state_data_t
{
  int hotkey = -1;
} gui;

// goes here because the keyframe code depends on the above defines/hotkeys
// could probably pass a function pointer instead.
#include "gui/render_darkroom.hh"

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
} // end anonymous namespace


void render_darkroom()
{
  gui.hotkey = -1;
  if(!ImGui::IsPopupOpen("edit hotkeys", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    gui.hotkey = ImHotKey::GetHotKey(hk_darkroom, hk_darkroom_cnt);
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
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht - vkdt.wstate.dopesheet_view;
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
        dt_draw_rating(ImGui::GetMainViewport()->Pos.x + win_x-wd,   ImGui::GetMainViewport()->Pos.y + win_y-wd, wd, rating);
        dt_draw_labels(ImGui::GetMainViewport()->Pos.x + win_x+4*wd, ImGui::GetMainViewport()->Pos.y + win_y-wd, wd, labels);
      }
      ImGui::End();
    }
    ImGui::SetNextWindowPos (ImVec2(
          ImGui::GetMainViewport()->Pos.x + win_x,
          ImGui::GetMainViewport()->Pos.y + win_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);
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
    if(vkdt.wstate.show_perf_overlay) render_perf_overlay();

    ImGui::End();
    ImGui::PopStyleColor();
  } // end center view

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
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
        if(ImGui::Button("open node editor", ImVec2(-1, 0)))
          gui.hotkey = s_hotkey_nodes_enter;
        ImGui::PopStyleVar();
        ImGui::EndTabItem();
      }
      if(ImGui::BeginTabItem("esoteric"))
      {
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
     if(gui.hotkey >= s_hotkey_count && gui.hotkey < hk_darkroom_cnt) dt_keyaccel_exec(hk_darkroom[gui.hotkey].functionName);
     break;
  }
  dt_gui_dr_modals(); // draw modal windows for presets etc
}

void render_darkroom_init()
{
  hk_darkroom_cnt = dt_keyaccel_init(&keyaccel, hk_darkroom, hk_darkroom_cnt, hk_darkroom_size);
  ImHotKey::Deserialise("darkroom", hk_darkroom, hk_darkroom_cnt);
}

void render_darkroom_cleanup()
{
  ImHotKey::Serialise("darkroom", hk_darkroom, hk_darkroom_cnt);
  dt_keyaccel_cleanup(&keyaccel);
  widget_end(); // commit params if still ongoing
}
