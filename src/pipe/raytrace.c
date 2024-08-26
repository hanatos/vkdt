#include "raytrace.h"
#include "graph.h"
#include "core/log.h"
#include "modules/api.h"

int dt_raytrace_present(dt_graph_t *graph)
{
  return qvk.raytracing_supported && (graph->rt[graph->double_buffer%2].nid_cnt > 0);
}

void dt_raytrace_graph_reset(dt_graph_t *graph)
{
  graph->rt[0].nid_cnt = 0;
  graph->rt[0].staging_memory_type_bits = -1u;
  graph->rt[0].scratch_memory_type_bits = -1u;
  graph->rt[0].accel_memory_type_bits   = -1u;
  graph->rt[1].nid_cnt = 0;
  graph->rt[1].staging_memory_type_bits = -1u;
  graph->rt[1].scratch_memory_type_bits = -1u;
  graph->rt[1].accel_memory_type_bits   = -1u;
}

void
dt_raytrace_node_cleanup(
    dt_node_t *node)
{
  for(int f=0;f<2;f++)
  {
    if(node->rt[f].accel)
    {
      QVK_LOAD(vkDestroyAccelerationStructureKHR);
      qvkDestroyAccelerationStructureKHR(qvk.device, node->rt[f].accel, VK_NULL_HANDLE);
    }
    if(node->rt[f].buf_accel)   vkDestroyBuffer(qvk.device, node->rt[f].buf_accel,   VK_NULL_HANDLE);
    if(node->rt[f].buf_scratch) vkDestroyBuffer(qvk.device, node->rt[f].buf_scratch, VK_NULL_HANDLE);
    if(node->rt[f].buf_vtx)     vkDestroyBuffer(qvk.device, node->rt[f].buf_vtx,     VK_NULL_HANDLE);
    if(node->rt[f].buf_idx)     vkDestroyBuffer(qvk.device, node->rt[f].buf_idx,     VK_NULL_HANDLE);
    if(node->rt[f].buf_ext)     vkDestroyBuffer(qvk.device, node->rt[f].buf_ext,     VK_NULL_HANDLE);
    memset(&node->rt[f], 0, sizeof(node->rt[f]));
  }
}

void
dt_raytrace_graph_cleanup(
    dt_graph_t *graph)
{
  for(int f=0;f<2;f++)
  {
    free(graph->rt[f].nid);
    if(graph->rt[f].accel)
    {
      QVK_LOAD(vkDestroyAccelerationStructureKHR);
      qvkDestroyAccelerationStructureKHR(qvk.device, graph->rt[f].accel, VK_NULL_HANDLE);
    }
    if(graph->rt[f].buf_accel)     vkDestroyBuffer(qvk.device, graph->rt[f].buf_accel,   VK_NULL_HANDLE);
    if(graph->rt[f].buf_scratch)   vkDestroyBuffer(qvk.device, graph->rt[f].buf_scratch, VK_NULL_HANDLE);
    if(graph->rt[f].buf_staging)   vkDestroyBuffer(qvk.device, graph->rt[f].buf_staging, VK_NULL_HANDLE);
    if(graph->rt[f].vkmem_scratch) vkFreeMemory   (qvk.device, graph->rt[f].vkmem_scratch, 0);
    if(graph->rt[f].vkmem_staging) vkFreeMemory   (qvk.device, graph->rt[f].vkmem_staging, 0);
    if(graph->rt[f].vkmem_accel)   vkFreeMemory   (qvk.device, graph->rt[f].vkmem_accel,   0);
    if(graph->rt[f].dset_layout)   vkDestroyDescriptorSetLayout(qvk.device, graph->rt[f].dset_layout, 0);
    memset(&graph->rt[f], 0, sizeof(graph->rt[f]));
  }
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
  if(mr.memoryTypeBits) graph->rt[f].TYPE##_memory_type_bits &= mr.memoryTypeBits;\
  size_t off = graph->rt[f].TYPE##_end;\
  mr.alignment = MAX(mr.alignment, qvk.raytracing_acc_min_align); \
  BUF##_offset = ((off + (mr.alignment-1)) & ~(mr.alignment-1));\
  graph->rt[f].TYPE##_end = BUF##_offset + mr.size;\
} while(0)

#define CREATE_SCRATCH_BUF_R(SZ, BUF) CREATE_BUF_R(scratch, SZ, BUF,\
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
#define CREATE_STAGING_BUF_R(SZ, BUF) CREATE_BUF_R(staging, SZ, BUF,\
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
#define CREATE_ACCEL_BUF_R(SZ, BUF) CREATE_BUF_R(accel, SZ, BUF,\
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
    // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)

