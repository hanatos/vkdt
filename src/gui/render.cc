extern "C" {
#include "gui.h"
#include "view.h"
#include "qvk/qvk.h"
#include "pipe/graph-export.h"
#include "pipe/graph-io.h"
#include "pipe/graph-history.h"
#include "gui/darkroom-util.h"
}
#include "render.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "imnodes.h"
#include "widget_draw.hh"
#include "render_view.hh"
#if VKDT_USE_FREETYPE == 1
#include "misc/freetype/imgui_freetype.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "misc/freetype/imgui_freetype.cpp" // come on gcc, this is clearly not a header!
#pragma GCC diagnostic pop
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace { // anonymous gui state namespace

inline void dark_corporate_style()
{
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 * colors = style.Colors;

	/// 0 = FLAT APPEARENCE
	/// 1 = MORE "3D" LOOK
	int is3D = 0;

	colors[ImGuiCol_Text]                   = gamma(ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
	colors[ImGuiCol_TextDisabled]           = gamma(ImVec4(0.40f, 0.40f, 0.40f, 1.00f));
	colors[ImGuiCol_ChildBg]                = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
	colors[ImGuiCol_WindowBg]               = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
	colors[ImGuiCol_PopupBg]                = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
	colors[ImGuiCol_Border]                 = gamma(ImVec4(0.12f, 0.12f, 0.12f, 0.71f));
	colors[ImGuiCol_BorderShadow]           = gamma(ImVec4(1.00f, 1.00f, 1.00f, 0.06f));
	colors[ImGuiCol_FrameBg]                = gamma(ImVec4(0.42f, 0.42f, 0.42f, 0.54f));
	colors[ImGuiCol_FrameBgHovered]         = gamma(ImVec4(0.42f, 0.42f, 0.42f, 0.40f));
	colors[ImGuiCol_FrameBgActive]          = gamma(ImVec4(0.56f, 0.56f, 0.56f, 0.67f));
	colors[ImGuiCol_TitleBg]                = gamma(ImVec4(0.19f, 0.19f, 0.19f, 1.00f));
	colors[ImGuiCol_TitleBgActive]          = gamma(ImVec4(0.22f, 0.22f, 0.22f, 1.00f));
	colors[ImGuiCol_TitleBgCollapsed]       = gamma(ImVec4(0.17f, 0.17f, 0.17f, 0.90f));
	colors[ImGuiCol_MenuBarBg]              = gamma(ImVec4(0.335f, 0.335f, 0.335f, 1.000f));
	colors[ImGuiCol_ScrollbarBg]            = gamma(ImVec4(0.24f, 0.24f, 0.24f, 0.53f));
	colors[ImGuiCol_ScrollbarGrab]          = gamma(ImVec4(0.41f, 0.41f, 0.41f, 1.00f));
	colors[ImGuiCol_ScrollbarGrabHovered]   = gamma(ImVec4(0.52f, 0.52f, 0.52f, 1.00f));
	colors[ImGuiCol_ScrollbarGrabActive]    = gamma(ImVec4(0.76f, 0.76f, 0.76f, 1.00f));
	colors[ImGuiCol_CheckMark]              = gamma(ImVec4(0.65f, 0.65f, 0.65f, 1.00f));
	colors[ImGuiCol_SliderGrab]             = gamma(ImVec4(0.52f, 0.52f, 0.52f, 1.00f));
	colors[ImGuiCol_SliderGrabActive]       = gamma(ImVec4(0.64f, 0.64f, 0.64f, 1.00f));
	colors[ImGuiCol_Button]                 = gamma(ImVec4(0.54f, 0.54f, 0.54f, 0.35f));
	colors[ImGuiCol_ButtonHovered]          = gamma(ImVec4(0.52f, 0.52f, 0.52f, 0.59f));
	colors[ImGuiCol_ButtonActive]           = gamma(ImVec4(0.76f, 0.76f, 0.76f, 1.00f));
	colors[ImGuiCol_Header]                 = gamma(ImVec4(0.38f, 0.38f, 0.38f, 1.00f));
	colors[ImGuiCol_HeaderHovered]          = gamma(ImVec4(0.47f, 0.47f, 0.47f, 1.00f));
	colors[ImGuiCol_HeaderActive]           = gamma(ImVec4(0.76f, 0.76f, 0.76f, 0.77f));
	colors[ImGuiCol_Separator]              = gamma(ImVec4(0.000f, 0.000f, 0.000f, 0.137f));
	colors[ImGuiCol_SeparatorHovered]       = gamma(ImVec4(0.700f, 0.671f, 0.600f, 0.290f));
	colors[ImGuiCol_SeparatorActive]        = gamma(ImVec4(0.702f, 0.671f, 0.600f, 0.674f));
	colors[ImGuiCol_ResizeGrip]             = gamma(ImVec4(0.26f, 0.59f, 0.98f, 0.25f));
	colors[ImGuiCol_ResizeGripHovered]      = gamma(ImVec4(0.26f, 0.59f, 0.98f, 0.67f));
	colors[ImGuiCol_ResizeGripActive]       = gamma(ImVec4(0.26f, 0.59f, 0.98f, 0.95f));
	colors[ImGuiCol_PlotLines]              = gamma(ImVec4(0.61f, 0.61f, 0.61f, 1.00f));
	colors[ImGuiCol_PlotLinesHovered]       = gamma(ImVec4(1.00f, 0.43f, 0.35f, 1.00f));
	colors[ImGuiCol_PlotHistogram]          = gamma(ImVec4(0.90f, 0.70f, 0.00f, 1.00f));
	colors[ImGuiCol_PlotHistogramHovered]   = gamma(ImVec4(1.00f, 0.60f, 0.00f, 1.00f));
	colors[ImGuiCol_TextSelectedBg]         = gamma(ImVec4(0.73f, 0.73f, 0.73f, 0.35f));
	colors[ImGuiCol_ModalWindowDimBg]       = gamma(ImVec4(0.80f, 0.80f, 0.80f, 0.35f));
	colors[ImGuiCol_DragDropTarget]         = gamma(ImVec4(1.00f, 1.00f, 0.00f, 0.90f));
	colors[ImGuiCol_NavHighlight]           = gamma(ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
	colors[ImGuiCol_NavWindowingHighlight]  = gamma(ImVec4(1.00f, 1.00f, 1.00f, 0.70f));
	colors[ImGuiCol_NavWindowingDimBg]      = gamma(ImVec4(0.80f, 0.80f, 0.80f, 0.20f));

	style.PopupRounding = 3;

	style.WindowPadding = ImVec2(vkdt.state.panel_wd*0.01, vkdt.state.panel_wd*0.01);
	style.FramePadding  = ImVec2(vkdt.state.panel_wd*0.02, vkdt.state.panel_wd*0.01);
	style.ItemSpacing   = ImVec2(vkdt.state.panel_wd*0.01, vkdt.state.panel_wd*0.005);

	style.ScrollbarSize = 18;

	style.WindowBorderSize = 1;
	style.ChildBorderSize  = 1;
	style.PopupBorderSize  = 1;
	style.FrameBorderSize  = is3D;

	style.WindowRounding    = 0;
	style.ChildRounding     = 0;
	style.FrameRounding     = 0;
	style.ScrollbarRounding = 2;
	style.GrabRounding      = 3;

#ifdef IMGUI_HAS_DOCK
  style.TabBorderSize = is3D;
  style.TabRounding   = 3;

  colors[ImGuiCol_DockingEmptyBg]     = gamma(ImVec4(0.38f, 0.38f, 0.38f, 1.00f));
  colors[ImGuiCol_Tab]                = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
  colors[ImGuiCol_TabHovered]         = gamma(ImVec4(0.40f, 0.40f, 0.40f, 1.00f));
  colors[ImGuiCol_TabActive]          = gamma(ImVec4(0.33f, 0.33f, 0.33f, 1.00f));
  colors[ImGuiCol_TabUnfocused]       = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
  colors[ImGuiCol_TabUnfocusedActive] = gamma(ImVec4(0.33f, 0.33f, 0.33f, 1.00f));
  colors[ImGuiCol_DockingPreview]     = gamma(ImVec4(0.85f, 0.85f, 0.85f, 0.28f));

  if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }
#endif
}

static ImFont *g_font[4] = {0};
} // anonymous namespace

ImFont *dt_gui_imgui_get_font(int which)
{
  return g_font[which];
}

extern "C" void dt_gui_init_fonts()
{
  char tmp[PATH_MAX+100] = {0};
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();
  const float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
  float fontsize = floorf(qvk.win_height / 55.0f * dpi_scale);
  snprintf(tmp, sizeof(tmp), "%s/data/Roboto-Regular.ttf", dt_pipe.basedir);
  g_font[0] = io.Fonts->AddFontFromFileTTF(tmp, fontsize);
  g_font[1] = io.Fonts->AddFontFromFileTTF(tmp, floorf(1.5*fontsize));
  g_font[2] = io.Fonts->AddFontFromFileTTF(tmp, 2.0*fontsize);
  snprintf(tmp, sizeof(tmp), "%s/data/MaterialIcons-Regular.ttf", dt_pipe.basedir);
  ImFontConfig config;
  // config.MergeMode = true;
  config.GlyphMinAdvanceX = fontsize; // Use if you want to make the icon monospaced
  static const ImWchar icon_ranges[] = { 0xE000, 0xF000, 0};
  g_font[3] = io.Fonts->AddFontFromFileTTF(tmp, fontsize, &config, icon_ranges);
  vkdt.wstate.fontsize = fontsize;
#if VKDT_USE_FREETYPE == 1
  io.Fonts->TexGlyphPadding = 1;
  uint32_t flags = 0; // ImGuiFreeType::{NoHinting NoAutoHint ForceAutoHint LightHinting MonoHinting Bold Oblique Monochrome}
  for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
  {
    ImFontConfig* font_config = (ImFontConfig*)&io.Fonts->ConfigData[n];
    font_config->RasterizerMultiply = 1.0f;
  }
  ImGuiFreeType::BuildFontAtlas(io.Fonts, flags); // same flags
#endif

  { // upload Fonts, use any command queue
    VkCommandPool command_pool = vkdt.command_pool[0];
    VkCommandBuffer command_buffer = vkdt.command_buffer[0];

    vkResetCommandPool(qvk.device, command_pool, 0);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    vkEndCommandBuffer(command_buffer);
    threads_mutex_lock(&qvk.queue_mutex);
    vkQueueSubmit(qvk.queue_graphics, 1, &end_info, VK_NULL_HANDLE);
    threads_mutex_unlock(&qvk.queue_mutex);

    threads_mutex_lock(&qvk.queue_mutex);
    vkDeviceWaitIdle(qvk.device);
    threads_mutex_unlock(&qvk.queue_mutex);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
}

extern "C" int dt_gui_init_imgui()
{
  vkdt.wstate.lod = dt_rc_get_int(&vkdt.rc, "gui/lod", 1); // set finest lod by default
  // Setup Dear ImGui context
  ImGui::CreateContext();
  ImNodes::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = 0; // disable automatic writing of "imgui.ini"
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  if(vkdt.wstate.have_joystick)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos; //  | ImGuiBackendFlags_HasSetMousePos;

  // Setup Dear ImGui style
  // ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();
  dark_corporate_style();

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForVulkan(qvk.window, false);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance         = qvk.instance;
  init_info.PhysicalDevice   = qvk.physical_device;
  init_info.Device           = qvk.device;
  init_info.QueueFamily      = qvk.queue_idx_graphics;
  init_info.Queue            = qvk.queue_graphics;
  init_info.PipelineCache    = vkdt.pipeline_cache;
  init_info.DescriptorPool   = vkdt.descriptor_pool;
  init_info.Allocator        = 0;
  init_info.MinImageCount    = vkdt.min_image_count;
  init_info.ImageCount       = vkdt.image_count;
  init_info.CheckVkResultFn  = 0;//check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, vkdt.render_pass);

  char tmp[PATH_MAX+100] = {0};
  {
    int monitors_cnt;
    GLFWmonitor** monitors = glfwGetMonitors(&monitors_cnt);
    if(monitors_cnt > 2)
      fprintf(stderr, "[gui] you have more than 2 monitors attached! only the first two will be colour managed!\n");
    const char *name0 = glfwGetMonitorName(monitors[0]);
    const char *name1 = glfwGetMonitorName(monitors[MIN(monitors_cnt-1, 1)]);
    int xpos0, xpos1, ypos;
    glfwGetMonitorPos(monitors[0], &xpos0, &ypos);
    glfwGetMonitorPos(monitors[MIN(monitors_cnt-1, 1)], &xpos1, &ypos);
    float gamma0[] = {1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f};
    float rec2020_to_dspy0[] = { // to linear sRGB D65
       1.66022709, -0.58754775, -0.07283832,
      -0.12455356,  1.13292608, -0.0083496,
      -0.01815511, -0.100603  ,  1.11899813 };
    float gamma1[] = {1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f};
    float rec2020_to_dspy1[] = { // to linear sRGB D65
       1.66022709, -0.58754775, -0.07283832,
      -0.12455356,  1.13292608, -0.0083496,
      -0.01815511, -0.100603  ,  1.11899813 };
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.homedir, name0);
    FILE *f = fopen(tmp, "r");
    if(!f)
    {
      snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.basedir, name0);
      f = fopen(tmp, "r");
    }
    if(f)
    {
      fscanf(f, "%f %f %f\n", gamma0, gamma0+1, gamma0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+0, rec2020_to_dspy0+1, rec2020_to_dspy0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+3, rec2020_to_dspy0+4, rec2020_to_dspy0+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+6, rec2020_to_dspy0+7, rec2020_to_dspy0+8);
      fclose(f);
    }
    else fprintf(stderr, "[gui] no display profile file display.%s, using sRGB!\n", name0);
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.basedir, name1);
    f = fopen(tmp, "r");
    if(f)
    {
      fscanf(f, "%f %f %f\n", gamma1, gamma1+1, gamma1+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+0, rec2020_to_dspy1+1, rec2020_to_dspy1+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+3, rec2020_to_dspy1+4, rec2020_to_dspy1+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+6, rec2020_to_dspy1+7, rec2020_to_dspy1+8);
      fclose(f);
    }
    else fprintf(stderr, "[gui] no display profile file display.%s, using sRGB!\n", name1);
    int bitdepth = 8; // the display output will be dithered according to this
    if(qvk.surf_format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
       qvk.surf_format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      bitdepth = 10;
    ImGui_ImplVulkan_SetDisplayProfile(gamma0, rec2020_to_dspy0, gamma1, rec2020_to_dspy1, xpos1, bitdepth);
  }

  dt_gui_init_fonts();

  // prepare list of potential modules for ui selection:
  vkdt.wstate.module_names_buf = (char *)calloc(9, dt_pipe.num_modules+1);
  vkdt.wstate.module_names     = (const char **)malloc(sizeof(char*)*dt_pipe.num_modules);
  int pos = 0;
  for(uint32_t i=0;i<dt_pipe.num_modules;i++)
  {
    const char *name = dt_token_str(dt_pipe.module[i].name);
    const size_t len = strnlen(name, 8);
    memcpy(vkdt.wstate.module_names_buf + pos, name, len);
    vkdt.wstate.module_names[i] = vkdt.wstate.module_names_buf + pos;
    pos += len+1;
  }

  render_lighttable_init();
  render_darkroom_init();
  return 0;
}

