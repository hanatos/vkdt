#pragma once

// this takes care of the memory allocation, buffer binding, image and
// image_view creation and the descriptor sets for the connectors between
// nodes. staging buffers are created for source and sink nodes for images as
// well as source nodes for ssbo, to facilitate cpu memory mapping of host
// visible staging memory with the appropriate layout transforms in between.
// ssbo sinks don't have staging memory, but their sink connectors use the
// staging memory directly.
// there is support for array connectors, with multiple buffers per connector.
// images (not ssbo) also support *dynamic* connectors via the
// s_conn_dynamic_array flag on the connector. this is implemented with a
// fixed-size sub-allocator that makes aliasing in further pipeline processing
// impossible, so it is only really useful for dynamic texture caches that go
// directly from CPU to the consumer node.

static inline VkResult
dt_check_device_allocation(uint64_t size, int heap_index)
{
  // vkAllocateMemory overcommits, moves to system ram, and sometimes works even when you think it should not.
  // find out whether we still stay in device memory, and fail over if not:
  if(size > qvk.max_allocation_size) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
  };
  VkPhysicalDeviceMemoryProperties2 memprop = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    .pNext = &budget,
  };
  vkGetPhysicalDeviceMemoryProperties2(qvk.physical_device, &memprop);
  for (int i=0;i<memprop.memoryProperties.memoryHeapCount;i++)
    if(memprop.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
      if(budget.heapBudget[i] > size)
        return VK_SUCCESS;
  dt_log(s_log_qvk, "failed to allocate %ld MB index %d", size/1024/1024, heap_index);
  return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

static inline VkFormat
dt_connector_vkformat(const dt_connector_t *c)
{
  const int len = dt_connector_channels(c);
  if(c->format == dt_token("ui32") || c->format == dt_token("u32") || (c->format == dt_token("atom") && !qvk.float_atomics_supported))
  {
    switch(len)
    { // int32 does not have UNORM equivalents (use SFLOAT instead i guess)
      case 1: return VK_FORMAT_R32_UINT;
      case 2: return VK_FORMAT_R32G32_UINT;
      case 3: // return VK_FORMAT_R32G32B32_UINT;
      case 4: return VK_FORMAT_R32G32B32A32_UINT;
    }
  }
  if(c->format == dt_token("f32") || (c->format == dt_token("atom") && qvk.float_atomics_supported))
  {
    switch(len)
    {
      case 1: return VK_FORMAT_R32_SFLOAT;          // r32f
      case 2: return VK_FORMAT_R32G32_SFLOAT;       // rg32f
      case 3: // return VK_FORMAT_R32G32B32_SFLOAT; // glsl does not support this
      case 4: return VK_FORMAT_R32G32B32A32_SFLOAT; // rgba32f
    }
  }
  if(c->format == dt_token("f16"))
  {
    switch(len)
    {
      case 1: return VK_FORMAT_R16_SFLOAT;          // r16f
      case 2: return VK_FORMAT_R16G16_SFLOAT;       // rg16f
      case 3: // return VK_FORMAT_R16G16B16_SFLOAT; // glsl does not support this
      case 4: return VK_FORMAT_R16G16B16A16_SFLOAT; // rgba16f
    }
  }
  if(c->format == dt_token("ui16") || c->format == dt_token("u16"))
  {
    switch(len)
    {
      case 1: return VK_FORMAT_R16_UNORM;
      case 2: return VK_FORMAT_R16G16_UNORM;
      case 3: // return VK_FORMAT_R16G16B16_UNORM;
      case 4: return VK_FORMAT_R16G16B16A16_UNORM;
    }
  }
  if(c->format == dt_token("ui8") || c->format == dt_token("u8"))
  {
    switch(len)
    {
      case 1: return VK_FORMAT_R8_UNORM;
      case 2: return VK_FORMAT_R8G8_UNORM;
      case 3: // return VK_FORMAT_R8G8B8_UNORM;
      case 4: return VK_FORMAT_R8G8B8A8_UNORM;
    }
  }
  if(c->format == dt_token("yuv")) return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
  if(c->format == dt_token("bc1")) return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
  return VK_FORMAT_UNDEFINED;
}

// update descriptor sets
static inline VkResult
write_descriptor_sets(
    dt_graph_t     *graph,
    dt_node_t      *node,
    dt_connector_t *c)
{
  if(!node->dset[0]) return VK_SUCCESS; // node has no dset to write to
  if(dt_connector_owner(c))
  {
    if(dt_connector_ssbo(c))
    { // storage buffer
      VkWriteDescriptorSet    img_dset[DT_GRAPH_MAX_FRAMES] = {{0}};
      VkDescriptorBufferInfo *buf_info = alloca(sizeof(VkDescriptorBufferInfo)*2*MAX(1,c->array_length));
      int cur_buf = 0;
      for(int f=0;f<2;f++)
      {
        img_dset[f].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[f].dstSet          = node->dset[f];
        img_dset[f].dstBinding      = c - node->connector;
        img_dset[f].descriptorCount = MAX(c->array_length, 1);
        img_dset[f].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        img_dset[f].pBufferInfo     = buf_info + cur_buf;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, MIN(f, c->frames-1));
          buf_info[cur_buf++] = (VkDescriptorBufferInfo){
            .buffer = img->buffer,
            .offset = c->ssbo_offset,
            .range  = img->size-c->ssbo_offset,
          };
        }
      }
      vkUpdateDescriptorSets(qvk.device, 2, img_dset, 0, NULL);
    }
    else
    { // storage images
      VkDescriptorImageInfo *img_info = alloca(sizeof(VkDescriptorImageInfo) *2*MAX(1,c->array_length));
      if(c->flags & s_conn_dynamic_array)
      { // dynamic arrays will only initialise the descriptor set necessary for the current frame
        // since the frames refer to the odd/even pipelines and not to feedback buffers used in the same
        // command buffer:
        fprintf(stderr, "owner dynamic array write desc\n");
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, graph->double_buffer);
          if(!img->image_view) // for dynamic arrays maybe a lazy programmer left empty slots? set to tex 0:
            img = dt_graph_connector_image(graph,
                node - graph->node, c - node->connector, 0, graph->double_buffer);
          img_info[k] = (VkDescriptorImageInfo){
            .sampler     = VK_NULL_HANDLE,
            .imageView   = img->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
          };
        }
        VkWriteDescriptorSet img_dset = {
          .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet          = node->dset[graph->double_buffer],
          .dstBinding      = c - node->connector,
          .descriptorCount = MAX(c->array_length, 1),
          .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo      = img_info,
        };
        vkUpdateDescriptorSets(qvk.device, 1, &img_dset, 0, NULL);
      }
      else
      {
        VkWriteDescriptorSet img_dset[DT_GRAPH_MAX_FRAMES] = {{0}};
        int cur_img = 0;
        for(int f=0;f<2;f++)
        {
          img_dset[f].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          img_dset[f].dstSet          = node->dset[f];
          img_dset[f].dstBinding      = c - node->connector;
          img_dset[f].descriptorCount = MAX(c->array_length, 1);
          img_dset[f].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
          img_dset[f].pImageInfo      = img_info + cur_img;
          for(int k=0;k<MAX(1,c->array_length);k++)
          {
            dt_connector_image_t *img = dt_graph_connector_image(graph,
                node - graph->node, c - node->connector, k, MIN(f, c->frames-1));
            img_info[cur_img++] = (VkDescriptorImageInfo){
              .sampler     = VK_NULL_HANDLE,
              .imageView   = img->image_view,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
            assert(img->image_view);
          }
        }
        vkUpdateDescriptorSets(qvk.device, 2, img_dset, 0, NULL);
      }
    }
  }

  if(dt_connector_input(c))
  { // point our inputs to their counterparts:
    dt_cid_t owner = dt_connector_find_owner(graph, (dt_cid_t){node-graph->node, c-node->connector});
    if(dt_cid_unset(c->connected))
    { // sorry not connected, buffer will not be bound.
      // unconnected inputs are a problem however:
      snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf), "kernel %"PRItkn"_%"PRItkn"_%"PRItkn":%"PRItkn" is not connected!",
          dt_token_str(node->name), dt_token_str(node->module->inst),
          dt_token_str(node->kernel), dt_token_str(c->name));
      graph->gui_msg = graph->gui_msg_buf;
      return VK_INCOMPLETE; // signal a problem
    }

    if(dt_connector_ssbo(c))
    { // storage buffers
      VkWriteDescriptorSet    img_dset[DT_GRAPH_MAX_FRAMES] = {{0}};
      VkDescriptorBufferInfo *buf_info = alloca(sizeof(VkDescriptorBufferInfo)*2*MAX(1,c->array_length));
      int cur_buf = 0;
      for(int f=0;f<2;f++)
      {
        img_dset[f].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[f].dstSet          = node->dset[f];
        img_dset[f].dstBinding      = c - node->connector;
        img_dset[f].descriptorCount = MAX(c->array_length, 1);
        img_dset[f].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        img_dset[f].pBufferInfo     = buf_info + cur_buf;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          int frame = MIN(f, graph->node[owner.i].connector[owner.c].frames-1);
          if(c->flags & s_conn_feedback)
          { // feedback connections cross the frame wires:
            frame = 1-f;
            // this should be ensured during connection:
            assert(c->frames == 2); 
            assert(graph->node[owner.i].connector[owner.c].frames == 2);
          }
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, frame);
          // storage buffer
          buf_info[cur_buf++] = (VkDescriptorBufferInfo){
            .buffer = img->buffer,
            .offset = c->ssbo_offset,
            .range  = img->size-c->ssbo_offset,
          };
        }
      }
      vkUpdateDescriptorSets(qvk.device, 2, img_dset, 0, NULL);
    }
    else if(graph->node[owner.i].connector[owner.c].flags & s_conn_dynamic_array)
    { // dynamic arrays (don't cross wires)
      VkDescriptorImageInfo *img_info = alloca(sizeof(VkDescriptorImageInfo)*MAX(1,c->array_length));
      for(int k=0;k<MAX(1,c->array_length);k++)
      {
        dt_connector_image_t *img = dt_graph_connector_image(graph,
            node - graph->node, c - node->connector, k, graph->double_buffer);
        if(!img->image_view) // for dynamic arrays maybe a lazy programmer left empty slots? set to tex 0:
          img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, 0, graph->double_buffer);
        img_info[k] = (VkDescriptorImageInfo){
          .sampler = 
            (c->format == dt_token("yuv")) ? 0 : // needs immutable sampler
            (node->module->name == dt_token("display")) ? qvk.tex_sampler_dspy :
            (c->type == dt_token("sink") || c->format == dt_token("ui32") || c->format == dt_token("atom")) ? qvk.tex_sampler_nearest :
            qvk.tex_sampler,
          .imageView   = img->image_view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
      }
      VkWriteDescriptorSet img_dset = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = node->dset[graph->double_buffer],
        .dstBinding      = c - node->connector,
        .descriptorCount = MAX(c->array_length, 1),
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = img_info,
      };
      vkUpdateDescriptorSets(qvk.device, 1, &img_dset, 0, NULL);
    }
    else
    { // images
      VkWriteDescriptorSet    img_dset[DT_GRAPH_MAX_FRAMES] = {{0}};
      VkDescriptorImageInfo  *img_info = alloca(sizeof(VkDescriptorImageInfo) *2*MAX(1,c->array_length));
      int cur_img = 0;
      for(int f=0;f<2;f++)
      {
        img_dset[f].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[f].dstSet          = node->dset[f];
        img_dset[f].dstBinding      = c - node->connector;
        img_dset[f].descriptorCount = MAX(c->array_length, 1);
        img_dset[f].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        img_dset[f].pImageInfo      = img_info + cur_img;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          int frame = MIN(f, graph->node[owner.i].connector[owner.c].frames-1);
          if(c->flags & s_conn_feedback)
          { // feedback connections cross the frame wires:
            frame = 1-f;
            // this should be ensured during connection:
            assert(c->frames == 2); 
            assert(graph->node[owner.i].connector[owner.c].frames == 2);
          }
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, frame);
          img_info[cur_img++] = (VkDescriptorImageInfo){
            .sampler = 
              (c->format == dt_token("yuv")) ? 0 : // needs immutable sampler
              (node->module->name == dt_token("display")) ? qvk.tex_sampler_dspy :
              (c->type == dt_token("sink") || c->format == dt_token("ui32") || c->format == dt_token("atom")) ? qvk.tex_sampler_nearest :
              qvk.tex_sampler,
            .imageView   = img->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          };
        }
      }
      vkUpdateDescriptorSets(qvk.device, 2, img_dset, 0, NULL);
    }
  }
  return VK_SUCCESS;
}


