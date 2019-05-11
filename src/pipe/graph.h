#pragma once
#include "node.h"

typedef struct dt_graph_t
{
  dt_node_t       *nodes;
  dt_connection_t *connections;
  dt_module_t     *modules;
  // TODO: fast mapping nodeid -> index in node list?
}
dt_graph_t;

// TODO: setup vulkan pipeline by
// TODO: querying interface functions in module.
// TODO: if possible, push params as push constants, if not allocate uniform
// TODO: buffers to copy over

// 
static inline void
traverse_node(
    dt_node_t *node,
    int dry_run,
    dt_vkmem_t **out_mem) // output: allocated outputs, TODO in some fixed order
{

}

static inline void
traverse_graph
{
// TODO: include memory allocator:
// traverse graph (DAG) depth first:

  // for all sink nodes:
  // allocate all our write buffers
  // traverse() to init our input buffers
  // free all our write buffers

  // TODO: the above may need a special optimisation step
  // TODO: for short suffixes (i.e. histogram/colour picker nodes)
  // TODO: to avoid keeping mem buffers for a long time

  // traverse:
  // if source, special callback and return output buffer
  //
  // traverse all connected input nodes
  // have all input nodes free all their unconnected write buffers
  // allocate memory for all write buffers (output + scratch)
  // pretend to process/push to command queue
  // have all input nodes free all their remaining write buffers (our input)
  // return our output buffers (TODO: transfer ownership?)

}


// TODO: wrap some functions like this in a vulkan support header:
VkCommandBuffer create_commandbuffer(VkDevice d, VkCommandPool p);


dt_graph_create_command_buffer()
{
  VkCommandBuffer cb = create_commandbuffer(device, pool);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cb, &begin_info);

  // for every module:
  // for every node:
  // TODO: for every multiplicy of the same node that iterates (for instance for wavelet scales):
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
  // bind descriptor set
  // copy uniform data
  // handle push constants
  vkCmdDispatch(cb, wd, ht, 1);
  // push memory barriers if applicable
  //

  vkEndCommandBuffer(cb);
  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cb;
  // don't need waiting/signaling semaphores

  VkFence fence;
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vkCreateFence(device, &fence_info, 0, &fence);

  vkQueueSubmit(dqueues.compute.que, 1, &submit, fence);

  vkWaitForFences(device, 1, &fence, VK_TRUE, 1ul<<40);
  vkDestroyFence(device, fence, 0);

  // TODO: reuse these for multiple images?
  vkFreeCommandBuffers(device, pool, 1, &cb);
}

// read modules from ascii or binary
dt_graph_read()
// write only modules connected to sink modules
dt_graph_write();