// call from main loop:
extern "C" void dt_gui_render_frame_imgui()
{
  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  double now = glfwGetTime();

  static double button_pressed_time = 0.0;
  if(now - button_pressed_time > 0.1)
  { // the imgui/glfw backend does not support the "ps" button, so we do the stupid thing:
    int buttons_count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
    if(buttons && buttons[10]) vkdt.wstate.show_gamepadhelp ^= 1;
    button_pressed_time = now;
  }

  switch(vkdt.view_mode)
  {
    case s_view_files:
      render_files();
      break;
    case s_view_lighttable:
      render_lighttable();
      break;
    case s_view_darkroom:
      render_darkroom();
      break;
    case s_view_nodes:
      render_nodes();
      break;
    default:;
  }

  // draw notification message
  if(vkdt.wstate.notification_msg[0] &&
     now - vkdt.wstate.notification_time < 4.0)
  {
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar
      | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoResize
      | ImGuiWindowFlags_NoBackground;
    ImGui::SetNextWindowPos (ImVec2(vkdt.state.center_x,  vkdt.state.center_y/2),  ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.center_wd, 0.05 * vkdt.state.center_ht), ImGuiCond_Always);
    ImGui::Begin("notification message", 0, window_flags);
    ImGui::Text("%s", vkdt.wstate.notification_msg);
    ImGui::End();
  }

  ImGui::Render();
}