static inline VkResult
bind_buffers_to_memory(
    dt_graph_t     *graph,
    dt_node_t      *node,
    dt_connector_t *c,
    int             f,     // frame index
    int             k)     // array index
{
  if(!dt_connector_owner(c)) return VK_SUCCESS;
  if(f > 0 && c->frames < 2) return VK_SUCCESS; // this is just a copy, not a real double buffer
  dt_connector_image_t *img = dt_graph_connector_image(graph, node - graph->node, c - node->connector, k, f);
  if(dt_connector_ssbo(c))
  { // storage buffer
    if(img->image_view) vkDestroyImageView(qvk.device, img->image_view, VK_NULL_HANDLE);
    img->image_view = 0;
    QVKR(vkBindBufferMemory(qvk.device, img->buffer, graph->vkmem, img->offset));
  }
  else
  { // storage image
    if(!img->mem) return VK_SUCCESS; // ignore freed slots for dynamically allocated arrays.

    // only actually backed by memory if we have all these frames.
    // else we'll just point to the same image for all frames.
    if(c->format == dt_token("yuv"))
    { // two-plane yuv:
      VkBindImagePlaneMemoryInfo bind_image_plane0_info = {
        .sType       = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
        .planeAspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
      };
      VkBindImagePlaneMemoryInfo bind_image_plane1_info = {
        .sType       = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
        .planeAspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
      };
      VkBindImageMemoryInfo bind_image_memory_infos[] = {{
        .sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        .pNext        = &bind_image_plane0_info,
        .image        = img->image,
        .memory       = graph->vkmem,
        .memoryOffset = img->offset,
      },{
        .sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        .pNext        = &bind_image_plane1_info,
        .image        = img->image,
        .memory       = graph->vkmem,
        .memoryOffset = img->offset + img->plane1_offset,
      }};
      QVKR(vkBindImageMemory2(qvk.device, 2, bind_image_memory_infos));
    }
    else
    { // plain:
      QVKR(vkBindImageMemory(qvk.device, img->image, graph->vkmem, img->offset));
    }

    VkSamplerYcbcrConversionInfo ycbcr_info = {
      .sType      = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
      .conversion = qvk.yuv_conversion,
    };
    VkImageViewCreateInfo images_view_create_info = {
      .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext      = c->format == dt_token("yuv") ? &ycbcr_info : 0,
      .viewType   = VK_IMAGE_VIEW_TYPE_2D,
      .format     = dt_connector_vkformat(c),
      .image      = img->image,
      .subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = img->mip_levels,
        .baseArrayLayer = 0,
        .layerCount     = 1
      },
    };
    if(img->image_view) vkDestroyImageView(qvk.device, img->image_view, VK_NULL_HANDLE);
    QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &img->image_view));
  }
  return VK_SUCCESS;
}

