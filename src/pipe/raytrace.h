/// alloc_outputs:
/// * create pipeline
/// * create buffer + memory requirements + allocate in our alloc system
/// * create image
/// * TODO: collect ray tracing requirements

// ==> here the graph allocates the vkmem heap
// TODO: allocate 3x new heaps: with exactly the optimised flags for our staging, accel, and scratch memory (it's not like we'll share it anyways)

/// alloc_outputs2:
/// * bind memory to buffers
/// * create image views

/// alloc_outputs3:
/// * alloc descriptor sets
/// * bind staging memory to buffers
/// * create framebuffers for raster pipelines
/// * TODO: { .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR

/// record command buffer:
// ?? some ray tracing related things needed or just use ray query extensions in compute shaders and bind an extra descriptor set

// TODO: global:
// load_geo() callback?
// VkDeviceMemory on graph: rt_staging, rt_accel, rt_scratch
// keep track of number of rtgeos on graph, as well as sum of their buffer requests for the three rt vkmem types
/// =====================================================================================================================
  // need the following buffers:
  // * staging, one per accel struct (x nodes + graph) with VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
  // * for the actual accel struct VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  // * scratch memory for buiding	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
  // all of these should be persistent, except maybe the scratch memory (how much is that)
  // collect and allocate as we do, with special types probably for all three of them
/// =====================================================================================================================


/// XXX TODO: cleanup
/// XXX TODO: memory map instances
/// XXX TODO: read_source() equivalent for geometry on rtgeo nodes, memory map the staging buffers and upload!
/// XXX TODO: bind descriptor sets in create_nodes or so! for all nodes with raytrace flag

// extra storage on graph
typedef struct dt_raytrace_graph_t
{
  VkAccelerationStructureKHR                  accel;          // for ray tracing: top level for all nodes that may hold bottom level
  VkBuffer                                    buf_accel;      // memory for actual accel
  VkBuffer                                    buf_scratch;    // scratch memory for accel build
  VkBuffer                                    buf_staging;    // the buffer that goes with the rt_instances
  off_t                                       accel_offset;   // where the buffer is bound inside the vkmem
  off_t                                       scratch_offset;
  off_t                                       staging_offset;
  VkAccelerationStructureInstanceKHR         *instance;       // instances to point to nodes providing rt geo (mem mapped version of staging)
  int                        rt_instance_end, instance_max;   // currently inited/allocated instances
  VkAccelerationStructureBuildGeometryInfoKHR build_info;
  VkDeviceMemory                              vkmem_scratch;
  VkDeviceMemory                              vkmem_staging;  // memory for our instance array + all nodes vtx, idx data
  VkDeviceMemory                              vkmem_accel;    // memory for our top level accel + all nodes bottom level accel
  size_t                                      scratch_end, scratch_max;
  size_t                                      staging_end, staging_max;
  size_t                                      accel_end,   accel_max;
  VkDescriptorSetLayout                       dset_layout;
  int                                        *nid;
  int                                         nid_cnt;
}
dt_raytrace_graph_t;

// extra storage on node
typedef struct dt_raytrace_node_t
{
  VkAccelerationStructureKHR                  accel;          // needed for ray tracing kernels: bottom level structure
  VkAccelerationStructureBuildGeometryInfoKHR build_info;     // geometry info
  VkBuffer                                    buf_accel;      // buffer to hold accel struct
  VkBuffer                                    buf_scratch;    // scratch memory for accel build
  VkBuffer                                    buf_vtx;        // vertex buffer
  VkBuffer                                    buf_idx;        // index buffer
  off_t                                       accel_offset;
  off_t                                       scratch_offset;
  off_t                                       vtx_offset;
  off_t                                       idx_offset;
  uint32_t                                    vtx_cnt;        // number of vertices provided
  uint32_t                                    idx_cnt;        // number of indices provided by this node
  uint32_t                                    tri_cnt;        // number of indices provided by this node, i.e. idx_cnt/3
}
dt_raytrace_node_t;

static inline VkResult
dt_raytrace_graph_init(
    dt_graph_t *graph,
    int        *nid,
    int         nid_cnt)
{
  graph->nid_cnt = 0;
  for(int i=0;i<nid_cnt;i++)
    if(graph->node[nid[i]].type & s_node_geometry)
      graph->nid[graph->nid_cnt++] = i;

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

  // TODO: go through nodes and request their sizes
  for(int i=0;i<graph->nid_cnt;i++)
    dt_raytrace_node_init(graph, graph->node + graph->nid[i]);
}

static inline void
dt_raytrace_node_cleanup(
    dt_node_t *node)
{
}

