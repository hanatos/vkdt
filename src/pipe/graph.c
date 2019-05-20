#include "graph.h"
#include "module.h"
#include "io.h"

#include <stdio.h>

void
dt_graph_init(dt_graph_t *g)
{
  memset(g, 0, sizeof(*g));
  // TODO: allocate params pool

  // allocate module and node buffers:
  g->max_modules = 100;
  g->module = malloc(sizeof(dt_module_t)*g->max_modules);
  g->max_nodes = 300;
  g->node = malloc(sizeof(dt_node_t)*g->max_nodes);
  dt_vkalloc_init(&g->alloc);
}

void
dt_graph_cleanup(dt_graph_t *g)
{
  dt_vkalloc_cleanup(&g->alloc);
}

// helper to read parameters from config file
static inline int
read_param_ascii(
    dt_graph_t *graph,
    char       *line)
{
#if 0
  // read module:instance:param:value x cnt
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  // TODO: grab count from declaration in module_so_t and iterate this!
  float val = dt_read_float(line, &line);
  // TODO: set parameter:
  // module_set_param(..); // XXX grab module(name,inst), grab param location from so and set floats!
#endif
  return 0;
}

// helper to read a connection information from config file
static inline int
read_connection_ascii(
    dt_graph_t *graph,
    char       *line)
{
  dt_token_t mod0  = dt_read_token(line, &line);
  dt_token_t inst0 = dt_read_token(line, &line);
  dt_token_t conn0 = dt_read_token(line, &line);
  dt_token_t mod1  = dt_read_token(line, &line);
  dt_token_t inst1 = dt_read_token(line, &line);
  dt_token_t conn1 = dt_read_token(line, &line);

  int modid0 = dt_module_get(graph, mod0, inst0);
  int modid1 = dt_module_get(graph, mod1, inst1);
  if(modid0 == -1 || modid1 == -1)
  {
    fprintf(stderr, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"\n",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    fprintf(stderr, "[read connect] no such modules %d %d\n", modid0, modid1);
    return 1;
  }
  int conid0 = dt_module_get_connector(graph->module+modid0, conn0);
  int conid1 = dt_module_get_connector(graph->module+modid1, conn1);
  int err = dt_module_connect(graph, modid0, conid0, modid1, conid1);
  if(err)
  {
    fprintf(stderr, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"\n",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    fprintf(stderr, "[read connect] connection failed: error %d\n", err);
    return err;
  }
  return 0;
}

// helper to add a new module from config file
static inline int
read_module_ascii(
    dt_graph_t *graph,
    char       *line)
{
  // TODO: how does this know it failed?
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  // discard module id, but remember error state (returns modid=-1)
  return dt_module_add(graph, name, inst) < 0;
}

// this is a public api function on the graph, it reads the full stack
int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;
  char line[2048];
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    char *c = line;
    dt_token_t cmd = dt_read_token(c, &c);
    if     (cmd == dt_token("module")  && read_module_ascii(graph, c))     goto error;
    else if(cmd == dt_token("connect") && read_connection_ascii(graph, c)) goto error;
    else if(cmd == dt_token("param")   && read_param_ascii(graph, c))      goto error;
  }
  fclose(f);
  return 0;
error:
  fclose(f);
  return 1;
}


#if 0
static inline void
dt_graph_alloc_outputs(dt_vkalloc_t *a, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
    if(node->connector[i].type == dt_token("write") ||
       node->connector[i].type == dt_token("source"))
      dt_vkalloc(node->connector[i].mem, size);
}

// free all buffers which we are done with now that the node
// has been processed. that is: all inputs and all of our outputs
// which aren't connected to another node.
// TODO: consider protected buffers: for instance for loaded raw input, cached before currently active node, ..
static inline void
dt_graph_free_inputs(dt_vkalloc_t *a, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
    // TODO: what about sink nodes? do we keep them for caching? is the node
    // responsible to copy these away?
    if(node->connector[i].type == dt_token("read") ||
       node->connector[i].type == dt_token("sink") ||
       node->connector[i].connected_mid == -1)
      dt_vkfree(node->connector[i].mem);
}
#endif

void dt_graph_setup_pipeline(
    dt_graph_t *graph)
{
  // TODO: can be only one output sink node that determines ROI.
  // TODO: but we can totally have more than one sink to pull in
  // nodes for. we have to execute some of this multiple times
  // and have a XXX marker XXX on nodes that we already traversed.
  // there might also be cycles on module level.
{ // module scope
  dt_module_t *const arr = graph->module;
  // first pass: find output rois
  // just find first sink node:
  int display_node_id = 0;
  for(int i=0;i<graph->num_modules;i++)
    if(graph->module[i].connector[0].type == dt_token("sink"))
    { display_node_id = i; break; }
  // execute after all inputs have been traversed:
  // "int curr" will be the current node
  // walk all inputs and determine roi on all outputs
  int start_node_id = display_node_id;
#define TRAVERSE_POST \
  arr[curr].so->modify_roi_out(graph, arr+curr);
#include "graph-traverse.inc"

  // TODO: in fact we want this or that roi on output after considering
  // scaling/zoom-to-fit/whatever.
  // TODO: set display_sink output size

  // TODO: 2nd pass: request input rois
  start_node_id = display_node_id;
#define TRAVERSE_PRE\
  arr[curr].so->modify_roi_in(graph, arr+curr);
#include "graph-traverse.inc"

  // TODO: create nodes for all modules
  // TODO: could this be done in the modify_roi_in pass?

  // TODO: forward the output rois to other branches with sinks we didn't pull for:
  // XXX
} // end scope, done with modules

#if 0
{ // node scope
  dt_node_t *const arr = graph->node;
  int start_node_id = display_sink; // our output for export
  // TODO: while(not happy) {
  // TODO: 3rd pass: compute memory requirements
  // TODO: if not happy: cut input roi in half or what
#define TRAVERSE_POST\
    dt_graph_alloc_outputs(allocator, arr+curr);\
    dt_graph_alloc_inputs (allocator, arr+curr);
#include "graph-traverse.inc"
  // }
  // TODO: do that one after the other for all chopped roi
  // finally: create vulkan command buffer
#define TRAVERSE_POST\
    dt_graph_alloc_outputs(allocator, arr+curr);\
    arr[curr].create_pipeline();\
    dt_graph_alloc_inputs (allocator, arr+curr);
#include "graph-traverse.inc"
} // end scope, done with nodes
#endif
}

#if 0
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
#endif
