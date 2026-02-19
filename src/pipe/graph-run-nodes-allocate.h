#pragma once

// this takes care of the memory allocation, buffer binding, image and image_view
// creation and the descriptor sets for the connectors between nodes.
// staging buffers are created for source and sink nodes for images, to facilitate
// cpu memory mapping of host visible staging memory with the appropriate
// layout transforms in between. ssbo/buffers don't have staging memory, but their
// source and sink connectors use the staging memory directly.
// there is support for array connectors, with multiple buffers per connector.
// images (not ssbo) also support *dynamic* connectors via the
// s_conn_dynamic_array flag on the connector.
// this is implemented with a fixed-size sub-allocator that makes aliasing
// in further pipeline processing impossible, so it is only really useful
// for dynamic texture caches that go directly from CPU to the consumer node.

static inline VkResult
dt_check_device_allocation(uint64_t size)
{
  // vkAllocateMemory overcommits, moves to system ram, and sometimes works even when you think it should not.
  // find out whether we still stay in device memory, and fail over if not:
  // if(size > qvk.max_allocation_size) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
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
    dt_connector_t *c,
    int             dyn_array)
{
  VkDescriptorImageInfo  *img_info = alloca(sizeof(VkDescriptorImageInfo) *DT_GRAPH_MAX_FRAMES*MAX(1,c->array_length));
  VkDescriptorBufferInfo *buf_info = alloca(sizeof(VkDescriptorBufferInfo)*DT_GRAPH_MAX_FRAMES*MAX(1,c->array_length));
  VkWriteDescriptorSet    img_dset[DT_GRAPH_MAX_FRAMES] = {{0}};
  int cur_img = 0, cur_dset = 0, cur_buf = 0;

  if(dt_connector_owner(c))
  { // allocate our output buffers
    if(!dyn_array && dt_connector_ssbo(c))
    { // storage buffer
      const int c_dyn_array = c->flags & s_conn_dynamic_array;
      int fm = c_dyn_array ? graph->double_buffer   : 0;
      int fM = c_dyn_array ? graph->double_buffer+1 : 2;
      if((!dyn_array && !c_dyn_array) || (dyn_array && c_dyn_array)) for(int f=fm;f<fM;f++)
      {
        int ii = cur_buf;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, MIN(f, c->frames-1));
          int iii = cur_buf++;
          buf_info[iii].buffer = img->buffer;
          buf_info[iii].offset = c->ssbo_offset;
          buf_info[iii].range  = img->size-c->ssbo_offset;
        }
        int dset = cur_dset++;
        img_dset[dset].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[dset].dstSet          = node->dset[f];
        img_dset[dset].dstBinding      = c - node->connector;
        img_dset[dset].dstArrayElement = 0;
        img_dset[dset].descriptorCount = MAX(c->array_length, 1);
        img_dset[dset].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        img_dset[dset].pBufferInfo     = buf_info + ii;
      }
    }
    else
    { // image buffer
      // dynamic arrays will only initialise the descriptor set necessary for the current frame
      // since the frames refer to the odd/even pipelines and not to feedback buffers used in the same
      // command buffer:
      const int c_dyn_array = c->flags & s_conn_dynamic_array;
      int fm = c_dyn_array ? graph->double_buffer   : 0;
      int fM = c_dyn_array ? graph->double_buffer+1 : 2;
      if((!dyn_array && !c_dyn_array) || (dyn_array && c_dyn_array)) for(int f=fm;f<fM;f++)
      {
        int ii = cur_img;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, MIN(f, c->frames-1));
          if(!img->image_view) // for dynamic arrays maybe a lazy programmer left empty slots? set to tex 0:
            img = dt_graph_connector_image(graph,
                node - graph->node, c - node->connector, 0, MIN(f, c->frames-1));

          int iii = cur_img++;
          img_info[iii].sampler     = VK_NULL_HANDLE;
          img_info[iii].imageView   = img->image_view;
          assert(img->image_view);
          img_info[iii].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        int dset = cur_dset++;
        img_dset[dset].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[dset].dstSet          = node->dset[f];
        img_dset[dset].dstBinding      = c - node->connector;
        img_dset[dset].dstArrayElement = 0;
        img_dset[dset].descriptorCount = MAX(c->array_length, 1);
        img_dset[dset].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        img_dset[dset].pImageInfo      = img_info + ii;
      }
    }
  }
  else if(dt_connector_input(c))
  { // point our inputs to their counterparts:
    dt_cid_t owner = dt_connector_find_owner(graph, (dt_cid_t){node-graph->node, c-node->connector});
    const int c_dyn_array = !dt_cid_unset(owner) ? graph->node[owner.i].connector[owner.c].flags & s_conn_dynamic_array : 0;
    if(!dt_cid_unset(owner) &&
      ((!dyn_array && !c_dyn_array) || (dyn_array && c_dyn_array)))
    {
      int fm = c_dyn_array ? graph->double_buffer     : 0;
      int fM = c_dyn_array ? graph->double_buffer + 1 : 2;
      for(int f=fm;f<fM;f++)
      {
        int ii = dt_connector_ssbo(c) ? cur_buf : cur_img;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          int frame = MIN(f, 
            graph->node[owner.i].connector[owner.c].frames-1);
          if(c->flags & s_conn_feedback)
          { // feedback connections cross the frame wires:
            frame = 1-f;
            // this should be ensured during connection:
            assert(c->frames == 2); 
            assert(graph->node[owner.i].connector[owner.c].frames == 2);
          }
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, c - node->connector, k, frame);
          if(!img)
          {
            snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf), "kernel %"PRItkn"_%"PRItkn"_%"PRItkn":%"PRItkn" is not connected!",
                dt_token_str(node->name), dt_token_str(node->module->inst),
                dt_token_str(node->kernel), dt_token_str(c->name));
            graph->gui_msg = graph->gui_msg_buf;
            return VK_INCOMPLETE; // signal a problem
          }
          if(dt_connector_ssbo(c))
          { // storage buffer
            int iii = cur_buf++;
            buf_info[iii].buffer = img->buffer;
            buf_info[iii].offset = c->ssbo_offset;
            buf_info[iii].range  = img->size-c->ssbo_offset;
          }
          else
          { // image buffer
            // the image struct is shared between in and out connectors, but we 
            // acces the frame either straight or crossed, depending on feedback mode.
            if(!img->image_view) // for dynamic arrays maybe a lazy programmer left empty slots? set to tex 0:
              img = dt_graph_connector_image(graph,
                  node - graph->node, c - node->connector, 0, frame);
            int iii = cur_img++;
            if(c->format == dt_token("yuv"))
              img_info[iii].sampler   = 0; // needs immutable sampler
            else if(node->module->name == dt_token("display"))
              img_info[iii].sampler   = qvk.tex_sampler_dspy;
            else if(c->type == dt_token("sink") || c->format == dt_token("ui32") || c->format == dt_token("atom"))
              img_info[iii].sampler   = qvk.tex_sampler_nearest;
            else
              img_info[iii].sampler   = qvk.tex_sampler;
            img_info[iii].imageView   = img->image_view;
            assert(img->image_view);
            img_info[iii].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          }
        }
        int dset = cur_dset++;
        img_dset[dset].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[dset].dstSet          = node->dset[f];
        img_dset[dset].dstBinding      = c - node->connector;
        img_dset[dset].dstArrayElement = 0;
        img_dset[dset].descriptorCount = MAX(c->array_length, 1);
        if(dt_connector_ssbo(c))
        {
          img_dset[dset].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          img_dset[dset].pBufferInfo     = buf_info + ii;
        }
        else
        {
          img_dset[dset].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
          img_dset[dset].pImageInfo      = img_info + ii;
        }
      }
    }
    else if(dt_cid_unset(c->connected))
    { // sorry not connected, buffer will not be bound.
      // unconnected inputs are a problem however:
      snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf), "kernel %"PRItkn"_%"PRItkn"_%"PRItkn":%"PRItkn" is not connected!",
          dt_token_str(node->name), dt_token_str(node->module->inst),
          dt_token_str(node->kernel), dt_token_str(c->name));
      graph->gui_msg = graph->gui_msg_buf;
      return VK_INCOMPLETE; // signal a problem
    }
  }
  if(node->dset_layout && cur_dset)
    vkUpdateDescriptorSets(qvk.device, cur_dset, img_dset, 0, NULL);
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
    if(c->type == dt_token("source"))
      QVKR(vkBindBufferMemory(qvk.device, img->buffer, graph->vkmem_staging, img->offset));
    else
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