// cleanup all ray tracing resources
static inline void
dt_raytrace_graph_cleanup(
    dt_graph_t *graph)
{
	QVK_LOAD(vkDestroyAccelerationStructureKHR)
// TODO: destroy stuff:
    // top level on graph
    // bottom level on all nodes that have it
    // destroy buffers and deallocate the 3x vkmem we have reserved

    // TODO: destroy descriptor set layout

	if (structure->top_level) qvkDestroyAccelerationStructureKHR(device->device, structure->top_level, NULL);
	if (structure->bottom_level) qvkDestroyAccelerationStructureKHR(device->device, structure->bottom_level, NULL);
	destroy_buffers(&structure->buffers, device);

  free(graph->rt_instance)
  graph->rt_instance_end = graph->rt_instance_max = 0;
}

// called per node to determine the resource requirements of the rtgeo related buffers
static inline VkResult
dt_raytrace_node_init(
    dt_graph_t *graph,
    dt_node_t  *node)
{
  // on node, get size for bottom level accel struct, allocate buffer for mesh
	VkBufferCreateInfo buf_info[] = {{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = node->rt.vtx_cnt * sizeof(float) * 3,
    .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  },{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = node->rt.idx_cnt * sizeof(int)
    .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  }};
  QVKR(vkCreateBuffer(qvk.device, buf_info[0], 0, &node->rt.buf_vtx);
  QVKR(vkCreateBuffer(qvk.device, buf_info[1], 0, &node->rt.buf_idx);
  VkMemoryRequirements mr0, mr1;
  vkGetBufferMemoryRequirements(qvk.device, node->rt.buf_vtx, &mr0);
  vkGetBufferMemoryRequirements(qvk.device, node->rt.buf_idx, &mr1);
  graph->staging_memory_type_bits |= mr0.memoryTypeBits;
  graph->staging_memory_type_bits |= mr1.memoryTypeBits;
  // move graph->rt.staging_end according to alignment + size,
  // store buffer offset for later binding.
  size_t off = graph->rt.staging_end;
  node->rt.vtx_offset = ((off + (mr0.alignment-1)) & ~(mr0.alignment-1));
  off = node->rt.vtx_offset + mr0.size;
  node->rt.idx_offset = ((off + (mr1.alignment-1)) & ~(mr1.alignment-1));
  graph->rt.staging_end = node->rt.idx_offset + mr1.size;

	VkAccelerationStructureBuildSizesInfoKHR accel_size = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};
	VkAccelerationStructureGeometryKHR accel_geometry = {
		.sType             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType      = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry          = {
			.triangles       = {
				.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
				.maxVertex     = node->rt.vtx_cnt - 1,
				.vertexStride  = 3 * sizeof(float), // XXX set to 4 if we have the normal in between
        .transformData = 0,
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
    .pGeometries   = &accel_geometry,
	};
	QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
	qvkGetAccelerationStructureBuildSizesKHR(
		qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&node->rt.build_info, &node->rt.tri_cnt, &accel_size);

  // create scratch buffer
	VkBufferCreateInfo scratch_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = accel_size.buildScratchSize,
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
  };
  QVKR(vkCreateBuffer(qvk.device, scratch_info, 0, &node->rt.buf_scratch);
  vkGetBufferMemoryRequirements(qvk.device, node->rt.scratch, &mr0);
  graph->scratch_memory_type_bits |= mr0.memoryTypeBits;
  off = graph->rt.scratch_end;
  node->rt.scratch_offset = ((off + (mr0.alignment-1)) & ~(mr0.alignment-1));
  graph->rt.scratch_end = node->rt.scratch_offset + mr0.size;

  // create acceleration struct backing buffer
	VkBufferCreateInfo accel_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = accel_size.accelerationStructureSize,
    .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
  };
  QVKR(vkCreateBuffer(qvk.device, accel_info, 0, &node->rt.buf_accel);
  vkGetBufferMemoryRequirements(qvk.device, node->rt.buf_accel, &mr0);
  graph->accel_memory_type_bits |= mr0.memoryTypeBits;
  off = graph->rt.accel_end;
  node->rt.accel_offset = ((off + (mr0.alignment-1)) & ~(mr0.alignment-1));
  graph->rt.accel_end = node->rt.accel_offset + mr0.size;

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

/// =====================================================================================================================
/// build the acceleration structures and alloc/free scratch mem
/// do this in graph_run() if a rebuild is needed (and only then, see module flags!)

// allocates memory, creates and binds rtgeo node buffers too
static inline VkResult
dt_raytrace_graph_alloc(
    dt_graph_t *graph)
{
  // create staging buffer for graph, allocate staging memory, bind graph + node staging:
	VkBufferCreateInfo buf_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = graph->nid_cnt * sizeof(VkAccelerationStructureInstanceKHR),
    .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  };
  QVKR(vkCreateBuffer(qvk.device, buf_info, 0, &graph->rt.buf_staging);
  VkMemoryRequirements mr;
  graph->staging_memory_type_bits |= mr.memoryTypeBits;
  vkGetBufferMemoryRequirements(qvk.device, graph->rt.buf_staging, &mr);
  // move graph->rt.staging_end according to alignment + size,
  // store buffer offset for later binding.
  size_t off = graph->rt.staging_end;
  graph->rt.staging_offset = ((off + (mr.alignment-1)) & ~(mr.alignment-1));
  graph->rt.staging_end = graph->rt.staging_offset + mr.size;

  if(graph->rt.staging_end > graph->rt.staging_max)
  { // allocate staging memory for ray tracing
    // XXX need to issue re-upload of geometry!
    if(graph->rt.vkmem_staging)
    {
      QVKR(vkDeviceWaitIdle(qvk.device));
      vkFreeMemory(qvk.device, graph->rt.vkmem_staging, 0);
    }
    VkMemoryAllocateInfo mem_alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->rt.staging_end,
      .memoryTypeIndex = qvk_get_memory_type(graph->rt.staging_memory_type_bits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->rt.vkmem_staging));
    graph->rt.staging_max = graph->rt.staging_end;
  }

  // bind staging buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.staging, graph->vkmem_staging, graph->rt.staging_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
  {
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].vtx, graph->rt.vkmem_staging, graph->node[graph->rt.nid[i]].vtx_offset));
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].idx, graph->rt.vkmem_staging, graph->node[graph->rt.nid[i]].idx_offset));
  }


  // get top-level acceleration structure size + scratch mem requirements.
  // create scratch buffer for graph and allocation, bind scratch buffers of graph and nodes.
	VkAccelerationStructureBuildSizesInfoKHR accel_size = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};
	VkAccelerationStructureGeometryKHR geometry = {
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
    .pGeometries   = &geometry,
	};
	QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
	qvkGetAccelerationStructureBuildSizesKHR(
		qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&graph->rt.build_info, &graph->rt.nid_cnt, &accel_size);

	VkBufferCreateInfo scratch_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = accel_size.buildScratchSize,
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
  };
  QVKR(vkCreateBuffer(qvk.device, scratch_info, 0, &graph->rt.buf_scratch);
  vkGetBufferMemoryRequirements(qvk.device, graph->rt.buf_scratch, &mr0);
  graph->scratch_memory_type_bits |= mr0.memoryTypeBits;
  off = graph->rt.scratch_end;
  graph->rt.scratch_offset = ((off + (mr0.alignment-1)) & ~(mr0.alignment-1));
  graph->rt.scratch_end = graph->rt.scratch_offset + mr0.size;

  if(graph->rt.scratch_end > graph->rt.scratch_max)
  { // allocate scratch memory for accel struct build
    if(graph->rt.vkmem_scratch)
    {
      QVKR(vkDeviceWaitIdle(qvk.device));
      vkFreeMemory(qvk.device, graph->rt.vkmem_scratch, 0);
    }
    VkMemoryAllocateInfo mem_alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->rt.scratch_end,
      .memoryTypeIndex = qvk_get_memory_type(graph->rt.scratch_memory_type_bits,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->rt.vkmem_scratch));
    graph->rt.scratch_max = graph->rt.scratch_end;
  }

  // bind scratch buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.scratch, graph->vkmem_scratch, graph->rt.scratch_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].scratch, graph->rt.vkmem_scratch, graph->node[graph->rt.nid[i]].scratch_offset));


  // create acceleration structure buffer
	buf_info = (VkBufferCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = accel_size.accelerationStructureSize,
    .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
	};
  QVKR(vkCreateBuffer(qvk.device, scratch_info, 0, &graph->rt.buf_accel);
  vkGetBufferMemoryRequirements(qvk.device, graph->rt.buf_accel, &mr0);
  graph->accel_memory_type_bits |= mr0.memoryTypeBits;
  off = graph->rt.accel_end;
  graph->rt.accel_offset = ((off + (mr0.alignment-1)) & ~(mr0.alignment-1));
  graph->rt.accel_end = graph->rt.accel_offset + mr0.size;

  if(graph->rt.accel_end > graph->rt.accel_max)
  { // allocate accel memory for accel struct build
    if(graph->rt.vkmem_accel)
    {
      QVKR(vkDeviceWaitIdle(qvk.device));
      vkFreeMemory(qvk.device, graph->rt.vkmem_accel, 0);
    }
    VkMemoryAllocateInfo mem_alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->rt.accel_end,
      .memoryTypeIndex = qvk_get_memory_type(graph->rt.accel_memory_type_bits,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->rt.vkmem_accel));
    graph->rt.accel_max = graph->rt.accel_end;
  }

  // bind accel buffers to the allocation
  QVKR(vkBindBufferMemory(qvk.device, graph->rt.buf_accel, graph->vkmem_accel, graph->rt.accel_offset));
  for(int i=0;i<graph->rt.nid_cnt;i++)
    QVKR(vkBindBufferMemory(qvk.device, graph->node[graph->rt.nid[i]].buf_accel, graph->rt.vkmem_accel, graph->node[graph->rt.nid[i]].accel_offset));

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