#define ALLOC_MEM_R(TYPE, BITS, memory_allocate_flags) do { \
  if(graph->rt[f].TYPE##_end > graph->rt[f].TYPE##_max) { \
    if(graph->rt[f].vkmem_##TYPE ) {\
      vkFreeMemory(qvk.device, graph->rt[f].vkmem_##TYPE, 0);\
    }\
    VkMemoryAllocateFlagsInfo allocation_flags = {\
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,\
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,\
    };\
    VkMemoryAllocateInfo mem_alloc_info = {\
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,\
      .pNext           = &allocation_flags,\
      .allocationSize  = graph->rt[f].TYPE##_end,\
      .memoryTypeIndex = qvk_get_memory_type(graph->rt[f].TYPE##_memory_type_bits, BITS )\
    };\
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->rt[f].vkmem_##TYPE));\
    graph->rt[f].TYPE##_max = graph->rt[f].TYPE##_end;\
}} while(0)

// called per node to determine the resource requirements of the rtgeo related buffers
static inline VkResult
dt_raytrace_node_init(
    dt_graph_t *graph,
    dt_node_t  *node,
    const int f)
{
  if(!qvk.raytracing_supported) return VK_SUCCESS;
  node->rt[f].vtx_cnt = 3, node->rt[f].idx_cnt = 3;
  for(int c=0;c<node->num_connectors;c++) if(node->connector[c].format == dt_token("geo"))
  { // find connector with geo:
    node->rt[f].vtx_cnt = node->connector[c].roi.full_wd;
    node->rt[f].idx_cnt = node->connector[c].roi.full_ht;
    break;
  }
  node->rt[f].tri_cnt = node->rt[f].idx_cnt/3;
  if(node->rt[f].tri_cnt == 0) return VK_SUCCESS;
  CREATE_STAGING_BUF_R(node->rt[f].vtx_cnt * sizeof(float) * 3,    node->rt[f].buf_vtx);
  CREATE_STAGING_BUF_R(node->rt[f].idx_cnt * sizeof(uint32_t),     node->rt[f].buf_idx);
  CREATE_STAGING_BUF_R(node->rt[f].tri_cnt * sizeof(int16_t) * 14, node->rt[f].buf_ext);

  VkAccelerationStructureBuildSizesInfoKHR accel_size = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  node->rt[f].geometry = (VkAccelerationStructureGeometryKHR) {
    .sType             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType      = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .geometry          = {
      .triangles       = {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .maxVertex     = node->rt[f].vtx_cnt-1,
        .vertexStride  = 3 * sizeof(float),
        .transformData = {0},
        .indexType     = VK_INDEX_TYPE_UINT32,
        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
      }},
    // .flags             = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };
  node->rt[f].build_info = (VkAccelerationStructureBuildGeometryInfoKHR) {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries   = &node->rt[f].geometry,
  };
  QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
  qvkGetAccelerationStructureBuildSizesKHR(
      qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &node->rt[f].build_info, &node->rt[f].tri_cnt, &accel_size);

  // create scratch buffer and accel struct backing buffer
  CREATE_SCRATCH_BUF_R(accel_size.buildScratchSize, node->rt[f].buf_scratch);
  CREATE_ACCEL_BUF_R(accel_size.accelerationStructureSize, node->rt[f].buf_accel);

  // create acceleration struct
  QVK_LOAD(vkCreateAccelerationStructureKHR);
  QVK_LOAD(vkDestroyAccelerationStructureKHR);
  if(node->rt[f].accel) qvkDestroyAccelerationStructureKHR(qvk.device, node->rt[f].accel, VK_NULL_HANDLE);
  VkAccelerationStructureCreateInfoKHR create_info = {
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = node->rt[f].buf_accel,
    .offset = 0,
    .size   = accel_size.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
  };
  QVKR(qvkCreateAccelerationStructureKHR(qvk.device, &create_info, NULL, &node->rt[f].accel));
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
  for(int f=0;f<2;f++)
  {
    for(int i=0;i<nid_cnt;i++)
    {
      dt_node_t *node = graph->node + nid[i];
      if(node->module->so->read_geo &&
         node->connector[0].format == dt_token("geo") &&
         dt_connector_output(node->connector))
      { // geometry for ray tracing requires an output connector "geo" and the read_geo() callback
        node->type |= s_node_geometry;
        graph->rt[f].nid_cnt++;
      }
    }
    if(graph->rt[f].nid_cnt == 0) return VK_SUCCESS;
    if(graph->rt[f].nid_max < graph->rt[f].nid_cnt)
    {
      free(graph->rt[f].nid);
      graph->rt[f].nid_max = graph->rt[f].nid_cnt;
      graph->rt[f].nid = malloc(sizeof(uint32_t)*graph->rt[f].nid_max);
    }
    graph->rt[f].nid_cnt = 0;
    for(int i=0;i<nid_cnt;i++)
    {
      dt_node_t *node = graph->node + nid[i];
      if(node->type & s_node_geometry)
        graph->rt[f].nid[graph->rt[f].nid_cnt++] = nid[i];
    }

    VkDescriptorSetLayoutBinding bindings[] = {{
      .binding         = 0,
      .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
      .descriptorCount = 1,
      .stageFlags      = VK_SHADER_STAGE_ALL,
    },{
      .binding         = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = graph->rt[f].nid_cnt,
      .stageFlags      = VK_SHADER_STAGE_ALL,
    },{
      .binding         = 2,
      .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = graph->rt[f].nid_cnt,
      .stageFlags      = VK_SHADER_STAGE_ALL,
    },{
      .binding         = 3,
      .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = graph->rt[f].nid_cnt,
      .stageFlags      = VK_SHADER_STAGE_ALL,
    }};
    VkDescriptorSetLayoutCreateInfo dset_layout_info = {
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 4,
      .pBindings    = bindings,
    };
    if(graph->rt[f].dset_layout) vkDestroyDescriptorSetLayout(qvk.device, graph->rt[f].dset_layout, 0);
    QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &graph->rt[f].dset_layout));
    for(int i=0;i<graph->rt[f].nid_cnt;i++)
      dt_raytrace_node_init(graph, graph->node + graph->rt[f].nid[i], f);
  }
  return VK_SUCCESS;
}

