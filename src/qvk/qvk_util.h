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

typedef struct BufferResource_s {
	VkBuffer buffer;
	VkDeviceMemory memory;
	size_t size;
	int is_mapped;
} BufferResource_t;

VkResult
buffer_create(
		BufferResource_t *buf,
		VkDeviceSize size, 
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags mem_properties);

VkResult buffer_destroy(BufferResource_t *buf);
void buffer_unmap(BufferResource_t *buf);
void *buffer_map(BufferResource_t *buf);
void buffer_unmap(BufferResource_t *buf);

uint32_t qvk_get_memory_type(uint32_t mem_req_type_bits, VkMemoryPropertyFlags mem_prop);

#define BARRIER_COMPUTE_BUFFER(buf) \
  do { \
    VkBufferMemoryBarrier mem_barrier = { \
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, \
      .srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT, \
      .dstAccessMask    = VK_ACCESS_TRANSFER_READ_BIT, \
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
      .buffer = buf, \
      .offset = 0, \
      .size = VK_WHOLE_SIZE, \
    }; \
		vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, \
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 1, &mem_barrier, \
        0, NULL); \
  } while (0)

#define IMAGE_BARRIER(cmd_buf, ...) \
	do { \
		VkImageMemoryBarrier img_mem_barrier = { \
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, \
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
			__VA_ARGS__ \
		}; \
		vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, \
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, \
				1, &img_mem_barrier); \
	} while(0)

// XXX FIXME: apparently i'm using it wrong. if the input layout is UNDEFINED
// the data may be discarded (but it's not, lucky me)
#define BARRIER_IMG_LAYOUT(img, layout) \
  do { \
    VkImageSubresourceRange subresource_range = { \
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, \
      .baseMipLevel   = 0, \
      .levelCount     = 1, \
      .baseArrayLayer = 0, \
      .layerCount     = 1 \
    }; \
    IMAGE_BARRIER(cmd_buf, \
        .image            = img, \
        .subresourceRange = subresource_range, \
        .srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT, \
        .dstAccessMask    = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT, \
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED, \
        .newLayout        = layout, \
    ); \
  } while(0)

#define BARRIER_COMPUTE(img) BARRIER_IMG_LAYOUT(img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)

#define IMAGE_BARRIER_SINK(cmd_buf, ...) \
	do { \
		VkImageMemoryBarrier img_mem_barrier = { \
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, \
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
			.dstQueueFamilyIndex = qvk.queue_idx_graphics, \
			__VA_ARGS__ \
		}; \
		vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, \
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, \
				1, &img_mem_barrier); \
	} while(0)

#define BARRIER_COMPUTE_SINK(img) \
  do { \
    VkImageSubresourceRange subresource_range = { \
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, \
      .baseMipLevel   = 0, \
      .levelCount     = 1, \
      .baseArrayLayer = 0, \
      .layerCount     = 1 \
    }; \
    IMAGE_BARRIER_SINK(cmd_buf, \
        .image            = img, \
        .subresourceRange = subresource_range, \
        .srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT, \
        .dstAccessMask    = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT, \
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED, \
        .newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    ); \
  } while(0)

#define CREATE_PIPELINE_LAYOUT(dev, layout, ...) \
	do { \
		VkPipelineLayoutCreateInfo pipeline_layout_info = { \
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, \
			__VA_ARGS__ \
		}; \
		_VK(vkCreatePipelineLayout(dev, &pipeline_layout_info, NULL, layout)); \
	} while(0) \

const char * vk_format_to_string(VkFormat format);

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
