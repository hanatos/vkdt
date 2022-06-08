#include "raytrace.h"
#include "graph.h"
#include "core/log.h"
#include "modules/api.h"

int dt_raytrace_present(dt_graph_t *graph)
{
  return qvk.raytracing_supported && (graph->rt.nid_cnt > 0);
}

void dt_raytrace_graph_reset(dt_graph_t *graph)
{
  graph->rt.nid_cnt = 0;
  graph->rt.staging_memory_type_bits = -1u;
  graph->rt.scratch_memory_type_bits = -1u;
  graph->rt.accel_memory_type_bits   = -1u;
}

void
dt_raytrace_node_cleanup(
    dt_node_t *node)
{
  if(node->rt.accel)
  {
    QVK_LOAD(vkDestroyAccelerationStructureKHR);
    qvkDestroyAccelerationStructureKHR(qvk.device, node->rt.accel, VK_NULL_HANDLE);
  }
  if(node->rt.buf_accel)   vkDestroyBuffer(qvk.device, node->rt.buf_accel,   VK_NULL_HANDLE);
  if(node->rt.buf_scratch) vkDestroyBuffer(qvk.device, node->rt.buf_scratch, VK_NULL_HANDLE);
  if(node->rt.buf_vtx)     vkDestroyBuffer(qvk.device, node->rt.buf_vtx,     VK_NULL_HANDLE);
  if(node->rt.buf_idx)     vkDestroyBuffer(qvk.device, node->rt.buf_idx,     VK_NULL_HANDLE);
  memset(&node->rt, 0, sizeof(node->rt));
}

void
dt_raytrace_graph_cleanup(
    dt_graph_t *graph)
{
  free(graph->rt.nid);
  if(graph->rt.accel)
  {
    QVK_LOAD(vkDestroyAccelerationStructureKHR);
    qvkDestroyAccelerationStructureKHR(qvk.device, graph->rt.accel, VK_NULL_HANDLE);
  }
  if(graph->rt.buf_accel)     vkDestroyBuffer(qvk.device, graph->rt.buf_accel,   VK_NULL_HANDLE);
  if(graph->rt.buf_scratch)   vkDestroyBuffer(qvk.device, graph->rt.buf_scratch, VK_NULL_HANDLE);
  if(graph->rt.buf_staging)   vkDestroyBuffer(qvk.device, graph->rt.buf_staging, VK_NULL_HANDLE);
  if(graph->rt.vkmem_scratch) vkFreeMemory   (qvk.device, graph->rt.vkmem_scratch, 0);
  if(graph->rt.vkmem_staging) vkFreeMemory   (qvk.device, graph->rt.vkmem_staging, 0);
  if(graph->rt.vkmem_accel)   vkFreeMemory   (qvk.device, graph->rt.vkmem_accel,   0);
  if(graph->rt.dset_layout)   vkDestroyDescriptorSetLayout(qvk.device, graph->rt.dset_layout, 0);
  memset(&graph->rt, 0, sizeof(graph->rt));
}

#define CREATE_BUF_R(TYPE, SZ, BUF, BITS) do { \
  if(BUF) vkDestroyBuffer(qvk.device, BUF, 0);\
  assert(SZ > 0);\
  VkBufferCreateInfo buf_info = {\
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,\
    .size  = (SZ),\
    .usage = BITS \
  };\
  QVKR(vkCreateBuffer(qvk.device, &buf_info, 0, &(BUF)));\
  VkMemoryRequirements mr;\
  vkGetBufferMemoryRequirements(qvk.device, (BUF), &mr);\
  if(mr.memoryTypeBits) graph->rt.TYPE##_memory_type_bits &= mr.memoryTypeBits;\
  size_t off = graph->rt.TYPE##_end;\
  BUF##_offset = ((off + (mr.alignment-1)) & ~(mr.alignment-1));\
  graph->rt.TYPE##_end = BUF##_offset + mr.size;\
} while(0)

