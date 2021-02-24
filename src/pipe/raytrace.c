#include "raytrace.h"
#include "qvk/qvk.h"
#include "graph.h"
#include "core/log.h"
/// XXX TODO: cleanup
/// XXX TODO: bind descriptor sets in create_nodes or so! for all nodes with raytrace flag
/// * TODO: { .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR


VkResult
dt_raytrace_graph_init(
    dt_graph_t *graph,
    int        *nid,
    int         nid_cnt)
{
  graph->rt.nid_cnt = 0;
  for(int i=0;i<nid_cnt;i++)
    if(graph->node[nid[i]].type & s_node_geometry)
      graph->rt.nid[graph->rt.nid_cnt++] = nid[i];

#if 0
  // TODO create descriptor set 
      VkDescriptorSetLayoutBinding bindings[] = {{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
      },{
        .binding = 1, // module local uniform, params struct
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
      }};
      VkDescriptorSetLayoutCreateInfo dset_layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings,
      };
      QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &graph->uniform_dset_layout));
      // XXX===
#endif

  // TODO: go through nodes and request their sizes
  for(int i=0;i<graph->rt.nid_cnt;i++)
    dt_raytrace_node_init(graph, graph->node + graph->rt.nid[i]);

  return VK_SUCCESS;
}

// TODO: call in record_command_buffer for each node?
void
dt_raytrace_bind( // .. ?
    )
{
  // if node->type & s_node_raytrace
  // TODO: bind extra descriptor set
}

void
dt_raytrace_node_cleanup(
    dt_node_t *node)
{
  // TODO: do this separately?
}

// cleanup all ray tracing resources
void
dt_raytrace_graph_cleanup(
    dt_graph_t *graph)
{
//QVK_LOAD(vkDestroyAccelerationStructureKHR)
// TODO: destroy stuff:
    // top level on graph
    // bottom level on all nodes that have it
    // destroy buffers and deallocate the 3x vkmem we have reserved

    // TODO: destroy descriptor set layout

    // if (structure->top_level) qvkDestroyAccelerationStructureKHR(device->device, structure->top_level, NULL);
    // if (structure->bottom_level) qvkDestroyAccelerationStructureKHR(device->device, structure->bottom_level, NULL);
    // destroy_buffers(&structure->buffers, device);
}

#define CREATE_BUF_R(TYPE, SZ, BUF, BITS) do { \
  VkBufferCreateInfo buf_info = {\
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,\
    .size  = (SZ),\
    .usage = BITS \
  };\
  QVKR(vkCreateBuffer(qvk.device, &buf_info, 0, &(BUF)));\
  VkMemoryRequirements mr;\
  vkGetBufferMemoryRequirements(qvk.device, (BUF), &mr);\
  graph->rt.TYPE##_memory_type_bits |= mr.memoryTypeBits;\
  size_t off = graph->rt.TYPE##_end;\
  BUF##_offset = ((off + (mr.alignment-1)) & ~(mr.alignment-1));\
  graph->rt.TYPE##_end = BUF##_offset + mr.size;\
} while(0)

#define CREATE_SCRATCH_BUF_R(SZ, BUF) CREATE_BUF_R(scratch, SZ, BUF,\
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
#define CREATE_STAGING_BUF_R(SZ, BUF) CREATE_BUF_R(staging, SZ, BUF,\
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
#define CREATE_ACCEL_BUF_R(SZ, BUF) CREATE_BUF_R(accel, SZ, BUF,\
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)

#define ALLOC_MEM_R(TYPE, BITS) do { \
  if(graph->rt.TYPE##_end > graph->rt.TYPE##_max) { \
    if(graph->rt.vkmem_##TYPE ) {\
      QVKR(vkDeviceWaitIdle(qvk.device));\
      vkFreeMemory(qvk.device, graph->rt.vkmem_##TYPE, 0);\
    }\
    VkMemoryAllocateInfo mem_alloc_info = {\
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,\
      .allocationSize  = graph->rt.TYPE##_end,\
      .memoryTypeIndex = qvk_get_memory_type(graph->rt.TYPE##_memory_type_bits,\
          BITS )\
    };\
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->rt.vkmem_##TYPE));\
    graph->rt.TYPE##_max = graph->rt.TYPE##_end;\
}} while(0)

