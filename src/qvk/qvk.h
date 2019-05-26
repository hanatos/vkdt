#pragma once
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

#include "core/core.h"
#include "core/log.h"

#include <vulkan/vulkan.h>

#include "qvk_util.h"

// TODO: replace
// #include "shaders/global_ubo.h"
// #include "shaders/global_textures.h"

#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))

#define QVK_LOG_FUNC_(f) do {} while(0)
//#define QVK_LOG_FUNC_(f) fprintf(stderr, "[qvk] %s\n", f)
#define QVK_LOG_FUNC() QVK_LOG_FUNC_(__func__)

#define QVK_FUNC_UNIMPLEMENTED_(f) dt_log(s_log_qvk, "calling unimplemented function %s", f)
#define QVK_FUNC_UNIMPLEMENTED() QVK_FUNC_UNIMPLEMENTED_(__func__)

#define QVK(...) \
	do { \
		VkResult _res = __VA_ARGS__; \
		if(_res != VK_SUCCESS) { \
			dt_log(s_log_qvk, "error %d executing %s!", _res, # __VA_ARGS__); \
		} \
	} while(0)

#define QVKR(...) \
	do { \
		VkResult _res = __VA_ARGS__; \
		if(_res != VK_SUCCESS) { \
			dt_log(s_log_qvk, "error %d executing %s!", _res, # __VA_ARGS__); \
      return _res; \
		} \
	} while(0)

/* see main.c to override default file path. By default it will strip away
 * QVK_MOD_, fix the file ending, and convert to lower case */
#define LIST_SHADER_MODULES \
	SHADER_MODULE_DO(QVK_MOD_STRETCH_PIC_VERT)                       \
	SHADER_MODULE_DO(QVK_MOD_STRETCH_PIC_FRAG)                       \
	SHADER_MODULE_DO(QVK_MOD_PATH_TRACER_RGEN)                       \
	SHADER_MODULE_DO(QVK_MOD_PATH_TRACER_RCHIT)                      \
	SHADER_MODULE_DO(QVK_MOD_PATH_TRACER_RMISS)                      \
	SHADER_MODULE_DO(QVK_MOD_PATH_TRACER_RTX_RGEN)                   \
	SHADER_MODULE_DO(QVK_MOD_PATH_TRACER_RTX_RCHIT)                  \
	SHADER_MODULE_DO(QVK_MOD_PATH_TRACER_RTX_RMISS)                  \
	SHADER_MODULE_DO(QVK_MOD_INSTANCE_GEOMETRY_COMP)                 \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_SEED_RNG_COMP)                    \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_FWD_PROJECT_COMP)                 \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_GRADIENT_IMG_COMP)                \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_GRADIENT_ATROUS_COMP)             \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_ATROUS_COMP)                      \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_TEMPORAL_COMP)                    \
	SHADER_MODULE_DO(QVK_MOD_ASVGF_TAA_COMP)                         \

#ifndef QVK_SHADER_DIR
#define QVK_SHADER_DIR "qvk_shaders"
#endif

#define QVK_SHADER_PATH_TEMPLATE QVK_SHADER_DIR "/%s.spv"

#define QVK_SHADER_STAGE(_module, _stage) \
	{ \
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, \
		.stage = _stage, \
		.module = qvk.shader_modules[_module], \
		.pName = "main" \
	}

enum QVK_SHADER_MODULES {
#define SHADER_MODULE_DO(a) a,
	LIST_SHADER_MODULES
#undef SHADER_MODULE_DO
	NUM_QVK_SHADER_MODULES
};

#define QVK_MAX_FRAMES_IN_FLIGHT 2

enum {
	SEM_IMG_AVAILABLE   = 0,
	SEM_RENDER_FINISHED = SEM_IMG_AVAILABLE   + QVK_MAX_FRAMES_IN_FLIGHT,
	NUM_SEMAPHORES      = SEM_RENDER_FINISHED + QVK_MAX_FRAMES_IN_FLIGHT,
};

#define QVK_MAX_SWAPCHAIN_IMAGES 4

// forward declare
typedef struct SDL_Window SDL_Window;

typedef struct qvk_t
{
	VkInstance                  instance;
	VkPhysicalDevice            physical_device;
	VkPhysicalDeviceMemoryProperties mem_properties;
	VkDevice                    device;
	VkQueue                     queue_graphics;
	VkQueue                     queue_compute;
	VkQueue                     queue_transfer;
	int32_t                     queue_idx_graphics;
	int32_t                     queue_idx_compute;
	int32_t                     queue_idx_transfer;
	VkSurfaceKHR                surface;
	VkSwapchainKHR              swap_chain;
	VkSurfaceFormatKHR          surf_format;
	VkPresentModeKHR            present_mode;
	VkExtent2D                  extent;
	VkCommandPool               command_pool;
	uint32_t                    num_swap_chain_images;
	VkImage                     swap_chain_images[QVK_MAX_SWAPCHAIN_IMAGES];
	VkImageView                 swap_chain_image_views[QVK_MAX_SWAPCHAIN_IMAGES];

	uint32_t                    num_command_buffers;
	VkCommandBuffer             *command_buffers;
	VkCommandBuffer             cmd_buf_current;

	uint32_t                    num_extensions;
	VkExtensionProperties       *extensions;

	uint32_t                    num_layers;
	VkLayerProperties           *layers;

	VkDebugUtilsMessengerEXT    dbg_messenger;

	VkSemaphore                 semaphores[NUM_SEMAPHORES];
	VkFence                     fences_frame_sync[QVK_MAX_FRAMES_IN_FLIGHT];


	int                         win_width;
	int                         win_height;
	uint64_t                    frame_counter;

	SDL_Window                  *window;
	uint32_t                    num_sdl2_extensions;
	const char                  **sdl2_extensions;

	uint32_t                    current_image_index;

	VkShaderModule              shader_modules[NUM_QVK_SHADER_MODULES];

	VkDescriptorSetLayout       desc_set_layout_ubo;
	VkDescriptorSet             desc_set_ubo[QVK_MAX_SWAPCHAIN_IMAGES];

	VkDescriptorSetLayout       desc_set_layout_textures;
	VkDescriptorSet             desc_set_textures;
  // TODO: bring back with headers above
	// VkImage                     images      [NUM_QVK_IMAGES];
	// VkImageView                 images_views[NUM_QVK_IMAGES];

	VkDescriptorSetLayout       desc_set_layout_vertex_buffer;
	VkDescriptorSet             desc_set_vertex_buffer;

	BufferResource_t            buf_vertex;
	BufferResource_t            buf_vertex_staging;

  // TODO: bring back with uniform header
	// QVKUniformBuffer_t uniform_buffer;
}
qvk_t;

