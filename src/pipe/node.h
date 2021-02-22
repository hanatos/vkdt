#pragma once
#include "token.h"
#include "connector.h"

#include <vulkan/vulkan.h>

#define DT_GRAPH_MAX_FRAMES 2

// fwd declare
typedef struct dt_module_t dt_module_t;

typedef enum dt_node_type_t
{
  s_node_compute    = 0,  // compute shaders
  s_node_graphics   = 1,  // draw call/render pass
  s_node_geometry   = 2,  // generates geometry for ray tracing
  s_node_raytracing = 4,  // ray traces
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
  VkDescriptorSet       uniform_dset;              // uniform data is const per frame
  VkDescriptorSet       dset[DT_GRAPH_MAX_FRAMES]; // one descriptor set for every frame
  VkDescriptorSetLayout dset_layout;               // they all share the same layout

  VkRenderPass          draw_render_pass; // needed for raster kernels
  VkFramebuffer         draw_framebuffer; // 

  VkAccelerationStructureKHR                  rt_accel;      // needed for ray tracing kernels: bottom level structure
  VkAccelerationStructureBuildGeometryInfoKHR rt_build_info; // geometry info
  VkBuffer                                    rt_scratch;    // scratch memory for accel build
  VkBuffer                                    rt_vtx;        // vertex buffer
  VkBuffer                                    rt_idx;        // index buffer
  uint32_t                                    rt_tri_cnt;    // number of triangles provided by this node

  dt_node_type_t        type;             // indicates whether we need a render pass and framebuffer

  uint32_t wd, ht, dp;  // dimensions of kernel to be run

  uint32_t push_constant[64];  // GTX1080 has size == 256 as max anyways
  size_t   push_constant_size;
}
dt_node_t;