// called per node to determine the resource requirements of the rtgeo related buffers
VkResult
dt_raytrace_node_init(
    dt_graph_t *graph,
    dt_node_t  *node)
{
  // on node, get size for bottom level accel struct, allocate buffer for mesh
  CREATE_STAGING_BUF_R(node->rt.vtx_cnt * sizeof(float) * 3, node->rt.buf_vtx);
  CREATE_STAGING_BUF_R(node->rt.idx_cnt * sizeof(int),       node->rt.buf_idx);

  VkAccelerationStructureBuildSizesInfoKHR accel_size = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  node->rt.geometry = (VkAccelerationStructureGeometryKHR) {
    .sType             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType      = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .geometry          = {
      .triangles       = {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .maxVertex     = node->rt.vtx_cnt - 1,
        .vertexStride  = 3 * sizeof(float), // XXX set to 4 if we have the normal in between
        .transformData = {0},
        .indexType     = VK_INDEX_TYPE_UINT32,
        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
      }},
    .flags             = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };
  node->rt.build_info = (VkAccelerationStructureBuildGeometryInfoKHR) {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries   = &node->rt.geometry,
  };
  QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
  qvkGetAccelerationStructureBuildSizesKHR(
      qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &node->rt.build_info, &node->rt.tri_cnt, &accel_size);

  // create scratch buffer and accel struct backing buffer
  CREATE_SCRATCH_BUF_R(accel_size.buildScratchSize, node->rt.buf_scratch);
  CREATE_ACCEL_BUF_R(accel_size.accelerationStructureSize, node->rt.buf_accel);

  // create acceleration struct
  QVK_LOAD(vkCreateAccelerationStructureKHR);
  VkAccelerationStructureCreateInfoKHR create_info = {
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = node->rt.buf_accel,
    .offset = 0,
    .size   = accel_size.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
  };
  QVKR(qvkCreateAccelerationStructureKHR(qvk.device, &create_info, NULL, &node->rt.accel));

  return VK_SUCCESS;
}