// 2nd pass, now we have images and vkDeviceMemory, let's bind images and buffers
// to memory and create descriptor sets
static inline VkResult
alloc_outputs2(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(!(c->flags & s_conn_dynamic_array)) // regular case, first pass
      for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
        QVKR(bind_buffers_to_memory(graph, node, c, f, k));
  }
  return VK_SUCCESS;
}

// initialise descriptor sets, bind staging buffer memory
static inline VkResult
alloc_outputs3(dt_graph_t *graph, dt_node_t *node)
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

  for(int i=0;i<node->num_connectors;i++)
    if(!(node->connector[i].flags & s_conn_dynamic_array))
      QVKR(write_descriptor_sets(graph, node, node->connector + i, 0));

  for(int i=0;i<node->num_connectors;i++)
  { // bind staging memory:
    dt_connector_t *c = node->connector+i;
    if((!dt_connector_ssbo(c) && (c->type == dt_token("source"))) || c->type == dt_token("sink"))
    {
      vkBindBufferMemory(qvk.device, c->staging[0], graph->vkmem_staging, c->offset_staging[0]);
      vkBindBufferMemory(qvk.device, c->staging[1], graph->vkmem_staging, c->offset_staging[1]);
    }
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


// free all buffers which we are done with now that the node
// has been processed. that is: all inputs and all of our outputs
// which aren't connected to another node.
// note that vkfree doesn't in fact free anything in case the reference count is > 0
static inline VkResult
free_inputs(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if((c->type == dt_token("read") ||
        c->type == dt_token("modify")) &&
       !dt_cid_unset(c->connected) &&
       !(c->flags & s_conn_feedback) &&
       !(graph->node[c->connected.i].connector[c->connected.c].flags & s_conn_dynamic_array))
    { // only free "read"|"modify", not "sink" which we keep around for display
      // dt_log(s_log_pipe, "freeing input %"PRItkn"_%"PRItkn" %"PRItkn,
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(c->name));
      // ssbo do not use staging memory for sinks, and if they did these are no sinks:
      for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
      {
        dt_connector_image_t *im = dt_graph_connector_image(graph, node-graph->node, i, k, f);
        if(!im) return VK_INCOMPLETE;
        dt_vkfree(&graph->heap, im->mem);
      }
      // note that we keep the offset and VkImage etc around, we'll be using
      // these in consecutive runs through the pipeline and only clean up at
      // the very end. we just instruct our allocator that we're done with
      // this portion of the memory.
    }
    else if(dt_connector_owner(c) && !(c->flags & s_conn_dynamic_array))
    {
      // dt_log(s_log_pipe, "freeing output ref count %"PRItkn"_%"PRItkn" %"PRItkn" %d %d",
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(c->name),
      //     c->connected_mi, c->mem->ref);
      for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
      {
        dt_connector_image_t *im = dt_graph_connector_image(graph, node-graph->node, i, k, f);
        if(!im) return VK_INCOMPLETE;
        if(dt_connector_ssbo(c) && c->type == dt_token("source")) // host visible buffer
          dt_vkfree(&graph->heap_staging, im->mem);
        else // device visible only
          dt_vkfree(&graph->heap, im->mem);
      }
    }
    // staging memory for sources or sinks only needed during execution once
    if(c->mem_staging)
    {
      // dt_log(s_log_pipe, "freeing staging %"PRItkn"_%"PRItkn" %"PRItkn,
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(c->name));
      dt_vkfree(&graph->heap_staging, c->mem_staging);
    }
  }
  return VK_SUCCESS;
}

