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



raytrace_cleanup()
{
	QVK_LOAD(vkDestroyAccelerationStructureKHR)
// TODO: destroy stuff:
    // top level on graph
    // bottom level on all nodes that have it
    // destroy buffers and deallocate the 3x vkmem we have reserved

	if (structure->top_level) qvkDestroyAccelerationStructureKHR(device->device, structure->top_level, NULL);
	if (structure->bottom_level) qvkDestroyAccelerationStructureKHR(device->device, structure->bottom_level, NULL);
	destroy_buffers(&structure->buffers, device);

  free(graph->rt_instance)
  graph->rt_instance_end = graph->rt_instance_max = 0;
}



/// =====================================================================================================================
raytrace_alloc_outputs()
{
/// =====================================================================================================================
/// alloc_outputs()
/// on node, get size for bottom level accel struct, allocate buffer for mesh
  // TODO: this amounts to recording rt_tri_cnt on the node
	// Create a buffer for the dequantized triangle mesh
	VkBufferCreateInfo staging_infos[2] = {{ // XXX this is for the node/bottom level
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = mesh->triangle_count * sizeof(float) * 3 * 3,
    .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  },{ // XXX this is for the graph/top level
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = sizeof(VkAccelerationStructureInstanceKHR),  // XXX this should be * number of bottom level nodes providing geo
    .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  }};
#if 0
/// the buffer creation should be done by the graph after running
	buffers_t staging;
	uint8_t* staging_data;
	if (create_buffers(&staging, device, staging_infos, 2, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		|| vkMapMemory(device->device, staging.memory, 0, staging.size, 0, (void**) &staging_data))
	{
		printf("Failed to allocate and map a buffer for dequantized mesh data (%llu triangles) to create an acceleration structure.\n", mesh->triangle_count);
		destroy_buffers(&staging, device);
		destroy_acceleration_structure(structure, device);
		return 1;
	}
	// XXX Dequantize the mesh data
#endif
	// Figure out how big the buffers for the bottom-level need to be
	uint32_t primitive_count = (uint32_t) mesh->triangle_count;
	VkAccelerationStructureBuildSizesInfoKHR bottom_sizes = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};
  // XXX TODO: this one here we need further down when actually building the accel struct:
	// VkBufferDeviceAddressInfo vertices_address = {
	// 	.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
	// 	.buffer = staging.buffers[0].buffer
	// };
  // XXX all of this needs adjusting to our layout:
	VkAccelerationStructureGeometryKHR bottom_geometry = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry = {
			.triangles = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        // XXX put this back here after the allocation has finished and before we use the bottom_build_info to actually construct the thing!
				// .vertexData = { .deviceAddress = vkGetBufferDeviceAddress(device->device, &vertices_address) }, // luckily it seems this will be ignored by GetAccelerationStructureBuildSizesKHR :)
				.maxVertex = primitive_count * 3 - 1,
				.vertexStride = 3 * sizeof(float), // set to 4 if we have the normal in between
        .transformData = 0,
        // .indexData = 0, // XXX fill with triangle list later on when actually building!
				.indexType = VK_INDEX_TYPE_NONE_KHR, //  VK_INDEX_TYPE_UINT32 or so
				.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
			},
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};
	VkAccelerationStructureBuildGeometryInfoKHR bottom_build_info = {
		.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = 1,
    .pGeometries   = &bottom_geometry,
	};
	QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
	qvkGetAccelerationStructureBuildSizesKHR(
		device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&bottom_build_info, &primitive_count, &bottom_sizes);
/// =====================================================================================================================
}



