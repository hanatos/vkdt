extern "C" {
#include "gui.h"
#include "render.h"
#include "view.h"
#include "qvk/qvk.h"
#include "pipe/modules/api.h"
#include "pipe/graph-export.h"
#include "pipe/graph-io.h"
#include "gui/darkroom-util.h"
#include "db/thumbnails.h"
#include "db/rc.h"
extern int g_busy;  // when does gui go idle. this is terrible, should put it in vkdt.gui_busy properly.
}
#include "gui/api.hh"
#include "gui/hotkey.hh"
#include "gui/widget_thumbnail.hh"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "widget_filebrowser.hh"
#if VKDT_USE_FREETYPE == 1
#include "misc/freetype/imgui_freetype.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "misc/freetype/imgui_freetype.cpp" // come on gcc, this is clearly not a header!
#pragma GCC diagnostic pop
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace { // anonymous gui state namespace

// TODO: also init from .config/vkdt/hotkeys
static ImHotKey::HotKey hk_lighttable[] = {
  {"tag",           "assign a tag to selected images",  GLFW_KEY_LEFT_CONTROL, GLFW_KEY_T},
  {"select all",    "toggle select all/none",           GLFW_KEY_LEFT_CONTROL, GLFW_KEY_A},
  {"export",        "export selected images",           GLFW_KEY_LEFT_CONTROL, GLFW_KEY_S},
  {"copy",          "copy from selected image",         GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
  {"paste history", "paste history to selected images", GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
  {"scroll cur",    "scroll to current image",          GLFW_KEY_LEFT_SHIFT,   GLFW_KEY_C},
  {"scroll end",    "scroll to end of collection",      GLFW_KEY_LEFT_SHIFT,   GLFW_KEY_G},
  {"scroll top",    "scroll to top of collection",      GLFW_KEY_G},
};
static ImHotKey::HotKey hk_darkroom[] = {
  {"create preset", "create new preset from image",     GLFW_KEY_LEFT_CONTROL, GLFW_KEY_O},
  {"apply preset",  "choose preset to apply",           GLFW_KEY_LEFT_CONTROL, GLFW_KEY_P},
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

  int set_nav_focus;
} gui = {gui_state_data_t::s_gui_state_regular};

void widget_end()
{
  if(!vkdt.wstate.grabbed)
  {
    if(vkdt.wstate.active_widget_modid < 0) return; // all good already
    // rerun all (roi could have changed, buttons are drastic)
    // TODO: let module decide this!
    vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
        s_graph_run_all);// &~s_graph_run_upload_source);
    // reset view:
    darkroom_reset_zoom();
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

inline ImVec4 gamma(ImVec4 in)
{
  // TODO: these values will be interpreted as rec2020 coordinates. do we want that?
  // theme colours are given as float sRGB values in imgui, while we will
  // draw them in linear:
  return ImVec4(pow(in.x, 2.2), pow(in.y, 2.2), pow(in.z, 2.2), in.w);
}

inline void dark_corporate_style()
{
  ImGuiStyle & style = ImGui::GetStyle();
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
} // anonymous namespace

extern "C" int dt_gui_init_imgui()
{
  vkdt.wstate.lod = dt_rc_get_int(&vkdt.rc, "gui/lod", 1); // set finest lod
  // Setup Dear ImGui context
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  // this works and looks really cool and useful (but collides with my own keys):
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
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
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_pipe.basedir, name0);
    FILE *f = fopen(tmp, "r");
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

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Read 'misc/fonts/README.txt' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
  //io.Fonts->AddFontDefault();
  const float dpi_scale = dt_rc_get_float(&vkdt.rc, "gui/dpiscale", 1.0f);
  float fontsize = qvk.win_height / 55.0f * dpi_scale;
  // io.Fonts->AddFontFromFileTTF("data/OpenSans-Light.ttf", fontsize);
  snprintf(tmp, sizeof(tmp), "%s/data/Roboto-Regular.ttf", dt_pipe.basedir);
  io.Fonts->AddFontFromFileTTF(tmp, fontsize);
  // io.Fonts->AddFontFromFileTTF("../ext/imgui/misc/fonts/Roboto-Medium.ttf", fontsize);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
  //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
  //IM_ASSERT(font != NULL);
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

  // upload Fonts
  {
    // use any command queue
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

    vkDeviceWaitIdle(qvk.device);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }

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
  return 0;
}

namespace {

void render_lighttable_center(double &hotkey_time)
{ // center image view
  { // assign star rating/colour labels via gamepad:
    int /*axes_cnt = 0,*/ butt_cnt = 0;
    const uint8_t* butt = vkdt.wstate.have_joystick ? glfwGetJoystickButtons(GLFW_JOYSTICK_1, &butt_cnt) : 0;
    // const float* axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt) : 0;
    static double gamepad_time = ImGui::GetTime();
    if(butt && butt[2]) // triangle
      if(ImGui::GetTime() - gamepad_time > 0.1)
    {
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      int rtdir = 0, lbdir = 0;
      if(butt[4]) rtdir = -1; // l1
      if(butt[5]) rtdir =  1; // r1
      if(butt[6]) lbdir = -1; // l2
      if(butt[7]) lbdir =  1; // r2
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      {
        vkdt.db.image[sel[i]].rating = CLAMP(vkdt.db.image[sel[i]].rating + rtdir, 0, 5);
        if(lbdir)
        vkdt.db.image[sel[i]].labels =
          vkdt.db.image[sel[i]].labels ?
          CLAMP(lbdir > 0 ? (vkdt.db.image[sel[i]].labels << 1) :
                            (vkdt.db.image[sel[i]].labels >> 1), 0, 8) : 1;
      }
      gamepad_time = ImGui::GetTime();
    }
  }
  ImGuiStyle &style = ImGui::GetStyle();
  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_NoTitleBar;
  window_flags |= ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoResize;
  window_flags |= ImGuiWindowFlags_NoBackground;
  ImGui::SetNextWindowPos (ImVec2(vkdt.state.center_x,  vkdt.state.center_y),  ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.center_wd, vkdt.state.center_ht), ImGuiCond_Always);
  ImGui::Begin("lighttable center", 0, window_flags);

  const int ipl = 6;
  const int border = 0.004 * qvk.win_width;
  const int wd = vkdt.state.center_wd / ipl - border*2 - style.ItemSpacing.x*2;
  const int ht = wd;
  const int cnt = vkdt.db.collection_cnt;
  const int lines = (cnt+ipl-1)/ipl;
  ImGuiListClipper clipper;
  clipper.Begin(lines);
  while(clipper.Step())
  {
    // for whatever reason (gauge sizes?) imgui will always pass [0,1) as a first range.
    // we don't want these to trigger a deferred load.
    // in case [0,1) is within the visible region, however, [1,8) might be the next
    // range, for instance. this means we'll need to do some weird dance to detect it
    // TODO: ^
    // fprintf(stderr, "displaying range %u %u\n", clipper.DisplayStart, clipper.DisplayEnd);
    dt_thumbnails_load_list(
        &vkdt.thumbnails,
        &vkdt.db,
        vkdt.db.collection,
        MIN(clipper.DisplayStart * ipl, (int)vkdt.db.collection_cnt-1),
        MIN(clipper.DisplayEnd   * ipl, (int)vkdt.db.collection_cnt));
    for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;line++)
    {
      int i = line * ipl;
      for(int k=0;k<ipl;k++)
      {
        uint32_t tid = vkdt.db.image[vkdt.db.collection[i]].thumbnail;
        if(tid == -1u) tid = 0; // busybee
        if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
        {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0, 1.0, 1.0, 1.0));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
        }
        else if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
        {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0.6, 0.6, 1.0));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
        }
        float scale = MIN(
            wd/(float)vkdt.thumbnails.thumb[tid].wd,
            ht/(float)vkdt.thumbnails.thumb[tid].ht);
        float w = vkdt.thumbnails.thumb[tid].wd * scale;
        float h = vkdt.thumbnails.thumb[tid].ht * scale;
        uint32_t ret = ImGui::ThumbnailImage(
            vkdt.db.collection[i],
            vkdt.thumbnails.thumb[tid].dset,
            ImVec2(w, h),
            ImVec2(0,0), ImVec2(1,1),
            border,
            ImVec4(0.5f,0.5f,0.5f,1.0f),
            ImVec4(1.0f,1.0f,1.0f,1.0f),
            vkdt.db.image[vkdt.db.collection[i]].rating,
            vkdt.db.image[vkdt.db.collection[i]].labels,
            (vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db)) ?
            vkdt.db.image[vkdt.db.collection[i]].filename : 0,
            (vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db)) ?
            gui.set_nav_focus : 0);
        if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
          gui.set_nav_focus = MAX(0, gui.set_nav_focus-1);
        if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db) ||
          (vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected))
          ImGui::PopStyleColor(2);

        if(ret)
        {
          g_busy += 2;
          if(ImGui::GetIO().KeyCtrl)
          {
            if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
              dt_db_selection_remove(&vkdt.db, i);
            else
              dt_db_selection_add(&vkdt.db, i);
          }
          else if(ImGui::GetIO().KeyShift)
          { // shift selects ranges
            uint32_t colid = dt_db_current_colid(&vkdt.db);
            if(colid != -1u)
            {
              int a = MIN(colid, (uint32_t)i);
              int b = MAX(colid, (uint32_t)i);
              dt_db_selection_clear(&vkdt.db);
              for(int i=a;i<=b;i++)
                dt_db_selection_add(&vkdt.db, i);
            }
          }
          else
          { // no modifier, select exactly this image:
            if(dt_db_selection_contains(&vkdt.db, i) ||
              (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)))
            {
              darkroom_reset_zoom();
              dt_view_switch(s_view_darkroom);
            }
            else
            {
              dt_db_selection_clear(&vkdt.db);
              dt_db_selection_add(&vkdt.db, i);
            }
          }
        }

        if(++i >= cnt) break;
        if(k < ipl-1) ImGui::SameLine();
      }
    }
  }
  // lt hotkeys in same scope as center window (scroll)
  if(ImGui::GetTime() - hotkey_time > 0.1)
  {
    int hotkey = ImHotKey::GetHotKey(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
    switch(hotkey)
    {
      case 5:
        dt_gui_lt_scroll_current();
        break;
      case 6:
        dt_gui_lt_scroll_bottom();
        break;
      case 7:
        dt_gui_lt_scroll_top();
        break;
      default:
        goto dont_update_time;
    }
    hotkey_time = ImGui::GetTime();
dont_update_time:;
  }
  ImGui::End(); // lt center window
}