static inline VkResult
allocate_buffer_array_element(
    dt_graph_t        *graph,
    dt_node_t         *node,
    dt_connector_t    *c,                   // the connector with an output array of buffers
    dt_vkalloc_t      *unused_heap,         // TODO: allocator to ask for memory. this is graph->heap for non dynamic arrays
    uint64_t           unused_heap_offset,  // if dynamic/inner allocator is used, this is the offset to the outer memory
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
  VkMemoryRequirements buf_mem_req;

  if(c->type == dt_token("source"))
  { // source nodes are protected because we want to avoid re-transmit when possible.
    // also, they are allocated in host-visible staging memory
    vkGetBufferMemoryRequirements(qvk.device, img->buffer, &buf_mem_req);
    // graph->memory_type_bits_staging = buf_mem_req.memoryTypeBits;

    img->mem = dt_vkalloc_protected(&graph->heap_staging, buf_mem_req.size, buf_mem_req.alignment);
  }
  else
  { // other buffers are device only, i.e. use the default heap:
    vkGetBufferMemoryRequirements(qvk.device, img->buffer, &buf_mem_req);
    // graph->memory_type_bits = buf_mem_req.memoryTypeBits;

    // XXX TODO: need to work here if we want to enable uploads to dynamic arrays.
    // XXX this means allocate sources as protected inside the inner heap
    // XXX all inner allocations are already protected/feedback for all the outer heap knows.
    // XXX this is a problem for geometry node kinda graphs/memory use
    // if(heap_offset == 0 && (c->frames == 2 || c->type == dt_token("source") || (c->flags & s_conn_protected))) // allocate protected memory, only in outer heap
    if((c->frames == 2 || (c->flags & s_conn_protected))) // allocate protected memory
      img->mem = dt_vkalloc_protected(&graph->heap, buf_mem_req.size, buf_mem_req.alignment);
    else
      img->mem = dt_vkalloc(&graph->heap, buf_mem_req.size, buf_mem_req.alignment);
  }
  img->offset = img->mem->offset;
  img->size   = size; // for validation layers, this is the smaller of the two sizes.
  // set the staging offsets so it'll transparently work with read_source further down
  // when running the graph.
  c->size_staging      = size;
  c->offset_staging[f] = img->mem->offset;
  if(f == 0 && c->frames < 2) c->offset_staging[1] = img->mem->offset; // not double buffered
  // reference counting. we can't just do a ref++ here because we will
  // free directly after and wouldn't know which node later on still relies
  // on this buffer. hence we ran a reference counting pass before this, and
  // init the reference counter now accordingly:
  img->mem->ref = c->connected.i;

  if(c->type == dt_token("source") || (c->flags & s_conn_protected))
    img->mem->ref++; // add one more so we can run the pipeline starting from after upload easily
  return VK_SUCCESS;
}

