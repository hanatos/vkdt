#include "gui/win.h"

uint8_t glfw_keystate[GLFW_KEY_LAST+1] = {0};
bool g_initialised = false;

bool initialise(struct android_app* app);
void android_main(struct android_app* state);
void terminate();
void handle_cmd(struct android_app* app, int32_t cmd);
uint32_t handle_event(struct android_app *app, AInputEvent *event);

// XXX TODO merge this into main.c with some defines
void android_main(struct android_app* app)
{
  app->onAppCmd = handle_cmd;
  app->onInputEvent = handle_event;
      
  int events;
  android_poll_source* source;
  do
  {
    // app->looper <- wake this one for glfwPostEmptyEvent
// void ALooper_wake(ALooper *looper)
// Wakes the poll asynchronously.
    // poll once waits until next message comes in:
    // zero timeout means return immediately, -1 means wait indefinitely
    // ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData)
int
// Waits for events to be available, with optional timeout in milliseconds.
    // ALooper_pollOnce(focused ? 0 : -1, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT)
  // deprecated:
    if(ALooper_pollAll(g_initialized ? 1 : 0, 0, &events, &source) >= 0)
      if(source) source->process(app, source);

    // TODO main loop contents
    // if (g_initialised) VulkanDrawFrame();

  }
  while(app->destroyRequested == 0);
}

bool initialise(struct android_app* app)
{
  // TODO init the rest of qvk/gui
  // TODO instance extensions: VK_KHR_surface VK_KHR_android_surface
  // TODO device extension: VK_KHR_swapchain
}

void terminate()
{
  // TODO only finish native activity, do the rest when main exits the loop!

  g_initialized = false;
}