// allocates memory, creates and binds rtgeo node buffers too
// TODO do this in graph_run() if a rebuild is needed (and only then, see module flags!)
VkResult
dt_raytrace_graph_alloc(
    dt_graph_t *graph)
{
  // create staging buffer for graph, allocate staging memory, bind graph + node staging:
  CREATE_STAGING_BUF_R(graph->rt.nid_cnt * sizeof(VkAccelerationStructureInstanceKHR), graph->rt.buf_staging);
  ALLOC_MEM_R(staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT); // allocate for all staging bufs

  // bind staging buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_staging, graph->vkmem_staging, graph->rt.buf_staging_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
  {
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_vtx, graph->rt.vkmem_staging, graph->node[graph->rt.nid[i]].rt.buf_vtx_offset));
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_idx, graph->rt.vkmem_staging, graph->node[graph->rt.nid[i]].rt.buf_idx_offset));
  }

  // get top-level acceleration structure size + scratch mem requirements.
  // create scratch buffer for graph and allocation, bind scratch buffers of graph and nodes.
  VkAccelerationStructureBuildSizesInfoKHR accel_size = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  graph->rt.geometry = (VkAccelerationStructureGeometryKHR) {
    .sType               = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType        = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .geometry            = {
      .instances         = {
        .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
      }},
    .flags               = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };
  graph->rt.build_info = (VkAccelerationStructureBuildGeometryInfoKHR) {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries   = &graph->rt.geometry,
  };
  QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
  qvkGetAccelerationStructureBuildSizesKHR(
      qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &graph->rt.build_info, &graph->rt.nid_cnt, &accel_size);

  CREATE_SCRATCH_BUF_R(accel_size.buildScratchSize, graph->rt.buf_scratch);
  ALLOC_MEM_R(scratch, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // HEAP ?? // VK_MEMORY_HEAP_DEVICE_LOCAL_BIT

  // bind scratch buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_scratch, graph->rt.vkmem_scratch, graph->rt.buf_scratch_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_scratch, graph->rt.vkmem_scratch, graph->node[graph->rt.nid[i]].rt.buf_scratch_offset));

  // create acceleration structure buffer
  CREATE_ACCEL_BUF_R(accel_size.accelerationStructureSize, graph->rt.buf_accel);
  ALLOC_MEM_R(accel, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // bind accel buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_accel, graph->rt.vkmem_accel, graph->rt.buf_accel_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_accel, graph->rt.vkmem_accel, graph->node[graph->rt.nid[i]].rt.buf_accel_offset));

  // now create the top-level acceleration structure
  QVK_LOAD(vkCreateAccelerationStructureKHR);
  VkAccelerationStructureCreateInfoKHR create_info = {
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = graph->rt.buf_accel,
    .offset = 0,
    .size   = accel_size.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
  };
  QVKR(qvkCreateAccelerationStructureKHR(qvk.device, &create_info, NULL, &graph->rt.accel));

  return VK_SUCCESS;
}
#undef CREATE_SCRATCH_BUF_R
#undef CREATE_STAGING_BUF_R
#undef CREATE_ACCEL_BUF_R
#undef ALLOC_MEM_R

// call this from graph_run once command buffer is ready:
VkResult
dt_raytrace_record_command_buffer_accel_build(
    dt_graph_t *graph)
{
  // XXX TODO: do not build bottom if not needed (see flags on module)

  QVK_LOAD(vkGetAccelerationStructureDeviceAddressKHR);
  QVK_LOAD(vkCmdBuildAccelerationStructuresKHR);

  // XXX TODO: also create vkCreateDescriptorSetLayout for the accel bindings (set 2 or 3 so?)

  uint8_t *mapped_staging = 0;
  QVKR(vkMapMemory(qvk.device, graph->rt.vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void **)&mapped_staging));
  VkAccelerationStructureInstanceKHR *instance =
    (VkAccelerationStructureInstanceKHR *)mapped_staging + graph->rt.buf_staging_offset;
  for(int i=0;i<graph->rt.nid_cnt;i++)
  { // check all nodes for ray tracing geometry
    dt_node_t *node = graph->node + graph->rt.nid[i];

#if 0
    // TODO: if callback exists and if globally required to upload stuff or module requested in specifically
    void *vtx = mapped_staging + node->rt.buf_vtx_offset;
    void *idx = mapped_staging + node->rt.buf_idx_offset;
    node->module->so->read_geo(node->module, vtx, idx);
#endif

    VkAccelerationStructureDeviceAddressInfoKHR address_request = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = node->rt.accel,
    };
    instance[i] = (VkAccelerationStructureInstanceKHR) {
      .transform = { .matrix = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
      }},
      .mask  = 0xFF,
      .flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
      .accelerationStructureReference = qvkGetAccelerationStructureDeviceAddressKHR(qvk.device, &address_request),
    };
    VkBufferDeviceAddressInfo address_info[] = {{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt.buf_scratch,
    },{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt.buf_vtx,
    },{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt.buf_idx,
    }};
    // could batch up the build_info and build_ranges and call vkBuildAccelerationStructuresKHR only once:
    node->rt.build_info.dstAccelerationStructure                     = node->rt.accel;
    node->rt.build_info.scratchData.deviceAddress                    = vkGetBufferDeviceAddress(qvk.device, address_info+0);
    node->rt.geometry.geometry.triangles = (VkAccelerationStructureGeometryTrianglesDataKHR) {
      .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .maxVertex     = node->rt.vtx_cnt - 1,
      .vertexStride  = 3 * sizeof(float), // XXX set to 4 if we have the normal in between
      .transformData = {0},
      .indexType     = VK_INDEX_TYPE_UINT32,
      .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData    = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+1)},
      .indexData     = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+2)},
    };
    VkAccelerationStructureBuildRangeInfoKHR build_range = {
      .primitiveCount = node->rt.tri_cnt };
    const VkAccelerationStructureBuildRangeInfoKHR *p_build_range = &build_range;
    qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, 1, &node->rt.build_info, &p_build_range);
  }
  vkUnmapMemory(qvk.device, graph->rt.vkmem_staging);

  // barrier before top level is starting to build:
#define ACCEL_BARRIER do {\
  VkMemoryBarrier barrier = { \
    .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER, \
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, \
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, \
  }; \
  vkCmdPipelineBarrier(graph->command_buffer, \
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, \
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, \
      0, 1, &barrier, 0, NULL, 0, NULL); \
  } while(0)
  ACCEL_BARRIER;

  // build top level accel
  VkBufferDeviceAddressInfo scratch_adress_info = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt.buf_scratch,
  };
  VkBufferDeviceAddressInfo index_address = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt.buf_staging,
  };
  graph->rt.build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, &scratch_adress_info);
  graph->rt.build_info.dstAccelerationStructure  = graph->rt.accel;
  graph->rt.geometry.geometry.instances = (VkAccelerationStructureGeometryInstancesDataKHR) {
    .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
    .arrayOfPointers = VK_FALSE,
    .data            = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, &index_address) },
  };
  VkAccelerationStructureBuildRangeInfoKHR build_range = {
    .primitiveCount = graph->rt.nid_cnt };
  const VkAccelerationStructureBuildRangeInfoKHR *p_build_range = &build_range;
  qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, 1, &graph->rt.build_info, &p_build_range);

  ACCEL_BARRIER; // push another barrier
#undef ACCEL_BARRIER
  // TODO: i suppose we could now destroy the staging buffers
  return VK_SUCCESS;
}