static inline void
memory_requirements(
    dt_connector_t       *c,
    dt_connector_image_t *img,
    VkMemoryRequirements *mem_req)
{
  if(c->format == dt_token("yuv"))
  {
    VkImagePlaneMemoryRequirementsInfo plane_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
      .planeAspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
    };
    VkImageMemoryRequirementsInfo2 image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .pNext = &plane_info,
      .image = img->image,
    };
    VkMemoryRequirements2 memory_requirements2 = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vkGetImageMemoryRequirements2(qvk.device, &image_info, &memory_requirements2);
    *mem_req = memory_requirements2.memoryRequirements;
    // plane 1
    plane_info.planeAspect = VK_IMAGE_ASPECT_PLANE_1_BIT;
    vkGetImageMemoryRequirements2(qvk.device, &image_info, &memory_requirements2);
    img->plane1_offset = mem_req->size; // TODO: plus alignment gaps?
    mem_req->size += memory_requirements2.memoryRequirements.size;
  }
  else if(c->chan == dt_token("ssbo"))
    vkGetBufferMemoryRequirements(qvk.device, img->buffer, mem_req);
  else
    vkGetImageMemoryRequirements(qvk.device, img->image, mem_req);
}

// 2nd pass: find memory address to bind our buffers to:
static inline VkResult
alloc_alias_memory(dt_graph_t *graph, dt_node_t *node)
{
  const int nid = node - graph->node;
  VkMemoryRequirements mem_req;
  for(int cid=0;cid<node->num_connectors;cid++)
  { // allocate all outputs owned by us (img,ssbo,staging)
    dt_connector_t *c = node->connector+cid;
    if(dt_connector_owner(c))
    {
      if(c->flags & s_conn_dynamic_array)
      { // this is shit and should probably get a fake alignment value for a fake image.
        // amd requires this large one, nvidia can do one 0 less
        c->array_mem = dt_vkalloc_protected(&graph->heap, c->array_heap_size, 0x10000);
        c->array_heap = calloc(sizeof(dt_vkalloc_t), 1);
        dt_vkalloc_init(c->array_heap, c->array_length * 2, c->array_heap_size);
      }
      else
      {
        for(int f=0;f<c->frames;f++) for(int aid=0;aid<MAX(1,c->array_length);aid++)
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              nid, cid, aid, f);
          memory_requirements(c, img, &mem_req);
          if(c->flags & s_conn_protected) // allocate protected memory
            img->mem = dt_vkalloc_protected(&graph->heap, mem_req.size, mem_req.alignment);
          else
            img->mem = dt_vkalloc(&graph->heap, mem_req.size, mem_req.alignment);
          img->offset = img->mem->offset;
          img->size   = dt_connector_bufsize(c, c->roi.wd, c->roi.ht);
        }
      }
    }
    if(dt_connector_has_staging(c, node))
    {
      vkGetBufferMemoryRequirements(qvk.device, c->staging[0], &mem_req);
      int need_dbuf = node->flags & s_module_request_read_source; // only good for sources
      size_t staging_size = need_dbuf ? mem_req.size * 2 : mem_req.size;
      c->mem_staging       = dt_vkalloc(&graph->heap_staging, staging_size, mem_req.alignment);
      c->offset_staging[0] = c->mem_staging->offset;
      c->offset_staging[1] = need_dbuf ? c->mem_staging->offset + mem_req.size : c->mem_staging->offset;
      // fprintf(stderr, "allocing staging dbuf %d %"PRItkn"_%"PRItkn"_%"PRItkn"@%d [%lx,%lx)\n",
      //     need_dbuf, dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name),
      //     0, c->mem_staging->offset, c->mem_staging->offset + staging_size);
    }
  }
  for(int cid=0;cid<node->num_connectors;cid++)
  {
    dt_connector_t *c = node->connector+cid;
    dt_cid_t owner = dt_connector_find_owner(graph, c->connected);
    if(dt_cid_unset(owner)) continue;
    if(!(graph->node[owner.i].connector[owner.c].flags & s_conn_protected))
    { // the protected flag is set for feedback, dynamic arrays, sources, or explicitly
      for(int aid=0;aid<MAX(1,c->array_length);aid++)
      {
        dt_connector_image_t *img = dt_graph_connector_image(graph,
            nid, cid, aid, graph->double_buffer);
        // this could logically happen on the connector, we don't have more fine grained access:
        if(img->nid_last_ref == nid) // free this buffer, be it output or input
        {
          dt_vkfree(&graph->heap, img->mem);
          img->nid_last_ref = -1; // avoid double free
        }
      }
    }
    // can't free staging memory, the other pipeline might need it.
    // if at all, we could trade source for sink staging, but not even.
    // if(c->mem_staging) // always free staging memory, if we have it
      // dt_vkfree(&graph->heap_staging, c->mem_staging);
  }
  return VK_SUCCESS;
}