#define CREATE_SCRATCH_BUF_R(SZ, BUF) CREATE_BUF_R(scratch, SZ, BUF,\
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
#define CREATE_STAGING_BUF_R(SZ, BUF) CREATE_BUF_R(staging, SZ, BUF,\
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
#define CREATE_ACCEL_BUF_R(SZ, BUF) CREATE_BUF_R(accel, SZ, BUF,\
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
    // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)

#define ALLOC_MEM_R(TYPE, BITS, memory_allocate_flags) do { \
  if(graph->rt.TYPE##_end > graph->rt.TYPE##_max) { \
    if(graph->rt.vkmem_##TYPE ) {\
      QVKLR(&qvk.queue_mutex, vkDeviceWaitIdle(qvk.device));\
      vkFreeMemory(qvk.device, graph->rt.vkmem_##TYPE, 0);\
    }\
    VkMemoryAllocateFlagsInfo allocation_flags = {\
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,\
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,\
    };\
    VkMemoryAllocateInfo mem_alloc_info = {\
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,\
      .pNext           = &allocation_flags,\
      .allocationSize  = graph->rt.TYPE##_end,\
      .memoryTypeIndex = qvk_get_memory_type(graph->rt.TYPE##_memory_type_bits, BITS )\
    };\
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->rt.vkmem_##TYPE));\
    graph->rt.TYPE##_max = graph->rt.TYPE##_end;\
}} while(0)

// called per node to determine the resource requirements of the rtgeo related buffers
static inline VkResult
dt_raytrace_node_init(
    dt_graph_t *graph,
    dt_node_t  *node)
{
  if(!qvk.raytracing_supported) return VK_SUCCESS;
  node->rt.vtx_cnt = 3, node->rt.idx_cnt = 3;
  for(int c=0;c<node->num_connectors;c++) if(node->connector[c].format == dt_token("geo"))
  { // find connector with geo:
    node->rt.vtx_cnt = node->connector[c].roi.full_wd;
    node->rt.idx_cnt = node->connector[c].roi.full_ht;
    break;
  }
  node->rt.tri_cnt = node->rt.idx_cnt/3;
  CREATE_STAGING_BUF_R(node->rt.vtx_cnt * sizeof(float) * 3, node->rt.buf_vtx);
  CREATE_STAGING_BUF_R(node->rt.idx_cnt * sizeof(uint32_t),  node->rt.buf_idx);

  VkAccelerationStructureBuildSizesInfoKHR accel_size = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  node->rt.geometry = (VkAccelerationStructureGeometryKHR) {
    .sType             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType      = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .geometry          = {
      .triangles       = {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .maxVertex     = node->rt.vtx_cnt,
        .vertexStride  = 3 * sizeof(float),
        .transformData = {0},
        .indexType     = VK_INDEX_TYPE_UINT32,
        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
      }},
    // .flags             = VK_GEOMETRY_OPAQUE_BIT_KHR,
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
  QVK_LOAD(vkDestroyAccelerationStructureKHR);
  if(node->rt.accel) qvkDestroyAccelerationStructureKHR(qvk.device, node->rt.accel, VK_NULL_HANDLE);
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

VkResult
dt_raytrace_graph_init(
    dt_graph_t *graph,
    uint32_t   *nid,
    uint32_t    nid_cnt)
{
  if(!qvk.raytracing_supported) return VK_SUCCESS;
  dt_raytrace_graph_reset(graph);
  for(int i=0;i<nid_cnt;i++)
  {
    dt_node_t *node = graph->node + nid[i];
    if(node->module->so->read_geo &&
       node->connector[0].format == dt_token("geo") &&
       dt_connector_output(node->connector))
    { // geometry for ray tracing requires an output connector "geo" and the read_geo() callback
      node->type |= s_node_geometry;
      graph->rt.nid_cnt++;
    }
  }
  if(graph->rt.nid_cnt == 0) return VK_SUCCESS;
  if(graph->rt.nid_max < graph->rt.nid_cnt)
  {
    free(graph->rt.nid);
    graph->rt.nid_max = graph->rt.nid_cnt;
    graph->rt.nid = malloc(sizeof(uint32_t)*graph->rt.nid_max);
  }
  graph->rt.nid_cnt = 0;
  for(int i=0;i<nid_cnt;i++)
  {
    dt_node_t *node = graph->node + nid[i];
    if(node->type & s_node_geometry)
      graph->rt.nid[graph->rt.nid_cnt++] = nid[i];
  }

  VkDescriptorSetLayoutBinding bindings[] = {{
    .binding         = 0,
    .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    .descriptorCount = 1,
    .stageFlags      = VK_SHADER_STAGE_ALL,
  }};
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings    = bindings,
  };
  if(graph->rt.dset_layout) vkDestroyDescriptorSetLayout(qvk.device, graph->rt.dset_layout, 0);
  QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &graph->rt.dset_layout));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    dt_raytrace_node_init(graph, graph->node + graph->rt.nid[i]);
  return VK_SUCCESS;
}

