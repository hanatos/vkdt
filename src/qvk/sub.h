#pragma once
// apparently since vkQueueSubmit is so expensive what all game engines do is
// have a dedicated submitter thread. whatever, we'll do the same. may avoid
// spurious crashes in the nvidia driver (maybe they do thread local stuff?)
#include <vulkan/vulkan.h>
#ifdef __cplusplus
extern "C" {
#endif

// same as vkQueueSubmit (for one submit item), but executed in a dedicated submitter thread.
// you'll wait for the fence anyways, you might as well wait for the thread.
void qvk_submit(
  VkQueue       queue,
  int           cnt,    // ignored for now
  VkSubmitInfo *si,
  VkFence       fence);

int  qvk_sub_work();    // work on a slice of the queue
void qvk_sub_init();    // init queue
void qvk_sub_cleanup();
#ifdef __cplusplus
}
#endif
