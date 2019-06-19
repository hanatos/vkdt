#include "gui.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl.h"
#include "qvk/qvk.h"

#include <stdio.h>
#include <stdlib.h>

// XXX argh, what a terrible hack
#define dt_log fprintf
#define s_log_qvk stderr

extern "C" int dt_gui_init_imgui()
{
  // Setup Dear ImGui context
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();

  // Setup Platform/Renderer bindings
  ImGui_ImplSDL2_InitForVulkan(qvk.window);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = qvk.instance;
  init_info.PhysicalDevice = qvk.physical_device;
  init_info.Device = qvk.device;
  init_info.QueueFamily = qvk.queue_idx_graphics;
  init_info.Queue = qvk.queue_graphics;
  init_info.PipelineCache = vkdt.pipeline_cache;
  init_info.DescriptorPool = vkdt.descriptor_pool;
  init_info.Allocator = 0;
  init_info.MinImageCount = vkdt.min_image_count;
  init_info.ImageCount = vkdt.image_count;
  init_info.CheckVkResultFn = 0;//check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, vkdt.render_pass);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Read 'misc/fonts/README.txt' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
  //io.Fonts->AddFontDefault();
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
  //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
  //IM_ASSERT(font != NULL);

  // XXX TODO: move this out to gui.c so we don't need to use dt_log in cpp!
  // XXX maybe just remove the QVK() :/
  // upload Fonts
  {
    // use any command queue
    VkCommandPool command_pool = vkdt.command_pool[0];
    VkCommandBuffer command_buffer = vkdt.command_buffer[0];

    QVK(vkResetCommandPool(qvk.device, command_pool, 0));
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    QVK(vkBeginCommandBuffer(command_buffer, &begin_info));

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    QVK(vkEndCommandBuffer(command_buffer));
    QVK(vkQueueSubmit(qvk.queue_graphics, 1, &end_info, VK_NULL_HANDLE));

    QVK(vkDeviceWaitIdle(qvk.device));
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
  return 0;
}

extern "C" void dt_gui_poll_event_imgui(SDL_Event *event)
{
  // Poll and handle events (inputs, window resize, etc.)
  // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
  // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
  // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
  // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
  ImGui_ImplSDL2_ProcessEvent(event);
}

// call from main loop:
extern "C" void dt_gui_render_frame_imgui()
{
#if 0
        if (g_SwapChainRebuild)
        {
            g_SwapChainRebuild = false;
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, g_SwapChainResizeWidth, g_SwapChainResizeHeight, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
        }
#endif

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(qvk.window);
        ImGui::NewFrame();

        { // center image view
          ImGuiWindowFlags window_flags = 0;
          window_flags |= ImGuiWindowFlags_NoTitleBar;
          window_flags |= ImGuiWindowFlags_NoMove;
          window_flags |= ImGuiWindowFlags_NoResize;
          window_flags |= ImGuiWindowFlags_NoBackground;
          ImGui::SetNextWindowPos (ImVec2(0, 0),       ImGuiCond_FirstUseEver);
          ImGui::SetNextWindowSize(ImVec2(1420, 1080), ImGuiCond_FirstUseEver);
          ImGui::Begin("center", 0, window_flags);

          if(vkdt.graph_dev.output)
          {
            ImTextureID imgid = vkdt.graph_dev.output->dset;   // XXX put DescriptorSet of display node!
            float wd = (float)vkdt.graph_dev.output->connector[0].roi.roi_wd;
            float ht = (float)vkdt.graph_dev.output->connector[0].roi.roi_ht;
            float imwd = vkdt.view_width, imht = vkdt.view_height;
            float scale = MIN(imwd/wd, imht/ht);
            if(vkdt.view_scale > 0.0f) scale = vkdt.view_scale;
            float w = scale * wd, h = scale * ht;
            float cvx = vkdt.view_width *.5f;
            float cvy = vkdt.view_height*.5f;
            if(vkdt.view_look_at_x == FLT_MAX) vkdt.view_look_at_x = wd/2.0f;
            if(vkdt.view_look_at_y == FLT_MAX) vkdt.view_look_at_y = ht/2.0f;
            float ox = cvx - scale * vkdt.view_look_at_x;
            float oy = cvy - scale * vkdt.view_look_at_y;
            float x = ox + vkdt.view_x, y = oy + vkdt.view_y;
            // TODO: move uv coordinates instead of whole window!
            ImGui::GetWindowDrawList()->AddImage(
                imgid, ImVec2(x, y), ImVec2(x+w, y+h),
                ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
          }
          ImGui::End();
        }

        { // right panel
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
          ImGui::SetNextWindowPos (ImVec2(1420, 0),   ImGuiCond_FirstUseEver);
          ImGui::SetNextWindowSize(ImVec2(500, 1080), ImGuiCond_FirstUseEver);
          ImGui::Begin("panel-right", 0, window_flags);

          for(int i=0;i<vkdt.num_widgets;i++)
          {
            int modid = vkdt.widget_modid[i];
            int parid = vkdt.widget_parid[i];
            // TODO: distinguish by type:
            // TODO: distinguish by count:
            float *val = (float*)vkdt.graph_dev.module[modid].param + parid;
            char str[10] = {0};
            memcpy(str,
                &vkdt.graph_dev.module[modid].so->param[parid]->name, 8);
              ImGui::SliderFloat(str, val,
                  vkdt.graph_dev.module[modid].so->param[parid]->val[1],
                  vkdt.graph_dev.module[modid].so->param[parid]->val[2],
                  "%2.5f");
          }

#if 0
          ImGuiIO& io = ImGui::GetIO();
          ImTextureID imgid = io.Fonts->TexID;   // XXX put VkImage of display node!
          float wd = (float)io.Fonts->TexWidth;
          float ht = (float)io.Fonts->TexHeight;
          // lower level API:
          // ImGui::GetWindowDrawList()->AddImage()
          // IMGUI_API void  AddImage(ImTextureID user_texture_id, const ImVec2& a, const ImVec2& b, const ImVec2& uv_a = ImVec2(0,0), const ImVec2& uv_b = ImVec2(1,1), ImU32 col = IM_COL32_WHITE);
          ImGui::Image(imgid, ImVec2(wd, ht),    // VkImage and dimensions
              ImVec2(0,0), ImVec2(1,1),          // uv0 uv1
              ImVec4(1,1,1,1), ImVec4(0,0,0,0)); // tint + border col
#endif
          ImGui::End();
        }

        // Rendering
        ImGui::Render();
        // memcpy(&wd->ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
}

extern "C" void dt_gui_record_command_buffer_imgui(VkCommandBuffer cmd_buf)
{
  // Record Imgui Draw Data and draw funcs into command buffer
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
}

extern "C" void dt_gui_cleanup_imgui()
{
#if 0
    // Cleanup
    QVK(vkDeviceWaitIdle(g_Device));
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();
#endif
}