// allocates memory, creates and binds rtgeo node buffers too
VkResult
dt_raytrace_graph_alloc(
    dt_graph_t *graph)
{
  if(!qvk.raytracing_supported || graph->rt.nid_cnt == 0) return VK_SUCCESS;
  // create staging buffer for graph, allocate staging memory, bind graph + node staging:
  CREATE_STAGING_BUF_R(graph->rt.nid_cnt * sizeof(VkAccelerationStructureInstanceKHR), graph->rt.buf_staging);
  ALLOC_MEM_R(staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR);

  // bind staging buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_staging, graph->rt.vkmem_staging, graph->rt.buf_staging_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
  {
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_vtx, graph->rt.vkmem_staging, graph->node[graph->rt.nid[i]].rt.buf_vtx_offset));
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_idx, graph->rt.vkmem_staging, graph->node[graph->rt.nid[i]].rt.buf_idx_offset));
  }

  // get top-level acceleration structure size + scratch mem requirements.
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
    // .flags               = VK_GEOMETRY_OPAQUE_BIT_KHR,
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
  ALLOC_MEM_R(scratch, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR); // HEAP ?? // VK_MEMORY_HEAP_DEVICE_LOCAL_BIT

  // bind scratch buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_scratch, graph->rt.vkmem_scratch, graph->rt.buf_scratch_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_scratch, graph->rt.vkmem_scratch, graph->node[graph->rt.nid[i]].rt.buf_scratch_offset));

  // create acceleration structure buffer
  CREATE_ACCEL_BUF_R(accel_size.accelerationStructureSize, graph->rt.buf_accel);
  ALLOC_MEM_R(accel, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

  // bind accel buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_accel, graph->rt.vkmem_accel, graph->rt.buf_accel_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].rt.buf_accel, graph->rt.vkmem_accel, graph->node[graph->rt.nid[i]].rt.buf_accel_offset));

  // now create the top-level acceleration structure
  QVK_LOAD(vkCreateAccelerationStructureKHR);
  QVK_LOAD(vkDestroyAccelerationStructureKHR);
  if(graph->rt.accel) qvkDestroyAccelerationStructureKHR(qvk.device, graph->rt.accel, VK_NULL_HANDLE);
  VkAccelerationStructureCreateInfoKHR create_info = {
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = graph->rt.buf_accel,
    .offset = 0,
    .size   = accel_size.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
  };
  QVKR(qvkCreateAccelerationStructureKHR(qvk.device, &create_info, NULL, &graph->rt.accel));

  // allocate descriptor sets and point to our new accel struct
  VkDescriptorSetAllocateInfo dset_info = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool     = graph->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &graph->rt.dset_layout,
  };
  QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, graph->rt.dset));
  VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &graph->rt.accel,
  };
  VkWriteDescriptorSet dset_write[] = {{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    .descriptorCount = 1,
    .dstSet          = graph->rt.dset[0],
    .dstBinding      = 0,
    .pNext           = &acceleration_structure_info
  }};
  vkUpdateDescriptorSets(qvk.device, 1, dset_write, 0, NULL);
  return VK_SUCCESS;
}
#undef CREATE_SCRATCH_BUF_R
#undef CREATE_STAGING_BUF_R
#undef CREATE_ACCEL_BUF_R
#undef ALLOC_MEM_R

