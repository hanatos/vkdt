#include "buffer.h"
#include "core/log.h"
#include "qvk/qvk.h"

VkResult dt_check_device_allocation(uint64_t size, int heap_index)
{
  // vkAllocateMemory overcommits, moves to system ram, and sometimes works even when you think it should not.
  // find out whether we still stay in device memory, and fail over if not:
  if(size > qvk.max_allocation_size) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
  };
  VkPhysicalDeviceMemoryProperties2 memprop = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    .pNext = &budget,
  };
  vkGetPhysicalDeviceMemoryProperties2(qvk.physical_device, &memprop);
  for (int i=0;i<memprop.memoryProperties.memoryHeapCount;i++)
    if(memprop.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
      if(budget.heapBudget[i] > size)
        return VK_SUCCESS;
  dt_log(s_log_qvk, "failed to allocate %ld MB index %d", size/1024/1024, heap_index);
  return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

VkResult dt_buffer_alloc_debug(
    dt_buffer_t *b,
    int line, const char *file,
    uint64_t size, VkBufferUsageFlags usage, dt_buffer_flags_t flags)
{
  if(b->siz >= size) return VK_SUCCESS; // already allocated enough
  snprintf(b->dbg, sizeof(b->dbg), "%s:%d", file, line);
  const VkBufferUsageFlags usage_host_visible = 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = size,
    .usage = usage |
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
      (flags & s_buffer_flag_host_visible ? usage_host_visible : 0),
  };
  QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &b->buf));
  VkMemoryRequirements mem_req;
  vkGetBufferMemoryRequirements(qvk.device, b->buf, &mem_req);
  dt_check_device_allocation(mem_req.size, flags & s_buffer_flag_host_visible ? 1 : 0);
  b->siz = size; // that's the smaller size of the two
  VkMemoryAllocateFlagsInfo allocation_flags = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };
  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext           = &allocation_flags,
    .allocationSize  = mem_req.size,
    .memoryTypeIndex = flags & s_buffer_flag_host_visible ?
      qvk_memory_get_staging() : qvk_memory_get_device(),
  };
  QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &b->mem));
  vkBindBufferMemory(qvk.device, b->buf, b->mem, 0);
  if(flags & s_buffer_flag_mapped)
    QVKR(vkMapMemory(qvk.device, b->mem, 0, VK_WHOLE_SIZE, 0, &b->map));
  else b->map = 0;
  VkBufferDeviceAddressInfoKHR address_info = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR,
    .buffer = b->buf,
  };
  b->adr = vkGetBufferDeviceAddress(qvk.device, &address_info);
#ifdef DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
  VkDebugMarkerObjectNameInfoEXT name_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
    .object = (uint64_t)b->mem,
    .objectType = VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT,
    .pObjectName = b->dbg,
  };
  qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
#ifdef DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
  name_info = (VkDebugMarkerObjectNameInfoEXT) {
    .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
    .object = (uint64_t)b->buf,
    .objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
    .pObjectName = b->dbg,
  };
  qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
  return VK_SUCCESS;
}

void dt_buffer_init(dt_buffer_t *b)
{
  memset(b, 0, sizeof(*b));
}

void dt_buffer_free(dt_buffer_t *b)
{
  if(b->map) vkUnmapMemory(qvk.device, b->mem);
  vkDestroyBuffer(qvk.device, b->buf, 0);
  vkFreeMemory(qvk.device, b->mem, 0);
  memset(b, 0, sizeof(*b));
}
