#pragma once

static inline VkResult
dt_graph_run_nodes_download(
    dt_graph_t          *graph,
    const dt_graph_run_t run,
    dt_module_flags_t    module_flags)
{
  // XXX FIXME: this is a race condition for multi-frames. we'll need to wait until download is complete before starting the other command buffer!
  // XXX FIXME: maybe lock the queue mutex around the whole block?
  // XXX FIXME: may need an entirely different logic block for single frame?
  if((module_flags & s_module_request_write_sink) ||
     (run & s_graph_run_download_sink))
  {
    uint8_t *mapped = 0;
    QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void**)&mapped));
    for(int n=0;n<graph->num_nodes;n++)
    { // for all sink nodes:
      dt_node_t *node = graph->node + n;
      if(dt_node_sink(node))
      {
        if(node->module->so->write_sink &&
          ((node->module->flags & s_module_request_write_sink) ||
           (run & s_graph_run_download_sink)))
        {
          dt_write_sink_params_t p = { .node = node, .c = 0, .a = 0 };
          node->module->so->write_sink(node->module,
              mapped + node->connector[0].offset_staging, &p);
        }
      }
    }
    vkUnmapMemory(qvk.device, graph->vkmem_staging);
  }
  return VK_SUCCESS;
}