static inline VkResult
allocate_image_array_element(
    dt_graph_t        *graph,
    dt_node_t         *node,
    dt_connector_t    *c,                   // the connector with an output array of images
    dt_vkalloc_t      *heap,                // allocator to ask for memory. this is graph->heap for non dynamic arrays
    uint64_t           heap_offset,         // if dynamic/inner allocator is used, this is the offset to the outer memory
    int                f,                   // frame index for array connectors
    int                k)                   // array index or 0 if no array
{
  VkFormat format = dt_connector_vkformat(c);
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = c->format == dt_token("yuv") ? VK_IMAGE_CREATE_DISJOINT_BIT : 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = { // safeguard against disconnected 0-size extents
      .width  = MAX(c->roi.wd, 1),
      .height = MAX(c->roi.ht, 1),
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
    // we will potentially need access to these images from both graphics and compute pipeline:
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE, // VK_SHARING_MODE_CONCURRENT, 
    // .queueFamilyIndexCount = 0,//2,
    // .pQueueFamilyIndices   = 0,//queues,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  uint32_t wd = MAX(1, c->roi.wd), ht = MAX(1, c->roi.ht);
  // fprintf(stderr, "DEBUG allocating imag %"PRItkn":%"PRItkn":%"PRItkn":%"PRItkn" %d x %d\n", dt_token_str(node->name), dt_token_str(node->module->inst), dt_token_str(node->kernel), dt_token_str(c->name), wd, ht);
  if(c->array_dim)
  { // array with varying size images?
    wd = MAX(1, c->array_dim[2*k+0]);
    ht = MAX(1, c->array_dim[2*k+1]);
  }
  images_create_info.extent.width  = wd;
  images_create_info.extent.height = ht;
  dt_connector_image_t *img = dt_graph_connector_image(graph,
      node - graph->node, c - node->connector, k, f);
  if(c->flags & s_conn_mipmap) // maybe actually a few are enough
    img->mip_levels = images_create_info.mipLevels = MIN(4, (uint32_t)(log2f(MAX(wd, ht))) + 1);
  else img->mip_levels = 1;
  if(img->image)  vkDestroyImage (qvk.device, img->image,  VK_NULL_HANDLE);
  if(img->buffer) vkDestroyBuffer(qvk.device, img->buffer, VK_NULL_HANDLE);
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

  VkMemoryRequirements mem_req;
  if(c->format == dt_token("yuv"))
  { // get memory requirements for each plane and combine
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
    mem_req = memory_requirements2.memoryRequirements;
    // plane 1
    plane_info.planeAspect = VK_IMAGE_ASPECT_PLANE_1_BIT;
    vkGetImageMemoryRequirements2(qvk.device, &image_info, &memory_requirements2);
    // TODO: plus alignment gaps?
    img->plane1_offset = mem_req.size;
    mem_req.size += memory_requirements2.memoryRequirements.size;
    mem_req.memoryTypeBits |= memory_requirements2.memoryRequirements.memoryTypeBits;
  }
  else
  {
    vkGetImageMemoryRequirements(qvk.device, img->image, &mem_req);
  }
  // graph->memory_type_bits = mem_req.memoryTypeBits;

  assert(!(mem_req.alignment & (mem_req.alignment - 1)));

  if(heap_offset == 0 && (c->frames == 2 || c->type == dt_token("source") || (c->flags & s_conn_protected))) // allocate protected memory, only in outer heap
    img->mem = dt_vkalloc_protected(heap, mem_req.size, mem_req.alignment);
  else
    img->mem = dt_vkalloc(heap, mem_req.size, mem_req.alignment);
  // dt_log(s_log_pipe, "allocating %.1f/%.1f MB for %"PRItkn" %"PRItkn" "
  //     "%"PRItkn" %"PRItkn,
  //     mem_req.size/(1024.0*1024.0), size/(1024.0*1024.0), dt_token_str(node->name), dt_token_str(c->name),
  //     dt_token_str(c->chan), dt_token_str(c->format));
  assert(img->mem);
  img->offset = img->mem->offset + heap_offset;
  img->size   = img->mem->size;
  // reference counting. we can't just do a ref++ here because we will
  // free directly after and wouldn't know which node later on still relies
  // on this buffer. hence we ran a reference counting pass before this, and
  // init the reference counter now accordingly:
  if(heap_offset == 0)
    img->mem->ref = c->connected.i;

  // TODO: better and more general caching:
  if(heap_offset == 0 && (c->type == dt_token("source") || (c->flags & s_conn_protected)))
    img->mem->ref++; // add one more so we can run the pipeline starting from after upload easily

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
  for(int i=0;i<node->num_connectors;i++)
  {
    bindings[i].binding = i;
    if(dt_connector_ssbo(node->connector+i))
    { // might be "read"|"write"|"sink"|"source"|"modify" we don't care
      graph->dset_cnt_buffer += MAX(1, node->connector[i].array_length);
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else if(node->connector[i].type == dt_token("read") || node->connector[i].type == dt_token("sink"))
    {
      graph->dset_cnt_image_read += MAX(1, node->connector[i].array_length);
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else // image "write"|"source"|"modify"
    {
      graph->dset_cnt_image_write += MAX(1, node->connector[i].array_length);
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      if(node->type == s_node_graphics)
        drawn_connector[drawn_connector_cnt++] = i;
    }
    bindings[i].descriptorCount = MAX(1, node->connector[i].array_length);
    bindings[i].stageFlags = VK_SHADER_STAGE_ALL;//COMPUTE_BIT;
    bindings[i].pImmutableSamplers = node->connector[i].format == dt_token("yuv") ?
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

  // a sink needs a descriptor set (for display via imgui)
  if(!dt_node_source(node))
  {
    // create a descriptor set layout
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
  // it to imgui textures later on.
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
      // TODO: cache shader modules on global struct? (we'll reuse the draw kernels for all strokes)
      const int wd = node->connector[drawn_connector[0]].roi.wd;
      const int ht = node->connector[drawn_connector[0]].roi.ht;
      VkShaderModule shader_module_vert, shader_module_geom, shader_module_frag;
      QVKR(dt_graph_create_shader_module(graph, node->name, node->kernel, "vert", &shader_module_vert));
      QVKR(dt_graph_create_shader_module(graph, node->name, node->kernel, "frag", &shader_module_frag));
      VkResult geom = dt_graph_create_shader_module(graph, node->name, node->kernel, "geom", &shader_module_geom);

      // vertex shader, geometry shader, fragment shader
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
      },{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_GEOMETRY_BIT,
          .module = shader_module_geom,
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
        .topology               = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
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
        .stageCount = geom == VK_SUCCESS ? LENGTH(shader_info) : LENGTH(shader_info)-1,
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

      // TODO: keep cached for others
      vkDestroyShaderModule(qvk.device, shader_module_vert, 0);
      vkDestroyShaderModule(qvk.device, shader_module_frag, 0);
      if(geom == VK_SUCCESS) vkDestroyShaderModule(qvk.device, shader_module_geom, 0);
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

  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(dt_connector_ssbo(c) && dt_connector_owner(c))
    {
      // prepare allocation for storage buffer. this is easy, we do not
      // need to create an image, and computing the size is easy, too.

      const int dynalloc = c->flags & s_conn_dynamic_array; // connector asks for their own memory pool
      if(c->array_alloc)
      { // free any potential residuals of dynamic allocation
        dt_vkalloc_cleanup(c->array_alloc);
        free(c->array_alloc);
        c->array_alloc = 0;
        c->array_alloc_size = 0;
        c->array_mem = 0; // freeing will not do anything, since it's protected. this will only go away via nuking/graph_reset
      }
      // if regular allocation takes place, do it now (dynamic will be deferred)
      if(!dynalloc)
        for(int f=0;f<c->frames;f++)
          for(int k=0;k<MAX(1,c->array_length);k++)
            QVKR(allocate_buffer_array_element(
                graph, node, c, &graph->heap, 0, f, k));
      else
      { // in case of dynamic allocation, reserve a protected block and wait until later
        dt_log(s_log_err, "ssbo arrays cannot be dynamic!");
        assert(0);
        c->array_mem = dt_vkalloc_protected(&graph->heap, c->array_alloc_size, 0x100); // TODO check buffer alignment requirements
        c->array_alloc = calloc(sizeof(dt_vkalloc_t), 1);
        dt_vkalloc_init(c->array_alloc, c->array_length * 2, c->array_alloc_size);
      }
    }
    else if(!dt_connector_ssbo(c) && dt_connector_owner(c))
    { // allocate our output buffers
      const int dynalloc = c->flags & s_conn_dynamic_array; // connector asks for their own memory pool
      if(c->array_alloc)
      { // free any potential residuals of dynamic allocation
        dt_vkalloc_cleanup(c->array_alloc);
        free(c->array_alloc);
        c->array_alloc = 0;
        c->array_alloc_size = 0;
        c->array_mem = 0; // freeing will not do anything, since it's protected. this will only go away via nuking/graph_reset
      }
      // if regular allocation takes place, do it now (dynamic will be deferred)
      if(!dynalloc)
        for(int f=0;f<c->frames;f++)
          for(int k=0;k<MAX(1,c->array_length);k++)
            QVKR(allocate_image_array_element(
                graph, node, c, &graph->heap, 0, f, k));
      else
      { // in case of dynamic allocation, reserve a protected block and wait until later
        c->array_mem = dt_vkalloc_protected(&graph->heap, c->array_alloc_size, 0x10000); // this is shit and should probably get a fake alignment value for a fake image. amd requires this large one, nvidia can do one 0 less
        c->array_alloc = calloc(sizeof(dt_vkalloc_t), 1);
        dt_vkalloc_init(c->array_alloc, c->array_length * 2, c->array_alloc_size);
      }

      // allocate only one staging buffer for the whole array:
      if(c->type == dt_token("source"))
      {
        // allocate staging buffer for uploading to the just allocated image
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
        snprintf(name, sizeof(name), "%"PRItkn"_%"PRItkn"_%"PRItkn"@%d", dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name), 0);
        dt_stringpool_get(&graph->debug_markers, name, strlen(name), 0, &dedup);
        VkDebugMarkerObjectNameInfoEXT name_info = {
          .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
          .object      = (uint64_t) c->staging[0],
          .objectType  = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
          .pObjectName = dedup,
        };
        qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif
        VkMemoryRequirements buf_mem_req;
        vkGetBufferMemoryRequirements(qvk.device, c->staging[0], &buf_mem_req);
        // graph->memory_type_bits_staging = buf_mem_req.memoryTypeBits;
        int need_dbuf = node->flags & s_module_request_read_source;
        size_t staging_size = need_dbuf ? buf_mem_req.size * 2 : buf_mem_req.size;
        c->mem_staging    = dt_vkalloc(&graph->heap_staging, staging_size, buf_mem_req.alignment);
        c->offset_staging[0] = c->mem_staging->offset;
        c->offset_staging[1] = need_dbuf ? c->mem_staging->offset + buf_mem_req.size : c->mem_staging->offset;
        c->size_staging   = buf_mem_req.size;
        c->mem_staging->ref++; // ref staging memory so we don't overwrite it before the pipe starts (will be written in read_source() in the module)
      }
    }
    else if(dt_connector_input(c))
    { // point our inputs to their counterparts and allocate staging memory for sinks
      dt_cid_t owner = dt_connector_find_owner(graph, c->connected);
      if(!dt_cid_unset(owner))
      { // point to conn_image of connected output directly:
        node->conn_image[i] = graph->node[owner.i].conn_image[owner.c];
        if(c->type == dt_token("sink"))
        {
          // allocate staging buffer for downloading from connected input
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
          snprintf(name, sizeof(name), "%"PRItkn"_%"PRItkn"_%"PRItkn"@%d", dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name), 0);
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
          VkMemoryRequirements buf_mem_req;
          vkGetBufferMemoryRequirements(qvk.device, c->staging[0], &buf_mem_req);
          // graph->memory_type_bits_staging = buf_mem_req.memoryTypeBits;
          c->mem_staging       = dt_vkalloc(&graph->heap_staging, buf_mem_req.size, buf_mem_req.alignment);
          // sinks are (for now) not double buffered:
          c->offset_staging[0] = c->mem_staging->offset;
          c->offset_staging[1] = c->mem_staging->offset;
          c->size_staging      = c->mem_staging->size;
          c->mem_staging->ref++; // so we don't overwrite it before the pipe ends, because write_sink is called for all modules once at the end
        }
      }
    }
  }
  return VK_SUCCESS;
}


static inline VkResult
count_references(dt_graph_t *graph, dt_node_t *node)
{
  // dt_log(s_log_pipe, "counting references for %"PRItkn"_%"PRItkn,
  //     dt_token_str(node->name),
  //     dt_token_str(node->kernel));
  for(int c=0;c<node->num_connectors;c++)
  {
    if(dt_connector_input(node->connector+c))
    { // up reference counter of the connector that owns the output
      dt_cid_t m = dt_connector_find_owner(graph, node->connector[c].connected);
      if(!dt_cid_unset(m))
      {
        graph->node[m.i].connector[m.c].connected.i++; // owners have ref count
      }
      else
      {
        snprintf(graph->gui_msg_buf, sizeof(graph->gui_msg_buf), "kernel %"PRItkn"_%"PRItkn"_%"PRItkn":%"PRItkn" is not connected!",
            dt_token_str(node->name), dt_token_str(node->module->inst),
            dt_token_str(node->kernel), dt_token_str(node->connector[c].name));
        graph->gui_msg = graph->gui_msg_buf;
        return VK_INCOMPLETE;
      }
      // dt_log(s_log_pipe, "references %d on output %"PRItkn"_%"PRItkn":%"PRItkn,
      //     graph->node[mi].connector[mc].connected_mi,
      //     dt_token_str(graph->node[mi].name),
      //     dt_token_str(graph->node[mi].kernel),
      //     dt_token_str(graph->node[mi].connector[mc].name));
    }
    else // if(dt_connector_owner(node->connector+c))
    {
      // output cannot know all modules connected to it, so
      // it stores the reference counter instead.
      // we increase it here because every node needs its own outputs for processing.
      node->connector[c].connected.i++;
      // dt_log(s_log_pipe, "references %d on output %"PRItkn"_%"PRItkn":%"PRItkn,
      //     node->connector[c].connected_mi,
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(node->connector[c].name));
    }
  }
  return VK_SUCCESS;
}


// run the graph, node portion, allocate buffers
// this cleans up the heaps if the runflag is set, allocates new memory,
// binds images to buffers and creates images views. handles dynamic arrays and their descriptor sets.
// * alloc_outputs/free_inputs: count required descriptors in sets/create pipeline/load shader/vkalloc/static create buffers,images,staging buffers
// * allocate vulkan memory for each heap (heap, ssbo, staging, uniforms)
// * alloc_outputs2: bind buffers to memory
// * alloc_outputs3: descriptor sets, bind staging buf memory
// * handle dynamic buffer/image allocation
// * write dynamic descriptor sets
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
  // 1st pass alloc and free
  // ==============================================
  if(*run & s_graph_run_alloc)
  {
    QVKR(dt_raytrace_graph_init(graph, nodeid, cnt)); // init ray tracing on graph, after output roi and nodes have been inited.
    // nuke reference counters:
    for(int n=0;n<graph->num_nodes;n++)
      for(int c=0;c<graph->node[n].num_connectors;c++)
        if(dt_connector_owner(graph->node[n].connector+c))
          graph->node[n].connector[c].connected.i = 0;
    // perform reference counting on the final connected node graph.
    // this is needed for memory allocation later:
    for(int i=0;i<cnt;i++)
      QVKR(count_references(graph, graph->node+nodeid[i]));
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
    {
      QVKR(alloc_outputs(graph, graph->node+nodeid[i]));
      QVKR(free_inputs  (graph, graph->node+nodeid[i]));
    }
  }

  if(graph->heap.vmsize > graph->vkmem_size)
  {
    *run |= s_graph_run_upload_source; // new mem means new source
    if(graph->vkmem)
    {
      vkFreeMemory(qvk.device, graph->vkmem, 0);
      graph->vkmem = 0;
    }
    graph->vkmem_size = 0;
    QVKR(dt_check_device_allocation(graph->heap.vmsize));
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
    QVKR(dt_check_device_allocation(graph->heap_staging.vmsize));
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
    QVKR(dt_check_device_allocation(mem_req.size));
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
  // 2nd pass finish alloc
  // ==============================================
  if(*run & s_graph_run_alloc)
    for(int i=0;i<cnt;i++)
      QVKR(alloc_outputs2(graph, graph->node+nodeid[i]));

  if(*run & s_graph_run_alloc)
    for(int i=0;i<cnt;i++)
      QVKR(alloc_outputs3(graph, graph->node+nodeid[i]));

  // allocate memory and create descriptor sets for ray tracing
  if(*run & s_graph_run_alloc)
    QVKR(dt_raytrace_graph_alloc(graph));

  // find dynamically allocated connector in node that has array requests set:
  for(int i=0;i<cnt;i++)
  {
    dt_node_t *node = graph->node + nodeid[i];
    for(int j=0;j<node->num_connectors;j++)
    {
      dt_connector_t *c = node->connector+j;
      if(c->array_resync)
      { // last frame has pending sync work to do
        for(int a=0;a<MAX(1,c->array_length);a++)
        { // keep array images the same in general, but delete images that wandered off the command buffer bindings:
          dt_connector_image_t *oldimg = dt_graph_connector_image(graph, node-graph->node, j, a, graph->double_buffer^1);
          dt_connector_image_t *newimg = dt_graph_connector_image(graph, node-graph->node, j, a, graph->double_buffer);
          if(!oldimg || !newimg) continue; // will insert dummy later
          if(oldimg->image == newimg->image) continue; // is the same already
          if(newimg->buffer)     vkDestroyBuffer   (qvk.device, newimg->buffer,     VK_NULL_HANDLE);
          if(newimg->image)      vkDestroyImage    (qvk.device, newimg->image,      VK_NULL_HANDLE);
          if(newimg->image_view) vkDestroyImageView(qvk.device, newimg->image_view, VK_NULL_HANDLE);
          if(newimg->mem)        dt_vkfree(c->array_alloc, newimg->mem);
          *newimg = *oldimg;  // point to same memory
          *dynamic_array = 1; // need to re-create descriptor sets
        }
        c->array_resync = 0; // done with this request
      }
      if(dt_connector_owner(c) && (c->flags & s_conn_dynamic_array) && c->array_req)
      for(int k=0;k<MAX(1,c->array_length);k++)
      {
        if(k == 0 && !c->array_req[0])
        { // make sure the fallback slot 0 is always inited with at least rubbish:
          dt_connector_image_t *img = dt_graph_connector_image(graph, nodeid[i], j, k, graph->double_buffer);
          assert(img);
          if(!img->image_view)
          {
            *img = (dt_connector_image_t){0}; // make sure nothing is freed here
            QVKR(allocate_image_array_element(graph, node, c, c->array_alloc, c->array_mem->offset, graph->double_buffer, k));
            QVKR(bind_buffers_to_memory(graph, node, c, graph->double_buffer, k));
          }
        }
        if(c->array_req[k])
        { // free texture. we depend on f-1 still holding our data, it will be freed during resync.
          *dynamic_array = c->array_resync = 1; // remember to bring other frame in sync next time to pick up our changes
          dt_connector_image_t *img = dt_graph_connector_image(graph, nodeid[i], j, k, graph->double_buffer);
          assert(img);
          *img = (dt_connector_image_t){0}; // make sure nothing is freed here
          QVKR(allocate_image_array_element(graph, node, c, c->array_alloc, c->array_mem->offset, graph->double_buffer, k));
          QVKR(bind_buffers_to_memory(graph, node, c, graph->double_buffer, k));
        }
        // upload is handled below
      }
    } // end for all connectors
  }
  // write the dynamic array descriptor sets separately (this is skipped in alloc_outputs3 above).
  // this writes both the output and connected input descriptors.
  if(*dynamic_array)
    for(int i=0;i<cnt;i++) for(int j=0;j<graph->node[nodeid[i]].num_connectors;j++)
      QVKR(write_descriptor_sets(graph, graph->node+nodeid[i], graph->node[nodeid[i]].connector + j, 1));
  return VK_SUCCESS;
}
