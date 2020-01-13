#pragma once
#include "token.h"
#include "connector.h"

#include <vulkan/vulkan.h>

#define DT_GRAPH_MAX_FRAMES 2

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

  dt_connector_t connector [DT_MAX_CONNECTORS];
  uint32_t       conn_image[DT_MAX_CONNECTORS]; // start offset for connector into graph's connector allocation pool
  int num_connectors;

  VkPipeline            pipeline;
  VkPipelineLayout      pipeline_layout;
  VkDescriptorSet       dset[DT_GRAPH_MAX_FRAMES]; // one descriptor set for every frame
  VkDescriptorSetLayout dset_layout;               // they all share the same layout

  VkRenderPass          draw_render_pass; // needed for raster kernels
  VkFramebuffer         draw_framebuffer; // 
  dt_node_type_t        type;             // indicates whether we need a render pass and framebuffer

  uint32_t wd, ht, dp;  // dimensions of kernel to be run

  uint32_t push_constant[64];  // GTX1080 has size == 256 as max anyways
  size_t   push_constant_size;
}
dt_node_t;

static inline uint32_t 
dt_node_connector_image(
    dt_node_t *node,
    int cid,
    int arr,
    int frame)
{
  return node->conn_image[cid] + node->connector[cid].array_length * frame + arr;
}