// initialise descriptor sets
static inline VkResult
alloc_descriptor_sets(dt_graph_t *graph, dt_node_t *node)
{
  if(node->dset_layout)
  { // this is not set for source nodes. create descriptor set(s) for other nodes:
    VkDescriptorSetLayout lo[DT_GRAPH_MAX_FRAMES];
    for(int k=0;k<DT_GRAPH_MAX_FRAMES;k++) lo[k] = node->dset_layout;
    VkDescriptorSetAllocateInfo dset_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = graph->dset_pool,
      .descriptorSetCount = DT_GRAPH_MAX_FRAMES,
      .pSetLayouts = lo,
    };
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, node->dset));

    // uniform descriptor
    VkDescriptorSetAllocateInfo dset_info_u = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = graph->dset_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &graph->uniform_dset_layout,
    };
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info_u, node->uniform_dset+0));
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info_u, node->uniform_dset+1));
    VkDescriptorBufferInfo uniform_info[] = {{
      .buffer      = graph->uniform_buffer,
      .offset      = 0,
      .range       = graph->uniform_global_size,
    },{
      .buffer      = graph->uniform_buffer,
      .offset      = node->module->uniform_size ? node->module->uniform_offset : 0,
      .range       = node->module->uniform_size ? node->module->uniform_size : graph->uniform_global_size,
    },{
      .buffer      = graph->uniform_buffer,
      .offset      = graph->uniform_size,
      .range       = graph->uniform_global_size,
    },{
      .buffer      = graph->uniform_buffer,
      .offset      = graph->uniform_size + (node->module->uniform_size ? node->module->uniform_offset : 0),
      .range       = node->module->uniform_size ? node->module->uniform_size : graph->uniform_global_size,
    }};
    VkWriteDescriptorSet buf_dset[] = {{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = node->uniform_dset[0],
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = uniform_info+0,
    },{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = node->uniform_dset[0],
      .dstBinding      = 1,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = uniform_info+1,
    },{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = node->uniform_dset[1],
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = uniform_info+2,
    },{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = node->uniform_dset[1],
      .dstBinding      = 1,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = uniform_info+3,
    }};
    vkUpdateDescriptorSets(qvk.device, 4, buf_dset, 0, NULL);
  }

  if(node->type == s_node_graphics)
  { // create framebuffer for draw connectors
    VkImageView attachment[DT_MAX_CONNECTORS];
    int cnt = 0, first = -1;
    for(int i=0;i<node->num_connectors;i++) if(dt_connector_owner(node->connector+i))
    { // make sure there is no array and make sure they are same res!
      if(first == -1) first = i;
      assert(node->connector[i].array_length <= 1);
      assert(node->connector[i].roi.wd == 
             node->connector[first].roi.wd);
      assert(node->connector[i].roi.ht == 
             node->connector[first].roi.ht);

      dt_connector_image_t *img  = dt_graph_connector_image(graph,
          node - graph->node, i, 0, 0);
      attachment[cnt++] = img->image_view;
    }
    VkFramebufferCreateInfo fb_create_info = {
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = node->draw_render_pass,
      .attachmentCount = cnt,
      .pAttachments    = attachment,
      .width           = node->connector[first].roi.wd,
      .height          = node->connector[first].roi.ht,
      .layers          = 1,
    };
    if(cnt) QVKR(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, &node->draw_framebuffer));
  }
  return VK_SUCCESS;
}

// 3rd pass: now we have images and vkDeviceMemory, let's bind images and buffers
// to memory and create descriptor sets
static inline VkResult
alloc_bind_memory(dt_graph_t *graph, dt_node_t *node)
{
  for(int cid=0;cid<node->num_connectors;cid++)
  {
    dt_connector_t *c = node->connector+cid;
    if(c->mem_staging)
    { // bind staging memory:
      vkBindBufferMemory(qvk.device, c->staging[0], graph->vkmem_staging, c->offset_staging[0]);
      vkBindBufferMemory(qvk.device, c->staging[1], graph->vkmem_staging, c->offset_staging[1]);
    }

    if(!(c->flags & s_conn_dynamic_array)) // dynamic arrays take care of themselves later
      for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
        QVKR(bind_buffers_to_memory(graph, node, c, f, k));
  }
  return VK_SUCCESS;
}


static inline VkResult
create_buffer(
    dt_graph_t        *graph,
    dt_node_t         *node,
    dt_connector_t    *c,                   // the connector with an output array of buffers
    int                f,                   // double buffer index
    int                k)                   // array index or 0 if no array
{
  const int i = c - node->connector;
  const size_t size = dt_connector_bufsize(c, c->roi.wd, c->roi.ht);
  dt_connector_image_t *img = dt_graph_connector_image(graph,
      node - graph->node, i, k, f);
  if(img->image)  vkDestroyImage (qvk.device, img->image,  VK_NULL_HANDLE);
  if(img->buffer) vkDestroyBuffer(qvk.device, img->buffer, VK_NULL_HANDLE);
  img->image  = 0;
  img->buffer = 0;
  VkBufferCreateInfo create_info = {
    .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size        = size,
    .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | // to build ray tracing accels on geo from here
                   (qvk.raytracing_supported ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0),
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  QVKR(vkCreateBuffer(qvk.device, &create_info, 0, &img->buffer));
#ifdef DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
  char name[100];
  const char *dedup;
  snprintf(name, sizeof(name), "%"PRItkn"_%"PRItkn"_%"PRItkn"@%d", dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name), f);
  dt_stringpool_get(&graph->debug_markers, name, strlen(name), 0, &dedup);
  VkDebugMarkerObjectNameInfoEXT name_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
    .object = (uint64_t) img->buffer,
    .objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
    .pObjectName = dedup,
  };
  qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
  // fprintf(stderr, "DEBUG allocating ssbo %"PRItkn":%"PRItkn":%"PRItkn":%"PRItkn" %zu = %d x %d\n", dt_token_str(node->name), dt_token_str(node->module->inst), dt_token_str(node->kernel), dt_token_str(c->name), size, c->roi.wd, c->roi.ht);
  return VK_SUCCESS;
}

