#pragma once
#include "token.h"
#include "connector.h"

#include <vulkan/vulkan.h>

// fwd declare
typedef struct dt_module_t dt_module_t;

typedef struct dt_node_t
{
  dt_token_t name;      // name of the node
  dt_token_t kernel;    // kernel.comp is the file name of the compute shader

  dt_module_t *module;  // reference back to module and class

  dt_connector_t connector[DT_MAX_CONNECTORS];
  int num_connectors;

  VkPipeline            pipeline;
  VkPipelineLayout      pipeline_layout;
  VkDescriptorSet       dset;
  VkDescriptorSetLayout dset_layout;

  VkRenderPass          draw_render_pass; // needed for raster kernels

  uint32_t wd, ht, dp;  // dimensions of kernel to be run

  uint32_t push_constant[64];  // GTX1080 has size == 256 as max anyways
  size_t   push_constant_size;
}
dt_node_t;