void render_lighttable_right_panel(double &hotkey_time)
{ // right panel
  static dt_filebrowser_widget_t filebrowser = {{0}};
  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_NoTitleBar;
  // if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
  // window_flags |= ImGuiWindowFlags_MenuBar;
  window_flags |= ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoResize;
  // if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
  // if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
  // if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
  // if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
  ImGui::Begin("panel-right", 0, window_flags);

  float lineht = ImGui::GetTextLineHeight();
  float bwd = 0.5f;
  ImVec2 size(bwd*vkdt.state.panel_wd, 1.6*lineht);

  // lt hotkeys in same scope as buttons as modals (right panel)
  if(ImGui::GetTime() - hotkey_time > 0.1)
  {
    int hotkey = ImHotKey::GetHotKey(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]));
    switch(hotkey)
    {
      case 0: // assign tag
        dt_gui_lt_assign_tag();
        break;
      case 1: // toggle select all
        dt_gui_lt_toggle_select_all();
        break;
      case 2:
        dt_gui_lt_export();
        break;
      case 3:
        dt_gui_lt_copy();
        break;
      case 4:
        dt_gui_lt_paste_history();
        break;
      default:
        goto dont_update_time;
    }
    hotkey_time = ImGui::GetTime();
dont_update_time:;
  }

  if(ImGui::CollapsingHeader("settings"))
  {
    ImGui::Indent();
    if(ImGui::Button("hotkeys"))
      ImGui::OpenPopup("edit hotkeys");
    ImHotKey::Edit(hk_lighttable, sizeof(hk_lighttable)/sizeof(hk_lighttable[0]), "edit hotkeys");
    ImGui::Unindent();
  }

  if(ImGui::CollapsingHeader("collect"))
  {
    ImGui::Indent();
    int32_t filter_prop = static_cast<int32_t>(vkdt.db.collection_filter);
    int32_t sort_prop   = static_cast<int32_t>(vkdt.db.collection_sort);

    if(ImGui::Combo("sort", &sort_prop, dt_db_property_text))
    {
      vkdt.db.collection_sort = static_cast<dt_db_property_t>(sort_prop);
      dt_db_update_collection(&vkdt.db);
    }
    if(ImGui::Combo("filter", &filter_prop, dt_db_property_text))
    {
      vkdt.db.collection_filter = static_cast<dt_db_property_t>(filter_prop);
      dt_db_update_collection(&vkdt.db);
    }
    int filter_val = static_cast<int>(vkdt.db.collection_filter_val);
    if(ImGui::InputInt("filter value", &filter_val, 1, 100, 0))
    {
      vkdt.db.collection_filter_val = static_cast<uint64_t>(filter_val);
      dt_db_update_collection(&vkdt.db);
    }

    if(ImGui::Button("open directory", size))
    {
      const char *mru = dt_rc_get(&vkdt.rc, "gui/ruc_entry00", "null");
      if(strcmp(mru, "null"))
      {
        snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", mru);
        char *c = filebrowser.cwd + strlen(filebrowser.cwd) - 1;
        for(;*c!='/'&&c>filebrowser.cwd;c--);
        if(c > filebrowser.cwd) strcpy(c, "/"); // truncate at last '/' to remove subdir
        struct stat statbuf;
        int ret = stat(filebrowser.cwd, &statbuf);
        if(ret || (statbuf.st_mode & S_IFMT) != S_IFDIR) // don't point to non existing/non directory
          strcpy(filebrowser.cwd, "/");
      }
      dt_filebrowser_open(&filebrowser);
    }

    if(dt_filebrowser_display(&filebrowser, 'd'))
    { // "ok" pressed
      dt_gui_switch_collection(filebrowser.cwd);
      dt_filebrowser_cleanup(&filebrowser); // reset all but cwd
    }
    ImGui::Unindent();
  }

  if(ImGui::CollapsingHeader("tags"))
  {
    ImGui::Indent();
    // ==============================================================
    // assign tag modal popup:
    if(vkdt.db.selection_cnt)
      if(ImGui::Button("assign tag..", size))
        dt_gui_lt_assign_tag();

    // ==============================================================
    // recently used tags:
    char filename[PATH_MAX+100];
    for(int i=0;i<vkdt.tag_cnt;i++)
    {
      if(ImGui::Button(vkdt.tag[i], ImVec2(size.x*0.495, size.y)))
      { // load tag collection:
        snprintf(filename, sizeof(filename), "%s/tags/%s", vkdt.db.basedir, vkdt.tag[i]);
        dt_gui_switch_collection(filename);
      }
      if(((i & 3) != 3) && (i != vkdt.tag_cnt-1)) ImGui::SameLine();
    }
    // button to jump to original folder of selected image if it is a symlink
    uint32_t main_imgid = dt_db_current_imgid(&vkdt.db);
    if(main_imgid != -1u)
    {
      dt_db_image_path(&vkdt.db, main_imgid, filename, sizeof(filename));
      struct stat buf;
      lstat(filename, &buf);
      if(((buf.st_mode & S_IFMT)== S_IFLNK) && ImGui::Button("jump to original collection", size))
      {
        char *resolved = realpath(filename, 0);
        if(resolved)
        {
          char *c = 0;
          for(int i=0;resolved[i];i++) if(resolved[i] == '/') c = resolved+i;
          if(c) *c = 0; // get dirname, i.e. strip off image file name
          dt_gui_switch_collection(resolved);
          free(resolved);
        }
      }
    }
    ImGui::Unindent();
  } // end collapsing header "recent tags"

  if(ImGui::CollapsingHeader("recent collections"))
  { // recently used collections in ringbuffer:
    int32_t num = CLAMP(dt_rc_get_int(&vkdt.rc, "gui/ruc_num", 0), 0, 10);
    for(int i=0;i<num;i++)
    {
      char entry[512];
      snprintf(entry, sizeof(entry), "gui/ruc_entry%02d", i);
      const char *dir = dt_rc_get(&vkdt.rc, entry, "null");
      if(strcmp(dir, "null"))
        if(ImGui::Button(dir))
          dt_gui_switch_collection(dir);
    }
  } // end collapsing header "recent collections"

  if(vkdt.db.selection_cnt > 0 && ImGui::CollapsingHeader("selected images"))
  {
    ImGui::Indent();
    // ==============================================================
    // copy/paste history stack
    if(ImGui::Button("copy history stack", size))
      dt_gui_lt_copy();

    if(vkdt.wstate.copied_imgid != -1u)
    {
      ImGui::SameLine();
      if(ImGui::Button("paste history stack", size))
        dt_gui_lt_paste_history();
    }

    // ==============================================================
    // delete images
    static int really_delete = 0;
    if(ImGui::Button("delete image[s]", size))
      really_delete ^= 1;

    if(really_delete)
    {
      ImGui::SameLine();
      if(ImGui::Button("*really* delete image[s]", size))
      {
        dt_db_remove_selected_images(&vkdt.db, &vkdt.thumbnails, 1);
        really_delete = 0;
      }
    }

    // ==============================================================
    // reset history stack
    if(ImGui::Button("reset history stack", size))
    {
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      char filename[1024], realname[PATH_MAX];
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      {
        dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
        realpath(filename, realname);
        unlink(realname);
        dt_thumbnails_invalidate(&vkdt.thumbnail_gen, filename);
      }
      dt_thumbnails_cache_list(
          &vkdt.thumbnail_gen,
          &vkdt.db,
          sel, vkdt.db.selection_cnt);
    }

    // ==============================================================
    // merge/align images
    if(vkdt.db.selection_cnt > 1)
    if(ImGui::Button("merge into current", size))
    {
      // overwrite .cfg for this image file:
      uint32_t main_imgid = dt_db_current_imgid(&vkdt.db);
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);
      char filename[1024] = {0}, realname[PATH_MAX] = {0};
      dt_db_image_path(&vkdt.db, main_imgid, filename, sizeof(filename));
      FILE *f = fopen(filename, "wb");
      fprintf(f, "frames:1\n");
      fprintf(f, "module:i-raw:main\n");
      for(uint32_t i=1;i<vkdt.db.selection_cnt;i++)
      {
        fprintf(f, "module:i-raw:%02d\n", i);
        fprintf(f, "module:align:%02d\n", i);
        fprintf(f, "module:blend:%02d\n", i);
      }
      fprintf(f,
          "module:denoise:01\n"
          "module:hilite:01\n"
          "module:demosaic:01\n"
          "module:colour:01\n"
          "module:filmcurv:01\n"
          "module:hist:01\n"
          "module:display:hist\n"
          "module:display:main\n");
      realpath(filename, realname);
      char *base = basename(realname);
      base[strlen(base)-4] = 0;
      fprintf(f, "param:i-raw:main:filename:%s\n", base);
      int ii = 1;
      for(uint32_t i=0;i<vkdt.db.selection_cnt;i++)
      {
        if(sel[i] == main_imgid) continue;
        dt_db_image_path(&vkdt.db, sel[i], filename, sizeof(filename));
        realpath(filename, realname);
        char *base = basename(realname);
        base[strlen(base)-4] = 0;
        fprintf(f, "param:i-raw:%02d:filename:%s\n", ii, base);
        fprintf(f,
            "connect:i-raw:%02d:output:align:%02d:alignsrc\n"
            "connect:i-raw:%02d:output:align:%02d:input\n"
            "connect:align:%02d:output:blend:%02d:back\n"
            "connect:align:%02d:mask:blend:%02d:mask\n",
            ii, ii, ii, ii, ii, ii, ii, ii);
        if(ii == 1) fprintf(f, "connect:i-raw:main:output:blend:%02d:input\n", ii);
        else        fprintf(f, "connect:blend:%02d:output:blend:%02d:input\n", ii-1, ii);
        fprintf(f, "connect:i-raw:main:output:align:%02d:aligndst\n", ii);
        fprintf(f,
            "param:blend:%02d:opacity:%g\n"
            "param:align:%02d:merge_n:0.05\n"
            "param:align:%02d:merge_k:30\n"
            "param:align:%02d:blur0:2\n"
            "param:align:%02d:blur1:16\n"
            "param:align:%02d:blur2:32\n"
            "param:align:%02d:blur3:64\n",
            ii, pow(0.5, ii),
            ii, ii, ii, ii, ii, ii);
        ii++;
      }
      // TODO: grab from default darkroom cfg?
      fprintf(f,
          "connect:blend:%02d:output:denoise:01:input\n"
          "connect:denoise:01:output:hilite:01:input\n"
          "connect:hilite:01:output:demosaic:01:input\n"
          "connect:demosaic:01:output:colour:01:input\n"
          "connect:colour:01:output:filmcurv:01:input\n"
          "connect:filmcurv:01:output:display:main:input\n"
          "connect:filmcurv:01:output:hist:01:input\n"
          "connect:hist:01:output:display:hist:input\n", vkdt.db.selection_cnt-1);
      fclose(f);
      // now redo/delete thumbnail of main_imgid
      dt_thumbnails_cache_list(
          &vkdt.thumbnail_gen,
          &vkdt.db,
          &main_imgid, 1);
    }
    ImGui::Unindent();
  } // end collapsing header "selected"

  // ==============================================================
  // export selection
  if(vkdt.db.selection_cnt > 0 && ImGui::CollapsingHeader("export selection"))
  {
    ImGui::Indent();
    static int wd = dt_rc_get_int(&vkdt.rc, "gui/export/wd", 0);
    static int ht = dt_rc_get_int(&vkdt.rc, "gui/export/ht", 0);
    static int format = dt_rc_get_int(&vkdt.rc, "gui/export/format", 0);
    static float quality = dt_rc_get_float(&vkdt.rc, "gui/export/quality", 90);
    static char basename[240] = "";
    if(basename[0] == 0) strncpy(basename,
        dt_rc_get(&vkdt.rc, "gui/export/basename", "/tmp/img"),
        sizeof(basename)-1);
    const char format_data[] = "jpg\0pfm\0ffmpeg\0\0";
    if(ImGui::InputInt("width", &wd, 1, 100, 0))
      dt_rc_set_int(&vkdt.rc, "gui/export/wd", wd);
    if(ImGui::InputInt("height", &ht, 1, 100, 0))
      dt_rc_set_int(&vkdt.rc, "gui/export/ht", ht);
    if(ImGui::InputText("filename", basename, sizeof(basename)))
      dt_rc_set(&vkdt.rc, "gui/export/basename", basename);
    if(ImGui::InputFloat("quality", &quality, 1, 100, 0))
      dt_rc_set_float(&vkdt.rc, "gui/export/quality", quality);
    if(ImGui::Combo("format", &format, format_data))
      dt_rc_set_int(&vkdt.rc, "gui/export/format", format);
    if(ImGui::Button("export", size))
      dt_gui_lt_export();
    ImGui::Unindent();
  } // end collapsing header "export"

  dt_gui_lt_modals(); // draw modal windows from buttons/hotkeys

  ImGui::End(); // lt right panel
}