static inline VkResult
create_image(
    dt_graph_t        *graph,
    dt_node_t         *node,
    dt_connector_t    *c,    // the connector with an output array of images
    int                f,    // frame index for array connectors/double buffers
    int                k)    // array index or 0 if no array
{
  const uint32_t wd = MAX(1, c->array_dim ? c->array_dim[2*k+0] : c->roi.wd);
  const uint32_t ht = MAX(1, c->array_dim ? c->array_dim[2*k+1] : c->roi.ht);
  VkFormat format = dt_connector_vkformat(c);
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = c->format == dt_token("yuv") ? VK_IMAGE_CREATE_DISJOINT_BIT : 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = { // safeguard against disconnected 0-size extents
      .width  = wd,
      .height = ht,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = c->format == dt_token("yuv") ?
      VK_IMAGE_TILING_LINEAR:
      VK_IMAGE_TILING_OPTIMAL,
    .usage                 = 
      c->format == dt_token("bc1") || c->format == dt_token("yuv") ?
      VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT :
      VK_IMAGE_USAGE_STORAGE_BIT
      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT
      | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  // fprintf(stderr, "DEBUG allocating imag %"PRItkn":%"PRItkn":%"PRItkn":%"PRItkn" %d x %d\n", dt_token_str(node->name), dt_token_str(node->module->inst), dt_token_str(node->kernel), dt_token_str(c->name), wd, ht);
  dt_connector_image_t *img = dt_graph_connector_image(graph,
      node - graph->node, c - node->connector, k, f);
  if(c->flags & s_conn_mipmap) // maybe actually a few are enough
    img->mip_levels = images_create_info.mipLevels = MIN(4, (uint32_t)(log2f(MAX(wd, ht))) + 1);
  else img->mip_levels = 1;
  if(img->image)  vkDestroyImage (qvk.device, img->image,  VK_NULL_HANDLE);
  if(img->buffer) vkDestroyBuffer(qvk.device, img->buffer, VK_NULL_HANDLE);
  img->mem    = 0;
  img->image  = 0;
  img->buffer = 0;
  img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
  QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &img->image));
#ifdef DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
  char name[100];
  const char *dedup;
  snprintf(name, sizeof(name), "%"PRItkn"_%"PRItkn"_%"PRItkn"@%d,%d", dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name), f, k);
  dt_stringpool_get(&graph->debug_markers, name, strlen(name), 0, &dedup);
  VkDebugMarkerObjectNameInfoEXT name_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
    .object = (uint64_t) img->image,
    .objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT,
    .pObjectName = dedup,
  };
  qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
  return VK_SUCCESS;
}


// create VkBuffer objects associated with images and ssbo
static inline VkResult
create_buffers(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(c->array_heap)
    { // free any potential residuals of dynamic allocation
      dt_vkalloc_cleanup(c->array_heap);
      free(c->array_heap);
      c->array_heap= 0;
      c->array_heap_size = 0;
      c->array_mem = 0; // freeing will not do anything, since it's protected. this will only go away via nuking/graph_reset
    }

    if(dt_connector_has_staging(c, node))
    { // need staging memory (for sources or sinks, be it ssbo or images)
      VkBufferCreateInfo buffer_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = dt_connector_bufsize(c, c->roi.wd, c->roi.ht),
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      };
      QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &c->staging[0]));
      QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &c->staging[1]));
#ifdef DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
      char name[100];
      const char *dedup;
      snprintf(name, sizeof(name), "staging %"PRItkn"_%"PRItkn"_%"PRItkn"@%d", dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name), 0);
      dt_stringpool_get(&graph->debug_markers, name, strlen(name), 0, &dedup);
      VkDebugMarkerObjectNameInfoEXT name_info = {
        .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        .object      = (uint64_t)c->staging[0],
        .objectType  = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
        .pObjectName = dedup,
      };
      qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
    }

    if(dt_connector_input(c))
    { // point our inputs to their counterparts
      dt_cid_t owner = dt_connector_find_owner(graph, c->connected);
      if(!dt_cid_unset(owner)) // point to conn_image of connected output directly:
        node->conn_image[i] = graph->node[owner.i].conn_image[owner.c];
    }

    if(!(c->flags & s_conn_dynamic_array) && dt_connector_owner(c))
    {
      for(int f=0;f<c->frames;f++)
        for(int k=0;k<MAX(1,c->array_length);k++)
          if(dt_connector_ssbo(c))
            QVKR(create_buffer(graph, node, c, f, k));
          else 
            QVKR(create_image (graph, node, c, f, k));
    }
  }
  return VK_SUCCESS;
}


