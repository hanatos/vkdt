#pragma once

#define IMG_LAYOUT(img, oli, nli) do {\
  if(!img->image) break;\
  VkImageLayout nl = VK_IMAGE_LAYOUT_ ## nli;\
  if(nl != img->layout)\
    BARRIER_IMG_LAYOUT(img->image, img->layout, nl);\
  img->layout = nl;\
} while(0)

// run the graph.
// upload source to GPU buffers portion.
static inline VkResult
dt_graph_run_nodes_upload(
    dt_graph_t          *graph,
    const dt_graph_run_t run,
    uint32_t            *nodeid,
    int                  cnt,
    dt_module_flags_t    module_flags,
    int                  dynamic_array)
{
  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  threads_mutex_t *mutex = 0;// graph->io_mutex; // no speed impact, maybe not needed
  if(mutex) threads_mutex_lock(mutex);
  if( dynamic_array ||
     (module_flags & s_module_request_read_source) ||
     (run & s_graph_run_upload_source))
  {
    double upload_beg = dt_time();
    uint8_t *mapped = 0;
    QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void**)&mapped));
    for(int n=0;n<graph->num_nodes;n++)
    { // for all source nodes:
      dt_node_t *node = graph->node + n;
      if(dt_node_source(node))
      {
        if(node->module->so->read_source)
        {
          int run_node = (node->flags & s_module_request_read_source) ||
                         (run & s_graph_run_upload_source);
          const int c = 0;
          if(run_node || (dynamic_array && (node->connector[c].flags & s_conn_dynamic_array)))
          {
            for(int a=0;a<MAX(1,node->connector[c].array_length);a++)
            {
              if(node->connector[c].array_req)
              {
                if(!(node->connector[c].array_req[a])) continue;
                node->connector[c].array_req[a] = 0; // clear image load request
              }
              dt_read_source_params_t p = { .node = node, .c = c, .a = a };
              size_t offset = node->connector[c].offset_staging;
              if(node->connector[c].chan == dt_token("ssbo"))
                offset = dt_graph_connector_image(graph, node-graph->node, c, a, graph->double_buffer)->offset;
              node->module->so->read_source(node->module,
                  mapped + offset, &p);
              if(node->connector[c].array_length > 1)
              {
                if(!dt_graph_connector_image(graph, node-graph->node, c, a, graph->double_buffer)->image)
                  continue;
                vkUnmapMemory(qvk.device, graph->vkmem_staging);
                const uint32_t wd = MAX(1, node->connector[c].array_dim ? node->connector[c].array_dim[2*a+0] : node->connector[c].roi.wd);
                const uint32_t ht = MAX(1, node->connector[c].array_dim ? node->connector[c].array_dim[2*a+1] : node->connector[c].roi.ht);
                VkBufferImageCopy regions[] = {{
                  .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .imageSubresource.layerCount = 1,
                  .imageExtent = { wd, ht, 1 },
                },{
                  .imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT,
                  .imageSubresource.layerCount = 1,
                  .imageExtent = { wd, ht, 1 },
                },{
                  .bufferOffset = dt_graph_connector_image(graph, node-graph->node, c, a, graph->double_buffer)->plane1_offset,
                  .imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT,
                  .imageSubresource.layerCount = 1,
                  .imageExtent = { wd / 2, ht / 2, 1 },
                }};
                const int yuv = node->connector[c].format == dt_token("yuv");
                VkCommandBuffer cmd_buf = graph->command_buffer[graph->double_buffer];
                QVKR(vkBeginCommandBuffer(cmd_buf, &begin_info));
                IMG_LAYOUT(
                    dt_graph_connector_image(graph, node-graph->node, c, a, graph->double_buffer),
                    UNDEFINED,
                    TRANSFER_DST_OPTIMAL);
                vkCmdCopyBufferToImage(
                    cmd_buf,
                    node->connector[c].staging,
                    dt_graph_connector_image(graph, node-graph->node, c, a, graph->double_buffer)->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    yuv ? 2 : 1, yuv ? regions+1 : regions);
                IMG_LAYOUT(
                    dt_graph_connector_image(graph, node-graph->node, c, a, graph->double_buffer),
                    TRANSFER_DST_OPTIMAL,
                    SHADER_READ_ONLY_OPTIMAL);
                QVKR(vkEndCommandBuffer(cmd_buf));
                // we add one more command list, locking the command buffer in this case
                graph->process_dbuffer[graph->double_buffer] = MAX(graph->process_dbuffer[0], graph->process_dbuffer[1]) + 1;
                VkTimelineSemaphoreSubmitInfo timeline_info = {
                  .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                  .signalSemaphoreValueCount = 1,
                  .pSignalSemaphoreValues    = &graph->process_dbuffer[graph->double_buffer], // lock for writing, this signal will remove the lock
                };
                VkSubmitInfo submit = {
                  .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                  .commandBufferCount   = 1,
                  .pCommandBuffers      = &cmd_buf,
                  .pNext                = &timeline_info,
                  .signalSemaphoreCount = 1,
                  .pSignalSemaphores    = &graph->semaphore_process,
                };
                QVKLR(&qvk.queue[qvk.qid[graph->queue_name]].mutex,
                    vkQueueSubmit(qvk.queue[qvk.qid[graph->queue_name]].queue, 1, &submit, 0));
                VkSemaphoreWaitInfo wait_info = {
                  .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                  .semaphoreCount = 1,
                  .pSemaphores    = &graph->semaphore_process,
                  .pValues        = &graph->process_dbuffer[graph->double_buffer],
                };
                // wait inline on our semaphore because we share the staging buf
                QVKR(vkWaitSemaphores(qvk.device, &wait_info, UINT64_MAX));
                QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void**)&mapped));
              }
            }
          }
        }
        else
          dt_log(s_log_err|s_log_pipe, "source node '%"PRItkn"' has no read_source() callback!",
              dt_token_str(node->name));
      }
    }
    vkUnmapMemory(qvk.device, graph->vkmem_staging);
    double upload_end = dt_time();
    dt_log(s_log_perf, "upload source total:\t%8.3f ms", 1000.0*(upload_end-upload_beg));
  }
  if(mutex) threads_mutex_unlock(mutex);
  return VK_SUCCESS;
}
