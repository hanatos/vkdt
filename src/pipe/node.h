#pragma once
#include "token.h"
#include "connector.h"

#include <vulkan/vulkan.h>

// fwd declare
typedef struct dt_module_t dt_module_t;

typedef enum dt_node_type_t
{
  s_node_compute  = 0,
  s_node_graphics = 1,
}
dt_node_type_t;

typedef struct dt_node_t
{
  dt_token_t name;      // name of the node
  dt_token_t kernel;    // modules/node/kernel.comp is the file name of the compute shader

  dt_module_t *module;  // reference back to module and class

  dt_connector_t connector[DT_MAX_CONNECTORS];
  int num_connectors;

  VkPipeline            pipeline;
  VkPipelineLayout      pipeline_layout;
  VkDescriptorSet       dset;
  VkDescriptorSetLayout dset_layout;

  VkRenderPass          draw_render_pass; // needed for raster kernels
  VkFramebuffer         draw_framebuffer; // 
  dt_node_type_t        type;             // indicates whether we need a render pass and framebuffer

  uint32_t wd, ht, dp;  // dimensions of kernel to be run

  uint32_t push_constant[64];  // GTX1080 has size == 256 as max anyways
  size_t   push_constant_size;
}
dt_node_t;