// allocates memory, creates and binds rtgeo node buffers too
VkResult
dt_raytrace_graph_alloc(
    dt_graph_t *graph)
{
  for(int f=0;f<2;f++)
  {
  if(!qvk.raytracing_supported || graph->rt[f].nid_cnt == 0) return VK_SUCCESS;
  // create staging buffer for graph, allocate staging memory, bind graph + node staging:
  CREATE_STAGING_BUF_R(graph->rt[f].nid_cnt * sizeof(VkAccelerationStructureInstanceKHR), graph->rt[f].buf_staging);
  ALLOC_MEM_R(staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR);

  // bind staging buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt[f].buf_staging, graph->rt[f].vkmem_staging, graph->rt[f].buf_staging_offset));
  for(int i=0;i<graph->rt[f].nid_cnt;i++)
  {
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt[f].nid[i]].rt[f].buf_vtx, graph->rt[f].vkmem_staging, graph->node[graph->rt[f].nid[i]].rt[f].buf_vtx_offset));
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt[f].nid[i]].rt[f].buf_idx, graph->rt[f].vkmem_staging, graph->node[graph->rt[f].nid[i]].rt[f].buf_idx_offset));
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt[f].nid[i]].rt[f].buf_ext, graph->rt[f].vkmem_staging, graph->node[graph->rt[f].nid[i]].rt[f].buf_ext_offset));
  }

  // get top-level acceleration structure size + scratch mem requirements.
  VkAccelerationStructureBuildSizesInfoKHR accel_size = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  graph->rt[f].geometry = (VkAccelerationStructureGeometryKHR) {
    .sType               = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType        = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .geometry            = {
      .instances         = {
        .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
      }},
    // .flags               = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };
  graph->rt[f].build_info = (VkAccelerationStructureBuildGeometryInfoKHR) {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries   = &graph->rt[f].geometry,
  };
  QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
  qvkGetAccelerationStructureBuildSizesKHR(
      qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &graph->rt[f].build_info, &graph->rt[f].nid_cnt, &accel_size);

  CREATE_SCRATCH_BUF_R(accel_size.buildScratchSize, graph->rt[f].buf_scratch);
  ALLOC_MEM_R(scratch, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR); // HEAP ?? // VK_MEMORY_HEAP_DEVICE_LOCAL_BIT

  // bind scratch buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt[f].buf_scratch, graph->rt[f].vkmem_scratch, graph->rt[f].buf_scratch_offset));
  for(int i=0;i<graph->rt[f].nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt[f].nid[i]].rt[f].buf_scratch, graph->rt[f].vkmem_scratch, graph->node[graph->rt[f].nid[i]].rt[f].buf_scratch_offset));

  // create acceleration structure buffer
  CREATE_ACCEL_BUF_R(accel_size.accelerationStructureSize, graph->rt[f].buf_accel);
  ALLOC_MEM_R(accel, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

  // bind accel buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt[f].buf_accel, graph->rt[f].vkmem_accel, graph->rt[f].buf_accel_offset));
  for(int i=0;i<graph->rt[f].nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt[f].nid[i]].rt[f].buf_accel, graph->rt[f].vkmem_accel, graph->node[graph->rt[f].nid[i]].rt[f].buf_accel_offset));

  // now create the top-level acceleration structure
  QVK_LOAD(vkCreateAccelerationStructureKHR);
  QVK_LOAD(vkDestroyAccelerationStructureKHR);
  if(graph->rt[f].accel) qvkDestroyAccelerationStructureKHR(qvk.device, graph->rt[f].accel, VK_NULL_HANDLE);
  VkAccelerationStructureCreateInfoKHR create_info = {
    .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = graph->rt[f].buf_accel,
    .offset = 0,
    .size   = accel_size.accelerationStructureSize,
    .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
  };
  QVKR(qvkCreateAccelerationStructureKHR(qvk.device, &create_info, NULL, &graph->rt[f].accel));

  // allocate descriptor sets and point to our new accel struct
  VkDescriptorSetAllocateInfo dset_info = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool     = graph->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &graph->rt[f].dset_layout,
  };
  QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &graph->rt[f].dset));
  VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &graph->rt[f].accel,
  };
  VkDescriptorBufferInfo *bvtx = alloca(sizeof(VkDescriptorBufferInfo)*graph->rt[f].nid_cnt);
  VkDescriptorBufferInfo *bidx = alloca(sizeof(VkDescriptorBufferInfo)*graph->rt[f].nid_cnt);
  VkDescriptorBufferInfo *bext = alloca(sizeof(VkDescriptorBufferInfo)*graph->rt[f].nid_cnt);
  for(int i=0;i<graph->rt[f].nid_cnt;i++)
  {
    bvtx[i] = (VkDescriptorBufferInfo){
      .offset = 0,
      .buffer = graph->node[graph->rt[f].nid[i]].rt[f].buf_vtx,
      .range  = VK_WHOLE_SIZE,
    };
    bidx[i] = (VkDescriptorBufferInfo){
      .offset = 0,
      .buffer = graph->node[graph->rt[f].nid[i]].rt[f].buf_idx,
      .range  = VK_WHOLE_SIZE,
    };
    bext[i] = (VkDescriptorBufferInfo){
      .offset = 0,
      .buffer = graph->node[graph->rt[f].nid[i]].rt[f].buf_ext,
      .range  = VK_WHOLE_SIZE,
    };
  }
  VkWriteDescriptorSet dset_write[] = {{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    .descriptorCount = 1,
    .dstSet          = graph->rt[f].dset,
    .dstBinding      = 0,
    .pNext           = &acceleration_structure_info
  },{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = graph->rt[f].nid_cnt,
    .dstSet          = graph->rt[f].dset,
    .dstBinding      = 1,
    .pBufferInfo     = bvtx,
  },{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = graph->rt[f].nid_cnt,
    .dstSet          = graph->rt[f].dset,
    .dstBinding      = 2,
    .pBufferInfo     = bidx,
  },{
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = graph->rt[f].nid_cnt,
    .dstSet          = graph->rt[f].dset,
    .dstBinding      = 3,
    .pBufferInfo     = bext,
  }};
  vkUpdateDescriptorSets(qvk.device, 4, dset_write, 0, NULL);
  }
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
  const int f  =  graph->double_buffer % 2;
  const int fp = (graph->double_buffer+1) % 2;
  if(!qvk.raytracing_supported || graph->rt[f].nid_cnt == 0) return VK_SUCCESS;
  QVK_LOAD(vkGetAccelerationStructureDeviceAddressKHR);
  QVK_LOAD(vkCmdBuildAccelerationStructuresKHR);

  VkAccelerationStructureBuildGeometryInfoKHR *build_info = alloca(sizeof(build_info[0])*graph->rt[f].nid_cnt);
  VkAccelerationStructureBuildRangeInfoKHR *build_range = alloca(sizeof(build_range[0])*graph->rt[f].nid_cnt);
  const VkAccelerationStructureBuildRangeInfoKHR **p_build_range = alloca(sizeof(p_build_range[0])*graph->rt[f].nid_cnt);

  uint8_t *mapped_staging = 0;
  QVKR(vkMapMemory(qvk.device, graph->rt[f].vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void **)&mapped_staging));
  VkAccelerationStructureInstanceKHR *instance =
    (VkAccelerationStructureInstanceKHR *)(mapped_staging + graph->rt[f].buf_staging_offset);
  int rebuild_cnt = 0;
  for(int i=0;i<graph->rt[f].nid_cnt;i++)
  { // check all nodes for ray tracing geometry
    dt_node_t *node = graph->node + graph->rt[f].nid[i];
    if(!node->rt[f].force_read_geo && !(node->flags & s_module_request_read_geo)) continue;
    dt_read_geo_params_t p = (dt_read_geo_params_t) {
      .node   = node,
      .vtx    = (float    *)(mapped_staging + node->rt[f].buf_vtx_offset),
      .idx    = (uint32_t *)(mapped_staging + node->rt[f].buf_idx_offset),
      .ext    = (int16_t  *)(mapped_staging + node->rt[f].buf_ext_offset),
    };
    if(node->module->so->read_geo) node->module->so->read_geo(node->module, &p);
    // flag has been cleared, node says it's done! need to make sure the other frame buffer knows the static geo too!
    if(!(node->flags & s_module_request_read_geo) && !node->rt[f].force_read_geo) node->rt[fp].force_read_geo = 1;
    node->rt[f].force_read_geo = 0; // we are done now
    if(node->rt[f].vtx_cnt == 0) continue;

    VkAccelerationStructureDeviceAddressInfoKHR address_request = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = node->rt[f].accel,
    };
    int ii = rebuild_cnt++;
    instance[ii] = (VkAccelerationStructureInstanceKHR) {
      .transform = { .matrix = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
      }},
      .mask  = 0xFF,
      // .flags = // VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR |
        // VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
      .accelerationStructureReference = qvkGetAccelerationStructureDeviceAddressKHR(qvk.device, &address_request),
    };
    VkBufferDeviceAddressInfo address_info[] = {{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt[f].buf_scratch,
    },{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt[f].buf_vtx,
    },{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt[f].buf_idx,
    }};
    node->rt[f].build_info.dstAccelerationStructure  = node->rt[f].accel;
    node->rt[f].build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+0);
    node->rt[f].geometry.geometry.triangles = (VkAccelerationStructureGeometryTrianglesDataKHR) {
      .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .maxVertex     = node->rt[f].vtx_cnt-1,
      .vertexStride  = 3*sizeof(float),
      .transformData = {0},
      .indexType     = VK_INDEX_TYPE_UINT32,
      .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData    = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+1)},
      .indexData     = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, address_info+2)},
    };
    build_range  [ii] = (VkAccelerationStructureBuildRangeInfoKHR) { .primitiveCount = node->rt[f].tri_cnt };
    p_build_range[ii] = build_range + ii;
    build_info   [ii] = node->rt[f].build_info;
  }
  vkUnmapMemory(qvk.device, graph->rt[f].vkmem_staging);
  if(!rebuild_cnt) return VK_SUCCESS; // nothing to do, yay
  VkMemoryBarrier barrier = {
    .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
  };
  vkCmdPipelineBarrier(graph->command_buffer[f],
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      0, 1, &barrier, 0, NULL, 0, NULL);
  qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer[f], rebuild_cnt, build_info, p_build_range);

  // barrier before top level is starting to build
  barrier = (VkMemoryBarrier){
    .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
  };
  vkCmdPipelineBarrier(graph->command_buffer[f],
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      0, 1, &barrier, 0, NULL, 0, NULL);

  // build top level accel
  VkBufferDeviceAddressInfo scratch_adress_info = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt[f].buf_scratch,
  };
  VkBufferDeviceAddressInfo index_address = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt[f].buf_staging,
  };
  graph->rt[f].build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, &scratch_adress_info);
  graph->rt[f].build_info.dstAccelerationStructure  = graph->rt[f].accel;
  graph->rt[f].geometry.geometry.instances = (VkAccelerationStructureGeometryInstancesDataKHR) {
    .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
    .arrayOfPointers = VK_FALSE,
    .data            = { .deviceAddress = vkGetBufferDeviceAddress(qvk.device, &index_address) },
  };
  build_range[0] = (VkAccelerationStructureBuildRangeInfoKHR) { .primitiveCount = graph->rt[f].nid_cnt };
  qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer[f], 1, &graph->rt[f].build_info, p_build_range);

  // push another barrier
  barrier = (VkMemoryBarrier){
    .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
  };
  vkCmdPipelineBarrier(graph->command_buffer[f],
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      0, 1, &barrier, 0, NULL, 0, NULL);
  return VK_SUCCESS;
}
