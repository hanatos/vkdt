#pragma once

static inline void
generate_mipmaps(
    dt_graph_t           *graph,
    int                   rwd,
    int                   rht,
    dt_connector_image_t *img)
{
  if(img->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return;
  VkCommandBuffer cmd_buf = graph->command_buffer[graph->double_buffer];
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .image = img->image,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
    .subresourceRange.levelCount = 1,
  };
  int wd = img->wd > 0 ? img->wd : rwd;
  int ht = img->ht > 0 ? img->ht : rht;

  // put all mip levels into transfer_dst_optimal, since we want to blit onto them:
  barrier.subresourceRange.levelCount = img->mip_levels;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.oldLayout = img->layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd_buf,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
      0, NULL,
      0, NULL,
      1, &barrier);
  barrier.subresourceRange.levelCount = 1;

  for (uint32_t i = 1; i < img->mip_levels; i++)
  {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, NULL,
        0, NULL,
        1, &barrier);
    VkImageBlit blit = {
      .srcOffsets = {{ 0, 0, 0 }, { wd, ht, 1 }},
      .srcSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = i - 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
      .dstOffsets = {{ 0, 0, 0 }, { wd > 1 ? wd / 2 : 1, ht > 1 ? ht / 2 : 1, 1 }},
      .dstSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = i,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    vkCmdBlitImage(cmd_buf,
        img->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_LINEAR);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, NULL,
        0, NULL,
        1, &barrier);
    if(wd > 1) wd /= 2;
    if(ht > 1) ht /= 2;
  }
  barrier.subresourceRange.baseMipLevel = img->mip_levels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd_buf,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
      0, NULL,
      0, NULL,
      1, &barrier);
  img->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static inline VkResult