extern "C" void dt_gui_record_command_buffer_imgui(VkCommandBuffer cmd_buf)
{
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
}

extern "C" void dt_gui_cleanup_imgui()
{
  render_darkroom_cleanup();
  render_lighttable_cleanup();
  threads_mutex_lock(&qvk.queue_mutex);
  vkDeviceWaitIdle(qvk.device);
  threads_mutex_unlock(&qvk.queue_mutex);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImNodes::DestroyContext();
  ImGui::DestroyContext();
}

extern "C" void dt_gui_imgui_window_position(GLFWwindow *w, int x, int y)
{
  ImGui_ImplVulkan_SetWindowPos(x);
}

extern "C" void dt_gui_imgui_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}
extern "C" void dt_gui_imgui_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
  ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}
extern "C" void dt_gui_imgui_mouse_position(GLFWwindow *window, double x, double y)
{ }
extern "C" void dt_gui_imgui_character(GLFWwindow *window, int c)
{
  ImGui_ImplGlfw_CharCallback(window, c);
}
extern "C" void dt_gui_imgui_scrolled(GLFWwindow *window, double xoff, double yoff)
{
  ImGui_ImplGlfw_ScrollCallback(window, xoff, yoff);
}

extern "C" int dt_gui_imgui_want_mouse()
{
  return ImGui::GetIO().WantCaptureMouse;
}
extern "C" int dt_gui_imgui_want_keyboard()
{
  return ImGui::GetIO().WantCaptureKeyboard;
}
extern "C" int dt_gui_imgui_want_text()
{
  return ImGui::GetIO().WantTextInput; // this is meant for onscreen keyboards for TextInputs
}

