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
// this will schedule a vkQueueSubmit call in the main thread (calling qvk_sub_work occasionally)
// and if you pass a fence, will block until this happened. only then will the returned result
// be meaningful. if you don't pass a fence, it will always immediately return VK_SUCCESS.
VkResult qvk_submit(
  VkQueue       queue,
  int           cnt,     // ignored for now
  VkSubmitInfo *si,
  VkFence       fence);

int  qvk_sub_work();     // work on a slice of the queue
void qvk_sub_init();     // init queue
void qvk_sub_shutdown(); // signal the end is near
void qvk_sub_cleanup();  // cleanup
#ifdef __cplusplus
}
#endif
