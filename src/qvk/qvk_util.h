/*
Copyright (C) 2018 Christoph Schied

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef  __VK_UTIL_H__
#define  __VK_UTIL_H__

#include <vulkan/vulkan.h>

uint32_t qvk_memory_get_uniform();
uint32_t qvk_memory_get_staging();
uint32_t qvk_memory_get_device();

#define BARRIER_COMPUTE_BUFFER(buf) \
  do { \
    VkBufferMemoryBarrier mem_barrier = { \
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, \
      .srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT, \
      .dstAccessMask    = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT, \
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
      .buffer = buf, \
      .offset = 0, \
      .size = VK_WHOLE_SIZE, \
    }; \
		vkCmdPipelineBarrier(cmd_buf, \
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, \
				VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 1, &mem_barrier, \
        0, NULL); \
  } while (0)

#define IMAGE_BARRIER(cmd_buf, ...) \
  do { \
    VkImageMemoryBarrier img_mem_barrier = { \
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, \
      __VA_ARGS__ \
    }; \
    vkCmdPipelineBarrier(cmd_buf, \
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, \
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,\
        0, NULL, 0, NULL, \
        1, &img_mem_barrier); \
  } while(0)

#define BARRIER_IMG_LAYOUT(img, old_layout, new_layout, mip_levels) \
  do { \
    IMAGE_BARRIER(cmd_buf, \
        .image            = img, \
        .subresourceRange = {\
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, \
          .baseMipLevel   = 0, \
          .levelCount     = mip_levels, \
          .baseArrayLayer = 0, \
          .layerCount     = 1 \
        }, \
        .srcAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT, \
        .dstAccessMask    = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT, \
        .oldLayout        = old_layout, \
        .newLayout        = new_layout, \
    ); \
  } while(0)

const char *qvk_result_to_string(VkResult result);
const char *qvk_format_to_string(VkFormat format);
const char *qvk_colourspace_to_string(VkColorSpaceKHR format);

#ifdef QVK_ENABLE_VALIDATION
#define ATTACH_LABEL_VARIABLE(a, type) \
	do { \
		/*Com_Printf("attaching object label 0x%08lx %s\n", (uint64_t) a, #a);*/ \
		VkDebugMarkerObjectNameInfoEXT name_info = { \
			.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT, \
			.object = (uint64_t) a, \
			.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_##type##_EXT, \
			.pObjectName = #a \
		}; \
		qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info); \
	} while(0)

#define ATTACH_LABEL_VARIABLE_NAME(a, type, name) \
	do { \
		/*printf("attaching object label 0x%08lx %s\n", (uint64_t) a, name);*/ \
		VkDebugMarkerObjectNameInfoEXT name_info = { \
			.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT, \
			.object = (uint64_t) a, \
			.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_##type##_EXT, \
			.pObjectName = name, \
		}; \
		qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info); \
	} while(0)
#else
#define ATTACH_LABEL_VARIABLE(a, type) do{}while(0)
#define ATTACH_LABEL_VARIABLE_NAME(a, type, name) do{}while(0)
#endif

#endif  /*__VK_UTIL_H__*/
