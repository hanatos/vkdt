#pragma once
#include "qvk/qvk.h"
#include <stdlib.h>

// extra storage on graph
typedef struct dt_raytrace_graph_t
{
  VkAccelerationStructureKHR                  accel;          // for ray tracing: top level for all nodes that may hold bottom level
  VkBuffer                                    buf_accel;      // memory for actual accel
  VkBuffer                                    buf_scratch;    // scratch memory for accel build
  VkBuffer                                    buf_staging;    // the buffer that goes with the rt_instances
  off_t                                       buf_accel_offset;
  off_t                                       buf_scratch_offset;
  off_t                                       buf_staging_offset;
	VkAccelerationStructureGeometryKHR          geometry;
  VkAccelerationStructureBuildGeometryInfoKHR build_info;
  VkDeviceMemory                              vkmem_scratch;
  VkDeviceMemory                              vkmem_staging;  // memory for our instance array + all nodes vtx, idx data
  VkDeviceMemory                              vkmem_accel;    // memory for our top level accel + all nodes bottom level accel
  int                                         staging_memory_type_bits;
  int                                         scratch_memory_type_bits;
  int                                         accel_memory_type_bits;
  size_t                                      scratch_end, scratch_max;
  size_t                                      staging_end, staging_max;
  size_t                                      accel_end,   accel_max;
  VkDescriptorSetLayout                       dset_layout;
  uint32_t                                   *nid;
  uint32_t                                    nid_cnt;
}
dt_raytrace_graph_t;

// extra storage on node
typedef struct dt_raytrace_node_t
{
  VkAccelerationStructureKHR                  accel;          // needed for ray tracing kernels: bottom level structure
	VkAccelerationStructureGeometryKHR          geometry;
  VkAccelerationStructureBuildGeometryInfoKHR build_info;     // geometry info
  VkBuffer                                    buf_accel;      // buffer to hold accel struct
  VkBuffer                                    buf_scratch;    // scratch memory for accel build
  VkBuffer                                    buf_vtx;        // vertex buffer
  VkBuffer                                    buf_idx;        // index buffer
  off_t                                       buf_accel_offset;
  off_t                                       buf_scratch_offset;
  off_t                                       buf_vtx_offset;
  off_t                                       buf_idx_offset;
  uint32_t                                    vtx_cnt;        // number of vertices provided
  uint32_t                                    idx_cnt;        // number of indices provided by this node
  uint32_t                                    tri_cnt;        // number of indices provided by this node, i.e. idx_cnt/3
}
dt_raytrace_node_t;

// fwd declare
typedef struct dt_graph_t dt_graph_t;
typedef struct dt_node_t dt_node_t;

// TODO: remove node functions and call from graph version?
VkResult dt_raytrace_node_init(dt_graph_t *graph, dt_node_t  *node);
VkResult dt_raytrace_graph_init(
    dt_graph_t *graph,
    int        *nid,
    int         nid_cnt);
VkResult dt_raytrace_graph_alloc(dt_graph_t *graph);
VkResult dt_raytrace_record_command_buffer_accel_build(dt_graph_t *graph);
void dt_raytrace_bind(); // TODO
void dt_raytrace_node_cleanup(dt_node_t *node);
void dt_raytrace_graph_cleanup(dt_graph_t *graph);