record_command_buffer(dt_graph_t *graph, dt_node_t *node, int runflag)
{
  VkCommandBuffer cmd_buf = graph->command_buffer[graph->double_buffer];

  // sanity check: are all input connectors bound?
  for(int i=0;i<node->num_connectors;i++)
    if(dt_connector_input(node->connector+i))
      if(node->connector[i].connected_mi == -1)
      {
        dt_log(s_log_err, "input %"PRItkn":%"PRItkn" not connected!",
            dt_token_str(node->name), dt_token_str(node->connector[i].name));
        return VK_INCOMPLETE;
      }

  // runflag will be 1 if we ask to upload source explicitly (the first time around)
  if((runflag == 0) && dt_node_source(node) && !node->force_upload)
  {
    for(int i=0;i<node->num_connectors;i++)
    { // this is completely retarded and just to make the layout match what we expect below
      if(!dt_connector_ssbo(node->connector+i) &&
          dt_connector_output(node->connector+i) &&
         !(node->connector[i].flags & s_conn_dynamic_array))
      {
        if(node->type == s_node_graphics)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, 0, graph->double_buffer),
              SHADER_READ_ONLY_OPTIMAL,
              COLOR_ATTACHMENT_OPTIMAL);
        else for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->double_buffer),
              SHADER_READ_ONLY_OPTIMAL,
              GENERAL);
      }
    }
    return VK_SUCCESS;
  }

  // clear the force upload flag, we are done:
  if(node->force_upload == 2) node->force_upload = 0;

  // TODO: extend the runflag to only switch on modules *after* cached input/changed parameters

  // special case for end of pipeline and thumbnail creation:
  if(graph->thumbnail_image &&
      node->name         == dt_token("thumb") &&
      node->module->inst == dt_token("main"))
  {
    VkImageCopy cp = {
      .srcSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
      .srcOffset = {0},
      .dstSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
      .dstOffset = {0},
      .extent = {
        .width  = node->connector[0].roi.wd,
        .height = node->connector[0].roi.ht,
        .depth  = 1,
      },
    };
    dt_connector_image_t *img = dt_graph_connector_image(graph,
        node-graph->node, 0, 0, 0);
    IMG_LAYOUT(img, UNDEFINED, TRANSFER_SRC_OPTIMAL);
    BARRIER_IMG_LAYOUT(graph->thumbnail_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    vkCmdCopyImage(cmd_buf,
        img->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        graph->thumbnail_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &cp);
    BARRIER_IMG_LAYOUT(graph->thumbnail_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    return VK_SUCCESS;
  }

  // barriers and image layout transformations:
  // wait for our input images and transfer them to read only.
  // also wait for our output buffers to be transferred into general layout.
  // output is moved from UNDEFINED in the hope that the content can be discarded for
  // increased efficiency in this case.
  for(int i=0;i<node->num_connectors;i++)
  {
    if(dt_connector_ssbo(node->connector+i))
    { // input buffers have a barrier
      if(dt_connector_input(node->connector+i))
        for(int k=0;k<MAX(1,node->connector[i].array_length);k++)
          BARRIER_COMPUTE_BUFFER(
              dt_graph_connector_image(graph, node-graph->node, i, k,
                (node->connector[i].flags & s_conn_feedback) ?
                1-(graph->double_buffer & 1) :
                graph->double_buffer)->buffer);
      if(dt_connector_output(node->connector+i) &&
          ((node->connector[i].flags & s_conn_clear) ||
          ((node->connector[i].flags & s_conn_clear_once) && graph->frame == 0)))
        for(int k=0;k<MAX(1,node->connector[i].array_length);k++)
        {
          VkBuffer buf = dt_graph_connector_image(graph, node-graph->node, i, k,
              (node->connector[i].flags & s_conn_feedback) ?
              1-(graph->double_buffer & 1) :
              graph->double_buffer)->buffer;
          vkCmdFillBuffer(cmd_buf, buf, 0, VK_WHOLE_SIZE, 0);
          BARRIER_COMPUTE_BUFFER(buf);
        }
      // ssbo are not depending on image layouts
      continue;
    }
    // image layouts:
    if(node->connector[i].type == dt_token("read") ||
       node->connector[i].type == dt_token("sink"))
    {
      // this needs to prepare the frame we're actually reading.
      // for feedback connections, this is crossed over.
      if(!((node->connector[i].type == dt_token("sink")) && node->module->so->write_sink))
      {
        if(!(node->connector[i].flags & s_conn_dynamic_array))
        {
          for(int k=0;k<MAX(1,node->connector[i].array_length);k++)
          {
            if(node->connector[i].flags & s_conn_mipmap)
            { // we have mipmaps on this (display) node, can't simply transition:
              generate_mipmaps(graph, 
                  node->connector[i].roi.wd,
                  node->connector[i].roi.ht,
                  dt_graph_connector_image(graph, node-graph->node, i, k,
                    (node->connector[i].flags & s_conn_feedback) ?
                    1-(graph->double_buffer & 1) :
                    graph->double_buffer));
            }
            else
            {
              IMG_LAYOUT(
                  dt_graph_connector_image(graph, node-graph->node, i, k,
                    (node->connector[i].flags & s_conn_feedback) ?
                    1-(graph->double_buffer & 1) :
                    graph->double_buffer),
                  GENERAL,
                  SHADER_READ_ONLY_OPTIMAL);
            }
          }
        }
      }
      else for(int k=0;k<MAX(1,node->connector[i].array_length);k++)
        IMG_LAYOUT(
            dt_graph_connector_image(graph, node-graph->node, i, k, graph->double_buffer),
            GENERAL,
            TRANSFER_SRC_OPTIMAL);
    }
    else if(dt_connector_output(node->connector+i))
    { // layout transition on the output to general
      if((node->connector[i].flags & s_conn_clear) ||
        ((node->connector[i].flags & s_conn_clear_once) && (graph->frame == 0)))
      { // clear buffer?
        for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
        {
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->double_buffer),
              UNDEFINED,//SHADER_READ_ONLY_OPTIMAL,
              TRANSFER_DST_OPTIMAL);
          // in case clearing is requested, zero out the image:
          VkClearColorValue col = {{0}};
          VkImageSubresourceRange range = {
            .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel    = 0,
            .levelCount      = 1,
            .baseArrayLayer  = 0,
            .layerCount      = 1
          };
          vkCmdClearColorImage(
              cmd_buf,
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->double_buffer)->image,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              &col,
              1,
              &range);
        }
        if(node->type == s_node_graphics)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, 0, graph->double_buffer),
              TRANSFER_DST_OPTIMAL,
              COLOR_ATTACHMENT_OPTIMAL);
        else for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->double_buffer),
              TRANSFER_DST_OPTIMAL,
              GENERAL);
      }
      else
      {
        if(node->type == s_node_graphics)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, 0, graph->double_buffer),
              UNDEFINED,
              COLOR_ATTACHMENT_OPTIMAL);
        else if(!(node->connector[i].flags & s_conn_dynamic_array)) for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
          // IMG_LAYOUT does not implement a barrier in case the dst layout is already
          // GENERAL. this is a problem for "modify" image connectors.
          // "modify" connectors also potentially transition from GENERAL to GENERAL
          // so we'd need a "CURRENT" instead of "UNDEFINED"
          // this only works here because "write" connectors write from scratch
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph, node-graph->node, i, k, graph->double_buffer);
          if(node->connector[i].type == dt_token("modify"))
          {
            BARRIER_IMG_LAYOUT(img->image, img->layout, VK_IMAGE_LAYOUT_GENERAL, img->mip_levels);
            img->layout = VK_IMAGE_LAYOUT_GENERAL;
          }
          else
            IMG_LAYOUT(
              img,
              UNDEFINED,
              GENERAL);
        }
      }
    }
  }

  const uint32_t wd = node->connector[0].roi.wd;
  const uint32_t ht = node->connector[0].roi.ht;
  VkBufferImageCopy regions[] = {{
    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .imageSubresource.layerCount = 1,
    .imageExtent = { wd, ht, 1 },
  },{
    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT,
    .imageSubresource.layerCount = 1,
    .imageExtent = { wd, ht, 1 },
  },{
    .bufferOffset = dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer)->plane1_offset,
    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT,
    .imageSubresource.layerCount = 1,
    .imageExtent = { wd / 2, ht / 2, 1 },
  }};
  const int f = graph->double_buffer;
  const int yuv = node->connector[0].format == dt_token("yuv");
  if(dt_node_sink(node) && node->module->so->write_sink)
  { // only schedule copy back if the node actually asks for it
    if(dt_connector_ssbo(node->connector+0))
    {
      VkBufferCopy bufreg = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = dt_connector_bufsize(node->connector, wd, ht),
      };
      vkCmdCopyBuffer(
          cmd_buf,
          dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer)->buffer,
          node->connector[0].staging[graph->double_buffer],
          1, &bufreg);
      BARRIER_COMPUTE_BUFFER(node->connector[0].staging[graph->double_buffer]);
    }
    else
    {
      vkCmdCopyImageToBuffer(
          cmd_buf,
          dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer)->image,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          node->connector[0].staging[graph->double_buffer],
          yuv ? 2 : 1, yuv ? regions+1 : regions);
      BARRIER_COMPUTE_BUFFER(node->connector[0].staging[graph->double_buffer]);
    }
  }
  else if(dt_node_source(node) &&
         !dt_connector_ssbo(node->connector+0) && // ssbo source nodes use staging memory and thus don't need a copy.
         (node->connector[0].array_length <= 1))  // arrays share the staging buffer, are handled by iterating read_source()
  {
    // push profiler start
    if(graph->query[f].cnt < graph->query[f].max)
    {
      vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          graph->query[f].pool, graph->query[f].cnt);
      graph->query[f].name  [graph->query[f].cnt  ] = node->name;
      graph->query[f].kernel[graph->query[f].cnt++] = node->kernel;
    }
    IMG_LAYOUT(
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer),
        UNDEFINED,
        TRANSFER_DST_OPTIMAL);
    if(wd > 0 && ht > 0)
      vkCmdCopyBufferToImage(
        cmd_buf,
        node->connector[0].staging[graph->double_buffer],
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer)->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        yuv ? 2 : 1, yuv ? regions+1 : regions);
    IMG_LAYOUT(
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer),
        TRANSFER_DST_OPTIMAL,
        SHADER_READ_ONLY_OPTIMAL);
    // get a profiler timestamp:
    if(graph->query[f].cnt < graph->query[f].max)
    {
      vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          graph->query[f].pool, graph->query[f].cnt);
      graph->query[f].name  [graph->query[f].cnt  ] = node->name;
      graph->query[f].kernel[graph->query[f].cnt++] = node->kernel;
    }
  }
  else if(dt_node_source(node) &&
         dt_connector_ssbo(node->connector+0) && // ssbo source node just needs a barrier on the staging memory
         (node->connector[0].array_length <= 1)) // arrays share the staging buffer, are handled by iterating read_source()
  {
    BARRIER_COMPUTE_BUFFER(dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->double_buffer)->buffer);
  }

  // only non-sink and non-source nodes have a pipeline:
  if(!node->pipeline) return VK_SUCCESS;

  // push profiler start
  if(graph->query[f].cnt < graph->query[f].max)
  {
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        graph->query[f].pool, graph->query[f].cnt);
    graph->query[f].name  [graph->query[f].cnt  ] = node->name;
    graph->query[f].kernel[graph->query[f].cnt++] = node->kernel;
  }

  // compute or graphics pipeline?
  int draw = -1;
  if(node->type == s_node_graphics)
    for(int k=0;k<node->num_connectors;k++)
      if(dt_connector_output(node->connector+k)) { draw = k; break; }

  if(draw != -1)
  {
    VkClearValue clear_color = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkRenderPassBeginInfo render_pass_info = {
      .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass        = node->draw_render_pass,
      .framebuffer       = node->draw_framebuffer,
      .renderArea.offset = { 0, 0 },
      .renderArea.extent = {
        .width  = node->connector[draw].roi.wd,
        .height = node->connector[draw].roi.ht },
      .clearValueCount   = 1,
      .pClearValues      = &clear_color
    };
    vkCmdBeginRenderPass(cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // combine all descriptor sets:
  VkDescriptorSet desc_sets[] = {
    node->uniform_dset[f],
    node->dset[f],
    graph->rt.dset[f],
  };
  const int desc_sets_cnt = LENGTH(desc_sets) - 1 + dt_raytrace_present(graph);

  if(draw != -1)
  {
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, node->pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
      node->pipeline_layout, 0, desc_sets_cnt, desc_sets, 0, 0);
  }
  else
  {
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, node->pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
      node->pipeline_layout, 0, desc_sets_cnt, desc_sets, 0, 0);
  }

  // update some buffers:
  if(node->push_constant_size)
    vkCmdPushConstants(cmd_buf, node->pipeline_layout,
        VK_SHADER_STAGE_ALL, 0, node->push_constant_size, node->push_constant);

  if(draw == -1)
  {
    vkCmdDispatch(cmd_buf,
        (node->wd + DT_LOCAL_SIZE_X - 1) / DT_LOCAL_SIZE_X,
        (node->ht + DT_LOCAL_SIZE_Y - 1) / DT_LOCAL_SIZE_Y,
         node->dp);
  }
  else
  {
    const int pi = dt_module_get_param(node->module->so, dt_token("draw"));
    const int32_t *p_draw = dt_module_param_int(node->module, pi);

    if(p_draw[0] > 0)
      vkCmdDraw(cmd_buf, p_draw[0], 1, 0, 0);
    vkCmdEndRenderPass(cmd_buf);
  }

  // get a profiler timestamp:
  if(graph->query[f].cnt < graph->query[f].max)
  {
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        graph->query[f].pool, graph->query[f].cnt);
    graph->query[f].name  [graph->query[f].cnt  ] = node->name;
    graph->query[f].kernel[graph->query[f].cnt++] = node->kernel;
  }
  return VK_SUCCESS;
}


