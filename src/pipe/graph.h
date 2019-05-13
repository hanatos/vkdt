#pragma once
#include "node.h"
#include "module.h"

// TODO: setup vulkan pipeline by
// TODO: querying interface functions in module.
// TODO: if possible, push params as push constants, if not allocate uniform
// TODO: buffers to copy over

// TODO: need to walk a few different graphs:
//       on modules and on nodes. maybe it's a good idea to nest a node inside
//       a module so we can call the same functions?
// i think we'll go for a "template" kind of approach that just walks
// the graph on anything that has "connector" members and can execute macros
// before and after descending the tree.

setup_pipeline()
{
  // TODO: can be only one output sink node that determines ROI.
  // TODO: but we can totally have more than one sink to pull in
  // nodes for. we have to execute some of this multiple times
  // and have a marker on nodes that we already traversed.
{ // module scope
  dt_module_t *const arr = graph->module;
  // first pass: find output rois
  int start_node_id = display_sink; // our output for export
  // execute after all inputs have been traversed:
  // int curr <- will be the current node
  // TODO: walk all inputs and determine roi on all outputs
#define TRAVERSE_POST \
  arr[curr].modify_roi_out();
#include "graph-traverse.inc"

  // TODO: in fact we want this or that roi on output after considering
  // scaling/zoom-to-fit/whatever.
  // TODO: set display_sink output size

  // TODO: 2nd pass: request input rois
#define TRAVERSE_PRE\
  arr[curr].modify_roi_in();
#include "graph-traverse.inc"

  // TODO: forward the output rois to other branches with sinks we didn't pull for:
  // XXX
} // end scope, done with modules

{ // node scope
  dt_node_t *const arr = graph->node;
  int start_node_id = display_sink; // our output for export
  // TODO: while(not happy) {
  // TODO: 3rd pass: compute memory requirements
  // TODO: if not happy: cut input roi in half or what
#define TRAVERSE_POST\
    arr[curr].alloc_outputs();\
    arr[curr].free_inputs();
#include "graph-traverse.inc"
  // }
  // TODO: do that one after the other for all chopped roi
  // finally: create vulkan command buffer
#define TRAVERSE_POST\
    arr[curr].alloc_outputs();\
    arr[curr].create_pipeline();\
    arr[curr].free_inputs();
#include "graph-traverse.inc"
} // end scope, done with nodes
}

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