// allocate output buffers, also create vulkan pipeline and load spir-v portion
// of the compute shader.
static inline VkResult
alloc_outputs(dt_graph_t *graph, dt_node_t *node)
{
  // create descriptor bindings and pipeline:
  // we'll bind our buffers in the same order as in the connectors file.
  uint32_t drawn_connector_cnt = 0;
  uint32_t drawn_connector[DT_MAX_CONNECTORS];
  VkDescriptorSetLayoutBinding bindings[DT_MAX_CONNECTORS] = {{0}};
  const int nid = node - graph->node;
  for(int cid=0;cid<node->num_connectors;cid++)
  {
    dt_connector_t *c = node->connector+cid;
    dt_cid_t owner = dt_connector_find_owner(graph, (dt_cid_t){nid, cid});
    if(dt_cid_unset(owner)) return VK_INCOMPLETE; // not connected
    if(c->frames == 2 || c->type == dt_token("source") ||
       node->name == dt_token("bvh") || // ray tracing nodes require the vertex data
      (graph->node[owner.i].connector[owner.c].flags & s_conn_dynamic_array))
    {
      c->flags |= s_conn_protected; // allocate protected memory for feedback and sources (for caching)
      graph->node[owner.i].connector[owner.c].flags |= s_conn_protected;
    }
    if(!(c->flags & s_conn_protected)) for(int aid=0;aid<MAX(1,c->array_length);aid++)
    { // mark this as still in use by our node. we're called in fwd topological sort order.
      dt_connector_image_t *allocation = dt_graph_connector_image(
          graph, owner.i, owner.c, aid, graph->double_buffer);
      allocation->nid_last_ref = nid;
    }
    bindings[cid].binding = cid;
    if(dt_connector_ssbo(node->connector+cid))
    { // might be "read"|"write"|"sink"|"source"|"modify" we don't care
      graph->dset_cnt_buffer += MAX(1, node->connector[cid].array_length);
      bindings[cid].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else if(node->connector[cid].type == dt_token("read") ||
            node->connector[cid].type == dt_token("sink"))
    {
      graph->dset_cnt_image_read += MAX(1, node->connector[cid].array_length);
      bindings[cid].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else // image "write"|"source"|"modify"
    {
      graph->dset_cnt_image_write += MAX(1, node->connector[cid].array_length);
      bindings[cid].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      if(node->type == s_node_graphics)
        drawn_connector[drawn_connector_cnt++] = cid;
    }
    bindings[cid].descriptorCount = MAX(1, node->connector[cid].array_length);
    bindings[cid].stageFlags = VK_SHADER_STAGE_ALL;//COMPUTE_BIT;
    bindings[cid].pImmutableSamplers = node->connector[cid].format == dt_token("yuv") ?
      &qvk.tex_sampler_yuv : 0;
  }

  // create render pass in case we need rasterised output:
  if(drawn_connector_cnt)
  {
    VkAttachmentDescription attachment_desc[DT_MAX_CONNECTORS];
    VkAttachmentReference color_attachment[DT_MAX_CONNECTORS];
    for(int i=0;i<drawn_connector_cnt;i++)
    {
      int k = drawn_connector[i];
      attachment_desc[i] = (VkAttachmentDescription) {
        .format         = dt_connector_vkformat(node->connector+k),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR, // VK_ATTACHMENT_LOAD_OP_DONT_CARE, // select on s_conn_clear flag?
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };
      color_attachment[i] = (VkAttachmentReference) {
        .attachment = i,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };
    }
    VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = drawn_connector_cnt,
      .pColorAttachments = color_attachment,
    };
    VkRenderPassCreateInfo info = {
      .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = drawn_connector_cnt,
      .pAttachments    = attachment_desc,
      .subpassCount    = 1,
      .pSubpasses      = &subpass,
    };
    QVK(vkCreateRenderPass(qvk.device, &info, 0, &node->draw_render_pass));
  }

  // a sink needs a descriptor set (for display via gui)
  if(!dt_node_source(node))
  { // create a descriptor set layout
    VkDescriptorSetLayoutCreateInfo dset_layout_info = {
      .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = node->num_connectors,
      .pBindings    = bindings,
    };
    QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &node->dset_layout));
  }

  // a sink or a source does not need a pipeline to be run.
  // note, however, that a sink does need a descriptor set, as we want to bind
  // it to ui textures later on.
  if(!(dt_node_sink(node) || dt_node_source(node)))
  {
    // create the pipeline layout
    VkDescriptorSetLayout dset_layout[] = {
      graph->uniform_dset_layout,
      node->dset_layout,
      graph->rt.dset_layout, // goes last, is optional
    };
    VkPushConstantRange pcrange = {
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset     = 0,
      .size       = node->push_constant_size,
    };
    VkPipelineLayoutCreateInfo layout_info = {
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = LENGTH(dset_layout) - 1 + dt_raytrace_present(graph),
      .pSetLayouts            = dset_layout,
      .pushConstantRangeCount = node->push_constant_size ? 1 : 0,
      .pPushConstantRanges    = node->push_constant_size ? &pcrange : 0,
    };
    QVKR(vkCreatePipelineLayout(qvk.device, &layout_info, 0, &node->pipeline_layout));

    if(drawn_connector_cnt)
    { // create rasterisation pipeline
      const int wd = node->connector[drawn_connector[0]].roi.wd;
      const int ht = node->connector[drawn_connector[0]].roi.ht;
      VkShaderModule shader_module_vert, shader_module_frag;
      QVKR(dt_graph_create_shader_module(graph, node->name, node->kernel, "vert", &shader_module_vert));
      QVKR(dt_graph_create_shader_module(graph, node->name, node->kernel, "frag", &shader_module_frag));

      // vertex shader, fragment shader
      VkPipelineShaderStageCreateInfo shader_info[] = {{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_VERTEX_BIT,
          .module = shader_module_vert,
          .pName  = "main",
      },{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = shader_module_frag,
          .pName  = "main",
      }};

      VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = NULL,
      };

      VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        // TODO: make configurable:
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
      };

      VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float) wd,
        .height   = (float) ht,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      };

      VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = {
          .width  = wd,
          .height = ht,
        },
      };

      VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
      };

      VkPipelineRasterizationStateCreateInfo rasterizer_state = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE, /* skip rasterizer */
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
      };

      VkPipelineMultisampleStateCreateInfo multisample_state = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = VK_FALSE,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading      = 1.0f,
        .pSampleMask           = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
      };

      VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT
          | VK_COLOR_COMPONENT_G_BIT
          | VK_COLOR_COMPONENT_B_BIT
          | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
        .dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
        .colorBlendOp        = VK_BLEND_OP_MAX,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .alphaBlendOp        = VK_BLEND_OP_MAX,
      };

      VkPipelineColorBlendStateCreateInfo color_blend_state = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment,
        .blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f },
      };

      VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = LENGTH(shader_info),
        .pStages    = shader_info,

        .pVertexInputState   = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer_state,
        .pMultisampleState   = &multisample_state,
        .pDepthStencilState  = NULL,
        .pColorBlendState    = &color_blend_state,
        .pDynamicState       = NULL,

        .layout              = node->pipeline_layout,
        .renderPass          = node->draw_render_pass,
        .subpass             = 0,

        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
      };

      QVKR(vkCreateGraphicsPipelines(qvk.device, VK_NULL_HANDLE,
            1, &pipeline_info, NULL, &node->pipeline));

      vkDestroyShaderModule(qvk.device, shader_module_vert, 0);
      vkDestroyShaderModule(qvk.device, shader_module_frag, 0);
    }
    else
    { // create the compute shader stage
      VkShaderModule shader_module;
      QVKR(dt_graph_create_shader_module(graph, node->name, node->kernel, "comp", &shader_module));

      // TODO: cache pipelines on module->so ?
      // vk 1.3:
      // VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT sub = {
      //   .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
      //   .requiredSubgroupSize = 32,
      // };
      // and also remember to set computeFullSubgroups for intel!
      VkPipelineShaderStageCreateInfo stage_info = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
        .pSpecializationInfo = 0,
        .pName               = "main", // arbitrary entry point symbols are supported by glslangValidator, but need extra compilation, too. i think it's easier to structure code via includes then.
        .module              = shader_module,
        // .pNext               = &sub, // vk 1.3
      };

      // finally create the pipeline
      VkComputePipelineCreateInfo pipeline_info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage_info,
        .layout = node->pipeline_layout
      };
      QVKR(vkCreateComputePipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &node->pipeline));

      // we don't need the module any more
      vkDestroyShaderModule(qvk.device, stage_info.module, 0);
    }
  } // done with pipeline

  return create_buffers(graph, node);
}