static inline VkResult
dt_graph_run_nodes_record_cmd(
    dt_graph_t          *graph,
    const dt_graph_run_t run,
    uint32_t            *nodeid,
    int                  cnt,
    dt_module_flags_t    module_flags)
{
  if(run & s_graph_run_record_cmd_buf)
  {
    VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    int buf_curr = graph->double_buffer;
    QVKR(vkBeginCommandBuffer(graph->command_buffer[buf_curr], &begin_info));
    graph->query[buf_curr].cnt = 0;
    vkCmdResetQueryPool(graph->command_buffer[buf_curr], graph->query[buf_curr].pool, 0, graph->query[buf_curr].max);
    double rt_beg = dt_time();
    int run_all = run & s_graph_run_upload_source;
    int run_mod = module_flags & s_module_request_build_bvh;
    if(run_all || run_mod) QVKR(dt_raytrace_record_command_buffer_accel_build(graph));
    double rt_end = dt_time();
    dt_log(s_log_perf, "create raytrace accel:\t%8.3f ms", 1000.0*(rt_end-rt_beg));
    rt_beg = rt_end;
    for(int i=0;i<cnt;i++)
    {
      VkResult res = record_command_buffer(graph, graph->node+nodeid[i], run_all ||
          (graph->node[nodeid[i]].module->flags & s_module_request_read_source));
      if(res != VK_SUCCESS)
      { // need to clean up command buffer before we quit
        QVKR(vkEndCommandBuffer(graph->command_buffer[buf_curr]));
        return res;
      }
    }
    rt_end = dt_time();
    dt_log(s_log_perf, "record command buffer:\t%8.3f ms", 1000.0*(rt_end-rt_beg));
    QVKR(vkEndCommandBuffer(graph->command_buffer[buf_curr]));
  }
  return VK_SUCCESS;
}