raytrace_graph_alloc()
{
/// graph goes allocate stuff here
/// =====================================================================================================================
/// build the acceleration structures and alloc/free scratch mem
/// do this in graph_run() if a rebuild is needed (and only then, see module flags!)
#if 1
  // TODO: where to allocate the scratch mem? how much is it? can we afford to keep it around forever? then put it in separate memory buffer as the others
	// Allocate scratch memory for the build
	VkBufferCreateInfo scratch_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizes[0].buildScratchSize,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizes[1].buildScratchSize,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	buffers_t scratch;
	if (create_buffers(&scratch, device, scratch_infos, 2, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT))
		return 1;
#endif
/// =====================================================================================================================




/// =====================================================================================================================
/// on graph, top level accel
// TODO: if any ray tracing at all has been detected, do this:
	// Figure out how big the buffers for the top-level need to be
	VkAccelerationStructureBuildSizesInfoKHR top_sizes = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};
	VkBufferDeviceAddressInfo instances_address = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = staging.buffers[1].buffer
	};
	VkAccelerationStructureGeometryKHR top_geometry = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
		.geometry = {
			.instances = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
				.arrayOfPointers = VK_FALSE,
				// XXX see below .data = { .deviceAddress = vkGetBufferDeviceAddress(device->device, &instances_address) },
			},
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};
	VkAccelerationStructureBuildGeometryInfoKHR top_build_info = {
		.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = 1,
    .pGeometries   = &top_geometry,
	};
	QVK_LOAD(vkGetAccelerationStructureBuildSizesKHR);
	qvkGetAccelerationStructureBuildSizesKHR(
		device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&top_build_info, &primitive_count, &top_sizes);
/// =====================================================================================================================





	
/// =====================================================================================================================
/// allocate persistent buffers/stuff for the accel structs
/// TODO: consider usage types and put in global table held with graph! (see uniform, data, image buffers)
	// Create buffers for the acceleration structures
	QVK_LOAD(vkCreateAccelerationStructureKHR)
	VkAccelerationStructureBuildSizesInfoKHR sizes[2] = { bottom_sizes, top_sizes };
	VkBufferCreateInfo buffer_requests[] = {
		{ // XXX this is bottom level
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizes[0].accelerationStructureSize,
			.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
		},
		{ // XXX this is for top level
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizes[1].accelerationStructureSize,
			.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
		},
	};
  // XXX 
	if (create_buffers(&structure->buffers, device, buffer_requests, 2, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		return 1;

    // TODO: run this per node and once for the top level
	// Create the acceleration structures
	for (uint32_t i = 0; i != 2; ++i) {
		VkAccelerationStructureCreateInfoKHR create_info = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			.buffer = structure->buffers.buffers[i].buffer,
			.offset = 0, .size = sizes[i].accelerationStructureSize,
			.type = (i == 0) ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		};
		if (qvkCreateAccelerationStructureKHR(device->device, &create_info, NULL, &structure->levels[i]))
			return 1;
	}
/// =====================================================================================================================
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

  // TODO: allocate graph->rt_instance here?
  // XXX TODO: do not build bottom if not needed (see flags on module)
  graph->rt_instance_end = 0;
  for(int i=0;i<node_cnt;i++)
  { // check all nodes for ray tracing geometry
    dt_node_t *node = graph->node + nodeid[i];
    if(!(node->type & s_node_geometry)) continue;
    VkAccelerationStructureDeviceAddressInfoKHR address_request = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = node->rt_accel,
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
		  .primitiveCount = node->rt_tri_cnt }};
		VkBufferDeviceAddressInfo scratch_adress_info = {
			.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = node->rt_scratch,
		};
    VkBufferDeviceAddressInfo index_address = {
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt_idx,
    };
    VkBufferDeviceAddressInfo vertex_address = {
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = node->rt_vtx,
    };
    // could batch up the build_info and build_ranges and call vkBuildAccelerationStructuresKHR only once:
		node->rt_build_info.dstAccelerationStructure  = node->rt_accel;
		node->rt_build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(qvk.device, &scratch_adress_info);
		node->rt_build_info.pGeometries[0].geometry.triangles.vertexData = vkGetBufferDeviceAddress(qvk.device, &vertex_address);
		node->rt_build_info.pGeometries[0].geometry.triangles.indexData  = vkGetBufferDeviceAddress(qvk.device, &index_address);
    const VkAccelerationStructureBuildRangeInfoKHR build_range = {
      .primitiveCount = node->rt_tri_cnt };
		qvkCmdBuildAccelerationStructuresKHR(graph->command_buffer, 1, &node->rt_build_info, &&build_range);
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
    .buffer = graph->rt_scratch,
  };
  VkBufferDeviceAddressInfo index_address = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = graph->rt_instances,
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
}