// run the graph, node portion, allocate buffers
// this mostly does things if run & s_graph_run_alloc. if not, it only checks the state
// of dynamic arrays.
// this cleans up the heaps if the runflag is set, allocates new memory,
// binds images to buffers and creates images views. handles dynamic arrays and their descriptor sets.
// * alloc_outputs:
//   - create buffers for img, ssbo, staging
//   - determine last use of buffer in the topological sort (for free)
//   - count required descriptors
// * allocate vk memory and descriptor set pools
// * alloc_outputs2:
//   - run aliasing memory allocator and bind buffers to memory
//   - write descriptor sets
static inline VkResult
dt_graph_run_nodes_allocate(
    dt_graph_t     *graph,
    dt_graph_run_t *run,
    uint32_t       *nodeid,
    int             cnt,
    int            *dynamic_array) // will remain 0 if there are dynamic arrays that did not request changes, triggers upload later
{
  *dynamic_array = 0;
  // ==============================================
  // 1st pass
  // ==============================================
  if(*run & s_graph_run_alloc)
  {
    // init ray tracing on graph, after output roi and nodes have been inited:
    QVKR(dt_raytrace_graph_init(graph, nodeid, cnt));
    // free pipeline resources if previously allocated anything:
    dt_vkalloc_nuke(&graph->heap);
    dt_vkalloc_nuke(&graph->heap_staging);
    graph->dset_cnt_image_read = 0;
    graph->dset_cnt_image_write = 0;
    graph->dset_cnt_buffer = 0;
    graph->dset_cnt_uniform = DT_GRAPH_MAX_FRAMES; // we have one global uniform for params, per frame
    graph->memory_type_bits = ~0u;
    graph->memory_type_bits_staging = ~0u;
    for(int i=0;i<cnt;i++)
      QVKR(alloc_outputs(graph, graph->node+nodeid[i]));
  }

  // ==============================================
  //  2nd pass: run aliasing memory allocator
  // ==============================================
  if(*run & s_graph_run_alloc)
    for(int i=0;i<cnt;i++)
      QVKR(alloc_alias_memory(graph, graph->node+nodeid[i]));

  // ==============================================
  //  now allocate vkmem and descriptor set pools
  // ==============================================
  if(graph->heap.vmsize > graph->vkmem_size)
  {
    *run |= s_graph_run_upload_source; // new mem means new source
    if(graph->vkmem)
    {
      vkFreeMemory(qvk.device, graph->vkmem, 0);
      graph->vkmem = 0;
    }
    graph->vkmem_size = 0;
    QVKR(dt_check_device_allocation(graph->heap.vmsize, 0));
    VkMemoryAllocateFlagsInfo allocation_flags = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };
    VkMemoryAllocateInfo mem_alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext           = &allocation_flags,
      .allocationSize  = graph->heap.vmsize,
      .memoryTypeIndex = qvk_memory_get_device(),
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->vkmem));
    graph->vkmem_size = graph->heap.vmsize;
  }

  if(graph->heap_staging.vmsize > graph->vkmem_staging_size)
  {
    *run |= s_graph_run_upload_source; // new mem means new source
    if(graph->vkmem_staging)
    {
      vkFreeMemory(qvk.device, graph->vkmem_staging, 0);
      graph->vkmem_staging = 0;
    }
    graph->vkmem_staging_size = 0;
    QVKR(dt_check_device_allocation(graph->heap_staging.vmsize, 1));
    // staging memory to copy to and from device
    VkMemoryAllocateFlagsInfo allocation_flags = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };
    VkMemoryAllocateInfo mem_alloc_info_staging = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext           = &allocation_flags,
      .allocationSize  = graph->heap_staging.vmsize,
      .memoryTypeIndex = qvk_memory_get_staging(),
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info_staging, 0, &graph->vkmem_staging));
    graph->vkmem_staging_size = graph->heap_staging.vmsize;
  }

  if(graph->vkmem_uniform_size < DT_GRAPH_MAX_FRAMES * graph->uniform_size)
  {
    if(graph->vkmem_uniform)
    {
      vkFreeMemory(qvk.device, graph->vkmem_uniform, 0);
      graph->vkmem_uniform = 0;
    }
    graph->vkmem_uniform_size = 0;
    // uniform data to pass parameters
    VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size  = DT_GRAPH_MAX_FRAMES * graph->uniform_size,
      .usage = // VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vkDestroyBuffer(qvk.device, graph->uniform_buffer, 0);
    QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &graph->uniform_buffer));
