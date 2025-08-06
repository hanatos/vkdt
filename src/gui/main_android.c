#include "gui/win.h"

uint8_t glfw_keystate[GLFW_KEY_LAST+1] = {0};
bool g_initialised = false;

bool initialize(struct android_app* app);
void android_main(struct android_app* state);
void terminate();
void handle_cmd(struct android_app* app, int32_t cmd);
uint32_t handle_event(struct android_app *app, AInputEvent *event);

void android_main(struct android_app* app)
{
  app->onAppCmd = handle_cmd;
  app->onInputEvent = handle_event;
      
  int events;
  android_poll_source* source;
  do
  {
    // ALooper_pollOnce(focused ? 0 : -1, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT)
    if(ALooper_pollAll(g_initialized ? 1 : 0, 0, &events, &source) >= 0)
      if(source) source->process(app, source);

    // TODO main loop contents
    // if (g_initialised) VulkanDrawFrame();

  }
  while(app->destroyRequested == 0);
}

bool initialize(struct android_app* app)
{
  if(!InitVulkan())
  {
    LOGE("Vulkan is unavailable, install vulkan and re-start");
    return false;
  }
  // TODO init the rest of qvk/gui
  // TODO instance extensions: VK_KHR_surface VK_KHR_android_surface
  // TODO device extension: VK_KHR_swapchain
}

void terminate()
{
  // TODO tear down everything

  g_initialized = false;
}

void handle_cmd(struct android_app* app, int32_t cmd)
{
  switch (cmd)
  {
    case APP_CMD_INIT_WINDOW:
      initialize(app);
      break;
    case APP_CMD_WINDOW_RESIZED:
      // TODO grab new size and issue resize callbacks
      break;
    case APP_CMD_TERM_WINDOW:
      terminate();
      break;
    default:;
  }
}

uint32_t handle_event(struct android_app *app, AInputEvent *event)
{
  // TODO call our callbacks, set keystates, set mouse buttons
  if(AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
  {
    int32_t evsrc = AInputEvent_getSource(event);
    switch (evsrc) {
      case AINPUT_SOURCE_JOYSTICK:
      {
        // TODO fill gamepad struct
        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
        break;
      }
      case AINPUT_SOURCE_TOUCHSCREEN:
      {
        int32_t action = AMotionEvent_getAction(event);
                for (size_t i = 0; i < AMotionEvent_getPointerCount(event); ++i) {
            x = AMotionEvent_getX(event, i);
            y = AMotionEvent_getY(event, i);
        break;
      }
    }
  }
  else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
  {
    // TODO translate into something that can be queried like glfwGetKey
    int32_t keycode = AKeyEvent_getKeyCode((const AInputEvent*)event);
    int32_t action  = AKeyEvent_getAction((const AInputEvent*)event);

    // ?? 
    // _glfwInputKey(_glfw.windowListHead, 0 , AKeyEvent_getKeyCode(event), GLFW_PRESS,0);
    // XXX certainly not
    if (action == AKEY_EVENT_ACTION_UP)
      return 0;

    switch (keycode)
    {
    case AKEYCODE_BUTTON_A:
      break;
    }
  }
  return 0; // did not handle the event
}