void render_lighttable()
{
  static double hotkey_time = 0;
  render_lighttable_right_panel(hotkey_time);
  render_lighttable_center(hotkey_time);
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
          cerr = dt_module_connect(graph, m_prev, c_prev, m_new, c_new_in);
          if(!cerr)
            cerr = dt_module_connect(graph, m_new, c_new_out, m_our, c_our_in);
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
            cerr = dt_module_connect(graph, m_prev, c_prev, m_after[0], c_after[0]);
            if(!cerr)
              cerr = dt_module_connect(graph, m_after[0], c_after[0], m_our, c_our_in);
            if(!cerr)
              cerr = dt_module_connect(graph, m_our, c_our_out, m_sscc, c_sscc);
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
            cerr = dt_module_connect(graph, m_pprv, c_pprv, m_our, c_our_in);
            if(!cerr)
              cerr = dt_module_connect(graph, m_our, c_our_out, m_prev, c_prev_in);
            for(int k=0;k<cnt;k++)
              if(!cerr)
                cerr = dt_module_connect(graph, m_prev, c_prev, m_after[k], c_after[k]);
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
        cerr = dt_module_connect(graph, -1, -1, m_our, c_our);
        for(int k=0;k<cnt;k++)
          if(!cerr)
            cerr = dt_module_connect(graph, m_prev, c_prev, m_after[k], c_after[k]);
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
              cerr = dt_module_connect(graph, -1, -1, mod[1], con[1]);
            else
              cerr = dt_module_connect(graph, mod[0], con[0], mod[1], con[1]);
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

namespace {
inline void draw_widget(int modid, int parid)
{
  const dt_ui_param_t *param = vkdt.graph_dev.module[modid].so->param[parid];
  ImGuiIO& io = ImGui::GetIO();

  // skip if group mode does not match:
  if(param->widget.grpid != -1)
    if(dt_module_param_int(vkdt.graph_dev.module + modid, param->widget.grpid)[0] != param->widget.mode)
      return;

  double time_now = ImGui::GetTime();
  static double gamepad_time = ImGui::GetTime();
  int axes_cnt = 0, butt_cnt = 0;
  const uint8_t *butt = vkdt.wstate.have_joystick ? glfwGetJoystickButtons(GLFW_JOYSTICK_1, &butt_cnt) : 0;
  const float   *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes   (GLFW_JOYSTICK_1, &axes_cnt) : 0;
  static int gamepad_reset = 0;
  if(time_now - gamepad_time > 0.1 && butt && butt[12])
  {
    gamepad_reset = 1;
    gamepad_time = time_now;
  }
  // some state for double click detection for reset functionality
  static int doubleclick = 0;
  static double doubleclick_time = 0;
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
          dt_graph_run_t flags = s_graph_run_none;
          if(vkdt.graph_dev.module[modid].so->check_params)
            flags = vkdt.graph_dev.module[modid].so->check_params(vkdt.graph_dev.module+modid, parid, &oldval);
          vkdt.graph_dev.runflags = static_cast<dt_graph_run_t>(
              s_graph_run_record_cmd_buf | s_graph_run_wait_done | flags);
          vkdt.graph_dev.active_module = modid;
        }
        KEYFRAME
        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
          ImGui::SetTooltip("%s %.3f", str, val[0]);

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
          g_busy += 2;
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
        if(time_now - gamepad_time > 0.1)
        {
          if(io.NavInputs[ImGuiNavInput_TweakFast] > 0.0f)
          {
            vkdt.wstate.selected ++;
            if(vkdt.wstate.selected == 4) vkdt.wstate.selected = 0;
            gamepad_time = time_now;
          }
          if(io.NavInputs[ImGuiNavInput_Activate] > 0.0f)
          {
            accept = 1;
            gamepad_time = time_now;
          }
        }
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
        if(ImGui::Button(string) || accept) widget_end();
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
        if(time_now - gamepad_time > 0.1)
        {
          if(io.NavInputs[ImGuiNavInput_TweakFast] > 0.0f)
          {
            vkdt.wstate.selected ++;
            if(vkdt.wstate.selected == 4) vkdt.wstate.selected = 0;
            gamepad_time = time_now;
          }
          if(io.NavInputs[ImGuiNavInput_Activate] > 0.0f)
          {
            accept = 1;
            gamepad_time = time_now;
          }
        }
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
          widget_end();
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
          widget_end();
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
        if(ImGui::Button(string)) widget_end();
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
          vkdt.graph_dev.runflags = s_graph_run_all; // kinda grave change, rerun all
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
} // anonymous namespace

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
            { // only if not connected yet
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
          if(out_dspy)
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
    if(dt_module_add(graph, dt_token(vkdt.wstate.module_names[add_modid]), dt_token(mod_inst)) == -1)
      last_err = 16ul<<32;

  static dt_filebrowser_widget_t filebrowser = {{0}};
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
    if(dt_filebrowser_display(&filebrowser, 'f'))
    { // "ok" pressed
      fprintf(stderr, "read file: %s/%s\n", filebrowser.cwd, filebrowser.selected);
      snprintf(gui.block_filename, sizeof(gui.block_filename), "%s/%s", filebrowser.cwd, filebrowser.selected);
      dt_filebrowser_cleanup(&filebrowser); // reset all but cwd
      gui.state = gui_state_data_t::s_gui_state_insert_block;
      // .. and render_module() will continue adding it using the data in gui.block* when the "insert before this" button is pressed.
    }
    if(ImGui::Button("insert block.."))
    {
      // gui.state = gui_state_data_t::s_gui_state_insert_block;
      gui.block_token[0] = dt_token(mod_inst);
      // TODO: open a browser with all data/blocks/*.cfg
      snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s/data/blocks", dt_pipe.basedir);
      dt_filebrowser_open(&filebrowser);
    }
  }
}