#ifdef DEBUG_MARKERS
#ifdef QVK_ENABLE_VALIDATION
    char name[100];
    const char *dedup;
    snprintf(name, sizeof(name), "uniform buffer");
    dt_stringpool_get(&graph->debug_markers, name, strlen(name), 0, &dedup);
    VkDebugMarkerObjectNameInfoEXT name_info = {
      .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
      .object      = (uint64_t)graph->uniform_buffer,
      .objectType  = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
      .pObjectName = dedup,
    };
    qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(qvk.device, graph->uniform_buffer, &mem_req);
    VkMemoryAllocateInfo mem_alloc_info_uniform = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = mem_req.size,
      .memoryTypeIndex = qvk_memory_get_uniform(),
    };
    QVKR(dt_check_device_allocation(mem_req.size, 2));
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info_uniform, 0, &graph->vkmem_uniform));
    graph->vkmem_uniform_size = 2 * graph->uniform_size;
    vkBindBufferMemory(qvk.device, graph->uniform_buffer, graph->vkmem_uniform, 0);
  }

  if(*run & s_graph_run_alloc)
  {
    if(graph->dset_pool &&
      graph->dset_cnt_image_read_alloc >= graph->dset_cnt_image_read &&
      graph->dset_cnt_image_write_alloc >= graph->dset_cnt_image_write &&
      graph->dset_cnt_buffer_alloc >= graph->dset_cnt_buffer &&
      graph->dset_cnt_uniform_alloc >= graph->dset_cnt_uniform)
    {
      QVKR(vkResetDescriptorPool(qvk.device, graph->dset_pool, 0));
    }
    else
    {
      // create descriptor pool (keep at least one for each type)
      VkDescriptorPoolSize pool_sizes[] = {{
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1+DT_GRAPH_MAX_FRAMES*graph->dset_cnt_image_read,
      }, {
        .type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1+DT_GRAPH_MAX_FRAMES*graph->dset_cnt_image_write,
      }, {
        .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1+DT_GRAPH_MAX_FRAMES*graph->dset_cnt_buffer,
      }, {
        .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1+DT_GRAPH_MAX_FRAMES*2*graph->num_nodes,
      }, {
        .type            = qvk.raytracing_supported ?
          VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR :
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // dummy for validation layer
        .descriptorCount = DT_GRAPH_MAX_FRAMES,
      }};

      VkDescriptorPoolCreateInfo pool_info = {
        .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = LENGTH(pool_sizes),
        .pPoolSizes    = pool_sizes,
        .maxSets       = DT_GRAPH_MAX_FRAMES*(
            graph->dset_cnt_image_read + graph->dset_cnt_image_write
          + graph->num_nodes
          + graph->dset_cnt_buffer     + graph->dset_cnt_uniform),
      };
      vkDestroyDescriptorPool(qvk.device, graph->dset_pool, VK_NULL_HANDLE);
      graph->dset_pool = 0;
      QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &graph->dset_pool));
      graph->dset_cnt_image_read_alloc = graph->dset_cnt_image_read;
      graph->dset_cnt_image_write_alloc = graph->dset_cnt_image_write;
      graph->dset_cnt_buffer_alloc = graph->dset_cnt_buffer;
      graph->dset_cnt_uniform_alloc = graph->dset_cnt_uniform;
    }
  }

  // ==============================================
  // 3nd pass finish alloc
  // ==============================================
  if(*run & s_graph_run_alloc)
    for(int i=0;i<cnt;i++)
      QVKR(alloc_bind_memory(graph, graph->node+nodeid[i]));

  // allocate memory and create descriptor sets for ray tracing
  if(*run & s_graph_run_alloc)
    QVKR(dt_raytrace_graph_alloc(graph));

  // ==============================================
  //  find dynamically allocated connector in node that has array requests set:
  // ==============================================
  int recreate_descriptor_sets = 0;
  for(int i=0;i<cnt;i++)
  {
    const int nid = nodeid[i];
    dt_node_t *node = graph->node + nid;
    for(int cid=0;cid<node->num_connectors;cid++)
    {
      dt_connector_t *c = node->connector+cid;
      if(!(c->flags & s_conn_dynamic_array)) continue;
      if(!dt_connector_owner(c)) continue;

      if(c->array_resync)
      { // last frame has pending sync work to do
        for(int aid=0;aid<MAX(1,c->array_length);aid++)
        { // keep array images the same in general, but delete images that wandered off the command buffer bindings:
          dt_connector_image_t *oldimg = dt_graph_connector_image(graph, nid, cid, aid, graph->double_buffer^1);
          dt_connector_image_t *newimg = dt_graph_connector_image(graph, nid, cid, aid, graph->double_buffer);
          if(!oldimg || !newimg) continue; // will insert dummy later
          if(oldimg->image == newimg->image) continue; // is the same already
          if(newimg->buffer)     vkDestroyBuffer   (qvk.device, newimg->buffer,     VK_NULL_HANDLE);
          if(newimg->image_view) vkDestroyImageView(qvk.device, newimg->image_view, VK_NULL_HANDLE);
          if(newimg->image)      vkDestroyImage    (qvk.device, newimg->image,      VK_NULL_HANDLE);
          if(newimg->mem)        dt_vkfree(c->array_heap, newimg->mem);
          *newimg = *oldimg;  // point to same memory
          recreate_descriptor_sets = 1; // need to re-create descriptor setS
        }
        c->array_resync = 0; // done with this request
      }

      if(c->array_req)
      for(int aid=0;aid<MAX(1,c->array_length);aid++)
      {
        if(c->array_req[aid] || (aid == 0 && !c->array_req[0]))
        { // free texture. we depend on f-1 still holding our data, it will be freed during resync.
          dt_connector_image_t *img = dt_graph_connector_image(graph, nid, cid, aid, graph->double_buffer);
          if(c->array_req[aid] || (aid == 0 && !c->array_req[0] && !img->image_view))
          {
            recreate_descriptor_sets = 1; // need to recreate dsc set
            *dynamic_array = 1;           // need to upload new texture
            c->array_resync = 1;          // bring other frame in sync next time to pick up our changes
            assert(img);
            *img = (dt_connector_image_t){0}; // mirrors the other double buffer, if there is anything on it
            QVKR(create_image(graph, node, c, graph->double_buffer, aid));
            VkMemoryRequirements mem_req;
            memory_requirements(c, img, &mem_req);
            const uint32_t wd = MAX(1, c->array_dim ? c->array_dim[2*aid+0] : c->roi.wd);
            const uint32_t ht = MAX(1, c->array_dim ? c->array_dim[2*aid+1] : c->roi.ht);
            img->mem = dt_vkalloc(c->array_heap, mem_req.size, mem_req.alignment);
            img->offset = img->mem->offset + c->array_mem->offset;
            img->size   = dt_connector_bufsize(c, wd, ht);
            QVKR(bind_buffers_to_memory(graph, node, c, graph->double_buffer, aid));
          }
        }
        // upload is handled later
      }
    } // end for all connectors
  }

  // write the dynamic array descriptor sets
  // this writes both the output and connected input descriptors.
  for(int i=0;i<cnt;i++)
  {
    const int nid = nodeid[i];
    if(*run & s_graph_run_alloc)
      alloc_descriptor_sets(graph, graph->node+nid);
    if(*run & s_graph_run_alloc)
      for(int j=0;j<graph->node[nid].num_connectors;j++)
        QVKR(write_descriptor_sets(graph, graph->node+nid, graph->node[nid].connector + j));
    else for(int cid=0;cid<graph->node[nid].num_connectors;cid++)
    {
      dt_cid_t owner = dt_connector_find_owner(graph, (dt_cid_t){nid, cid});
      if(!dt_cid_unset(owner) &&
          (graph->node[owner.i].connector[owner.c].flags & s_conn_dynamic_array) &&
          recreate_descriptor_sets)
        QVKR(write_descriptor_sets(graph, graph->node+nid, graph->node[nid].connector+cid));
    }
  }
  return VK_SUCCESS;
}