// TODO: place timers around the build commands!
// TODO: cpu side upload too?
// call this from graph_run once command buffer is ready:
VkResult
dt_raytrace_record_command_buffer_accel_build(
    dt_graph_t *graph)
{
  if(!qvk.raytracing_supported || graph->rt.nid_cnt == 0) return VK_SUCCESS;
  // XXX TODO: do not build bottom if not needed (see flags on node)
  QVK_LOAD(vkGetAccelerationStructureDeviceAddressKHR);
  QVK_LOAD(vkCmdBuildAccelerationStructuresKHR);

  VkAccelerationStructureBuildGeometryInfoKHR *build_info = alloca(sizeof(build_info[0])*graph->rt.nid_cnt);
  VkAccelerationStructureBuildRangeInfoKHR *build_range = alloca(sizeof(build_range[0])*graph->rt.nid_cnt);
  const VkAccelerationStructureBuildRangeInfoKHR **p_build_range = alloca(sizeof(p_build_range[0])*graph->rt.nid_cnt);

  uint8_t *mapped_staging = 0;
  QVKR(vkMapMemory(qvk.device, graph->rt.vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void **)&mapped_staging));
  VkAccelerationStructureInstanceKHR *instance =
    (VkAccelerationStructureInstanceKHR *)(mapped_staging + graph->rt.buf_staging_offset);
  int rebuild_cnt = 0;
  for(int i=0;i<graph->rt.nid_cnt;i++)
  { // check all nodes for ray tracing geometry
    dt_node_t *node = graph->node + graph->rt.nid[i];
    if(!(node->flags & s_module_request_read_geo)) continue;
    dt_read_geo_params_t p = (dt_read_geo_params_t) {
      .node   = node,
      .vtx    = (float    *)(mapped_staging + node->rt.buf_vtx_offset),
      .idx    = (uint32_t *)(mapped_staging + node->rt.buf_idx_offset),
    };
    if(node->module->so->read_geo) node->module->so->read_geo(node->module, &p);

    VkAccelerationStructureDeviceAddressInfoKHR address_request = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = node->rt.accel,
    };
    int ii = rebuild_cnt++;
    instance[ii] = (VkAccelerationStructureInstanceKHR) {
      .transform = { .matrix = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
      }},
      .mask  = 0xFF,
      .flags = // VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR |
        VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
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
    node->rt.build_info.dstAccelerationStructure  = node->rt.accel;
    node->rt.build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+0);
    node->rt.geometry.geometry.triangles = (VkAccelerationStructureGeometryTrianglesDataKHR) {
      .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .maxVertex     = node->rt.vtx_cnt,
      .vertexStride  = 3*sizeof(float),
      .transformData = {0},
      .indexType     = VK_INDEX_TYPE_UINT32,
      .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData    = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+1)},
      .indexData     = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+2)},
    };
    build_range  [ii] = (VkAccelerationStructureBuildRangeInfoKHR) { .primitiveCount = node->rt.tri_cnt };
    p_build_range[ii] = build_range + ii;
    build_info   [ii] = node->rt.build_info;
  }
  vkUnmapMemory(qvk.device, graph->rt.vkmem_staging);
  if(!rebuild_cnt) return VK_SUCCESS; // nothing to do, yay
  qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, rebuild_cnt, build_info, p_build_range);

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
  ACCEL_BARRIER; // barrier before top level is starting to build

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
  build_range[0] = (VkAccelerationStructureBuildRangeInfoKHR) { .primitiveCount = graph->rt.nid_cnt };
  qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, 1, &graph->rt.build_info, p_build_range);

  ACCEL_BARRIER; // push another barrier
#undef ACCEL_BARRIER
  return VK_SUCCESS;
}