void render_darkroom()
{
  int axes_cnt = 0, butt_cnt = 0;
  const uint8_t *butt = vkdt.wstate.have_joystick ? glfwGetJoystickButtons(GLFW_JOYSTICK_1, &butt_cnt) : 0;
  const float   *axes = vkdt.wstate.have_joystick ? glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_cnt)    : 0;
  { // center image view
    int win_x = vkdt.state.center_x,  win_y = vkdt.state.center_y;
    int win_w = vkdt.state.center_wd, win_h = vkdt.state.center_ht;
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    // draw background over the full thing:
    ImVec2 border = ImVec2(2*win_x, 2*win_y);
    ImGui::SetNextWindowPos (ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(win_w+border.x, win_h+border.y), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gamma(ImVec4(0.5, 0.5, 0.5, 1.0)));
    ImGui::Begin("darkroom center", 0, window_flags);

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

      if(io.NavInputs[ImGuiNavInput_Cancel] > 0.0f)
      {
        dt_view_switch(s_view_lighttable);
        gui.set_nav_focus = 2; // introduce some delay because imgui nav has it too
        goto abort;
      }
      else if(io.NavInputs[ImGuiNavInput_Menu] == 0.0f &&
              io.NavInputs[ImGuiNavInput_TweakSlow] > 0.0f)
        dt_gui_dr_prev();
      else if(io.NavInputs[ImGuiNavInput_Menu] == 0.0f &&
              io.NavInputs[ImGuiNavInput_TweakFast] > 0.0f)
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
      if(butt && butt[11]) // left stick pressed
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
  }

  { // right panel
    int hotkey = -1;
    static double hotkey_time = 0;
    if(ImGui::GetTime() - hotkey_time > 0.02)
    {
      hotkey = ImHotKey::GetHotKey(hk_darkroom, sizeof(hk_darkroom)/sizeof(hk_darkroom[0]));
      hotkey_time = ImGui::GetTime();
    }
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    // if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
    // window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    // if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
    // if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
    // if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
    // if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos (ImVec2(qvk.win_width - vkdt.state.panel_wd, 0),   ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vkdt.state.panel_wd, vkdt.state.panel_ht), ImGuiCond_Always);
    ImGui::Begin("panel-right", 0, window_flags);

    // draw histogram image:
    dt_node_t *out_hist = dt_graph_get_display(&vkdt.graph_dev, dt_token("hist"));
    if(out_hist)
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
    if(out_view0)
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
            vkdt.graph_dev.frame_cnt = vkdt.state.anim_max_frame;
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
        default:;
      }
      dt_gui_dr_modals(); // draw modal window for presets
    }

    ImGui::End();
  } // end right panel
}

} // anonymous namespace

// call from main loop:
extern "C" void dt_gui_render_frame_imgui()
{
  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  switch(vkdt.view_mode)
  {
    case s_view_lighttable:
      render_lighttable();
      break;
    case s_view_darkroom:
      render_darkroom();
      break;
    default:;
  }

  // draw notification message
  double now = glfwGetTime();
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
  // Record Imgui Draw Data and draw funcs into command buffer
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
}

extern "C" void dt_gui_cleanup_imgui()
{
  widget_end(); // commit params if still ongoing
  vkDeviceWaitIdle(qvk.device);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
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
  return ImGui::GetIO().WantTextInput;
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