extern "C" void dt_gui_grab_mouse()
{
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
  glfwSetInputMode(qvk.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  vkdt.wstate.grabbed = 1;
  // dt_gui_dr_toggle_fullscreen_view();
}

extern "C" void dt_gui_ungrab_mouse()
{
  ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
  glfwSetInputMode(qvk.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  vkdt.wstate.grabbed = 0;
  // dt_gui_dr_toggle_fullscreen_view();
}

struct dt_gamepadhelp_t
{
  int sp;
  const char *help[10][dt_gamepadhelp_cnt];
};
static dt_gamepadhelp_t g_gamepadhelp = {0};

extern "C" void dt_gamepadhelp_set(dt_gamepadhelp_input_t which, const char *str)
{
  if(which < 0 || which >= dt_gamepadhelp_cnt) return;
  g_gamepadhelp.help[g_gamepadhelp.sp][which] = str;
}
extern "C" void dt_gamepadhelp_clear()
{
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
    g_gamepadhelp.help[g_gamepadhelp.sp][k] = 0;
}
extern "C" void dt_gamepadhelp_push()
{
  int sp = 0;
  if(g_gamepadhelp.sp < 10) sp = ++g_gamepadhelp.sp;
  else return;
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
    g_gamepadhelp.help[sp][k] = g_gamepadhelp.help[sp-1][k];
}
extern "C" void dt_gamepadhelp_pop()
{
  if(g_gamepadhelp.sp > 0) g_gamepadhelp.sp--;
}

void dt_gamepadhelp()
{
  static const char *dt_gamepadhelp_input_str[] = {
    "analog L", "analog R", "trigger L2", "trigger R2",
    "triangle", "circle", "cross", "square", "L1", "R1", // "∆", "o", "x", "□"
    "up", "left", "down", "right", // "↑", "←", "↓", "→",
    "start", "select", "ps", "L3", "R3",
  };

  const float m[] = {
    vkdt.state.center_wd / 1200.0f, 0.0f, vkdt.state.center_wd * 0.30f,
    0.0f, vkdt.state.center_wd / 1200.0f, vkdt.state.center_ht * 0.50f,
  };
  dt_draw(dt_draw_list_gamepad, IM_ARRAYSIZE(dt_draw_list_gamepad), 10, m);
  for(int k=0;k<dt_gamepadhelp_cnt;k++)
  {
    if(g_gamepadhelp.help[g_gamepadhelp.sp][k])
    {
      dt_draw(dt_draw_list_gamepad_arrow[k], IM_ARRAYSIZE(dt_draw_list_gamepad_arrow[k]), 10, m);
      ImVec2 v = ImVec2(dt_draw_list_gamepad_arrow[k][8], dt_draw_list_gamepad_arrow[k][9]);
      ImVec2 pos = ImVec2(
          v.x * m[3*0 + 0] + v.y * m[3*0 + 1] + m[3*0 + 2],
          v.x * m[3*1 + 0] + v.y * m[3*1 + 1] + m[3*1 + 2]);
      pos.y -= vkdt.wstate.fontsize * 1.1;
      ImGui::GetWindowDrawList()->AddText(pos, IM_COL32_WHITE, dt_gamepadhelp_input_str[k]);
      pos.y += vkdt.wstate.fontsize * 1.1;
      ImGui::GetWindowDrawList()->AddText(pos, IM_COL32_WHITE, g_gamepadhelp.help[g_gamepadhelp.sp][k]);
    }
  }
}

extern "C" int dt_gui_imgui_input_blocked()
{
  return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
}
