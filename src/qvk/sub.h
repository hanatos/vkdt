#pragma once
// apparently since vkQueueSubmit is so expensive what all game engines do is
// have a dedicated submitter thread. whatever, we'll do the same. may avoid
// spurious crashes in the nvidia driver (maybe they do thread local stuff?)
#include <vulkan/vulkan.h>

// same as vkQueueSubmit (for one submit item), but executed in a dedicated submitter thread.
// you'll wait for the fence anyways, you might as well wait for the thread.
void qvk_submit(
  VkQueue       queue,
  VkSubmitInfo *si,
  VkFence       fence);

// some required memory/mutex allocation
void qvk_sub_init();
void qvk_sub_cleanup();