/// =====================================================================================================================
/// call this from graph_run once command buffer is ready:
static inline void
raytrace_record_command_buffer_accel_build(
    graph_t    *graph,
    const int  *nodeid,
    const int   node_cnt)
{
	QVK_LOAD(vkGetAccelerationStructureDeviceAddressKHR)
	QVK_LOAD(vkCmdBuildAccelerationStructuresKHR)

  // XXX TODO: also create vkCreateDescriptorSetLayout for the accel bindings (set 2 or 3 so?)

  // TODO: memory map graph->rt_instance here?
  // XXX TODO: do not build bottom if not needed (see flags on module)
  graph->rt_instance_end = 0;
  for(int i=0;i<node_cnt;i++)
  { // check all nodes for ray tracing geometry
    dt_node_t *node = graph->node + nodeid[i];
    if(!(node->type & s_node_geometry)) continue;
    VkAccelerationStructureDeviceAddressInfoKHR address_request = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = node->rt.accel,
    };
    VkAccelerationStructureInstanceKHR instance = {
      .transform = { .matrix = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
      }},
      .mask  = 0xFF,
      .flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
      .accelerationStructureReference = qvkGetAccelerationStructureDeviceAddressKHR(qvk.device, &address_request),
    };
    // TODO: make sure < rt_instance_max?
    memcpy(graph->rt_instance + graph->rt_instance_end++, &instance, sizeof(instance));
    VkAccelerationStructureBuildRangeInfoKHR build_ranges[] = {{
		  .primitiveCount = node->rt.tri_cnt }};
		VkBufferDeviceAddressInfo scratch_adress_info = {
			.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = node->rt_scratch,
		};
    VkBufferDeviceAddressInfo index_address = {
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt.idx,
    };
    VkBufferDeviceAddressInfo vertex_address = {
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt.vtx,
    };
    // could batch up the build_info and build_ranges and call vkBuildAccelerationStructuresKHR only once:
		node->rt_build_info.dstAccelerationStructure  = node->rt_accel;
		node->rt_build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, &scratch_adress_info);
		node->rt_build_info.pGeometries[0].geometry.triangles.vertexData = vkGetBufferDeviceAddress(qvk.device, &vertex_address);
		node->rt_build_info.pGeometries[0].geometry.triangles.indexData  = vkGetBufferDeviceAddress(qvk.device, &index_address);
    const VkAccelerationStructureBuildRangeInfoKHR build_range = {
      .primitiveCount = node->rt.tri_cnt };
		qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, 1, &node->rt.build_info, &&build_range);
  }

  // barrier before top level is starting to build:
  VkMemoryBarrier barrier = {
    .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
  };
  vkCmdPipelineBarrier(graph->command_buffer,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      0, 1, &barrier, 0, NULL, 0, NULL);

  // build top level accel
	VkAccelerationStructureBuildRangeInfoKHR build_range = {
		.primitiveCount = graph->rt_instance_end };
  VkBufferDeviceAddressInfo scratch_adress_info = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt.scratch,
  };
  VkBufferDeviceAddressInfo index_address = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt.staging,
  };
  graph->rt_build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, &scratch_adress_info);
  graph->rt_build_info.dstAccelerationStructure  = graph->rt_accel;
  graph->rt_build_info.pGeometries[0].geometry.instances.data = vkGetBufferDeviceAddress(qvk.device, &index_address);
  qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, 1, &graph->rt_build_info, &&build_range);

  // push another barrier
  VkMemoryBarrier barrier2 = {
    .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
  };
  vkCmdPipelineBarrier(graph->command_buffer,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      0, 1, &barrier2, 0, NULL, 0, NULL);

  // TODO: i suppose we could now destroy the staging buffers
}