extern qvk_t qvk;



#define _VK_EXTENSION_LIST
// none of these are supported on intel:
// #define _VK_EXTENSION_LIST \
// 	_VK_EXTENSION_DO(vkCreateAccelerationStructureNV) \
// 	_VK_EXTENSION_DO(vkCreateAccelerationStructureNV) \
// 	_VK_EXTENSION_DO(vkDestroyAccelerationStructureNV) \
// 	_VK_EXTENSION_DO(vkGetAccelerationStructureMemoryRequirementsNV) \
// 	_VK_EXTENSION_DO(vkBindAccelerationStructureMemoryNV) \
// 	_VK_EXTENSION_DO(vkCmdBuildAccelerationStructureNV) \
// 	_VK_EXTENSION_DO(vkCmdCopyAccelerationStructureNV) \
// 	_VK_EXTENSION_DO(vkCmdTraceRaysNV) \
// 	_VK_EXTENSION_DO(vkCreateRayTracingPipelinesNV) \
// 	_VK_EXTENSION_DO(vkGetRayTracingShaderGroupHandlesNV) \
// 	_VK_EXTENSION_DO(vkGetAccelerationStructureHandleNV) \
// 	_VK_EXTENSION_DO(vkCmdWriteAccelerationStructuresPropertiesNV) \
// 	_VK_EXTENSION_DO(vkCompileDeferredNV) \
// 	_VK_EXTENSION_DO(vkDebugMarkerSetObjectNameEXT)


#define _VK_EXTENSION_DO(a) extern PFN_##a q##a;
_VK_EXTENSION_LIST
#undef _VK_EXTENSION_DO

#define PROFILER_LIST \
	PROFILER_DO(PROFILER_FRAME_TIME,                 0) \
	PROFILER_DO(PROFILER_INSTANCE_GEOMETRY,          1) \
	PROFILER_DO(PROFILER_BVH_UPDATE,                 1) \
	PROFILER_DO(PROFILER_ASVGF_GRADIENT_SAMPLES,     1) \
	PROFILER_DO(PROFILER_PATH_TRACER,                1) \
	PROFILER_DO(PROFILER_ASVGF_FULL,                 1) \
	PROFILER_DO(PROFILER_ASVGF_RECONSTRUCT_GRADIENT, 2) \
	PROFILER_DO(PROFILER_ASVGF_TEMPORAL,             2) \
	PROFILER_DO(PROFILER_ASVGF_ATROUS,               2) \
	PROFILER_DO(PROFILER_ASVGF_TAA,                  2)

enum {
#define PROFILER_DO(a, ...) a,
	PROFILER_LIST
#undef PROFILER_DO
	NUM_PROFILER_ENTRIES,
};

#define NUM_PROFILER_QUERIES_PER_FRAME (NUM_PROFILER_ENTRIES * 2)

typedef enum {
	PROFILER_START, 
	PROFILER_STOP,
} QVKProfilerAction;

// global initialisation:
VkResult qvk_init();
VkResult qvk_cleanup();

VkResult qvk_shader_modules_initialize();
VkResult qvk_shader_modules_destroy();



VkResult qvk_profiler_initialize();
VkResult qvk_profiler_destroy();
VkResult qvk_profiler_query(int idx, QVKProfilerAction action);
VkResult qvk_profiler_next_frame(int frame_num);

VkResult qvk_textures_initialize();
VkResult qvk_textures_destroy();
VkResult qvk_textures_end_registration();

VkResult qvk_draw_initialize();
VkResult qvk_draw_destroy();
VkResult qvk_draw_destroy_pipelines();
VkResult qvk_draw_create_pipelines();
VkResult qvk_draw_submit_stretch_pics(VkCommandBuffer *cmd_buf);
VkResult qvk_draw_clear_stretch_pics();

VkResult qvk_uniform_buffer_create();
VkResult qvk_uniform_buffer_destroy();
VkResult qvk_uniform_buffer_update();

VkResult qvk_images_initialize();
VkResult qvk_images_destroy();

VkResult vkpt_asvgf_initialize();
VkResult vkpt_asvgf_destroy();
VkResult vkpt_asvgf_create_pipelines();
VkResult vkpt_asvgf_destroy_pipelines();
VkResult vkpt_asvgf_record_cmd_buffer(VkCommandBuffer cmd_buf);
VkResult vkpt_asvgf_create_gradient_samples(VkCommandBuffer cmd_buf, uint32_t frame_num);
