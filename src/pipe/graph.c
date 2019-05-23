#include "graph.h"
#include "module.h"
#include "io.h"
#include "core/log.h"

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
  for(int i=0;i<g->num_modules;i++)
    if(g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
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
    dt_log(s_log_pipe, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    dt_log(s_log_pipe, "[read connect] no such modules %d %d", modid0, modid1);
    return 1;
  }
  int conid0 = dt_module_get_connector(graph->module+modid0, conn0);
  int conid1 = dt_module_get_connector(graph->module+modid1, conn1);
  int err = dt_module_connect(graph, modid0, conid0, modid1, conid1);
  if(err)
  {
    dt_log(s_log_pipe, "[read connect] "
        "%"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"",
        dt_token_str(mod0), dt_token_str(inst0), dt_token_str(conn0),
        dt_token_str(mod1), dt_token_str(inst1), dt_token_str(conn1));
    dt_log(s_log_pipe, "[read connect] connection failed: error %d", err);
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


static inline void
alloc_outputs(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(c->type == dt_token("write") ||
       c->type == dt_token("source"))
    { // allocate our output buffers
      const size_t size = dt_connector_bufsize(c);
      c->mem = dt_vkalloc(&graph->alloc, size);
      fprintf(stderr, "allocating %lu bytes for %"PRItkn" %"PRItkn" -> %lX\n",
          size, dt_token_str(node->name), dt_token_str(c->name), (uint64_t)c->mem);
    }
    else if(c->type == dt_token("read") ||
            c->type == dt_token("sink"))
    { // point our inputs to their counterparts:
      if(c->connected_mid >= 0)
        c->mem = graph->node[c->connected_mid].connector[c->connected_cid].mem;
    }
  }
}

// free all buffers which we are done with now that the node
// has been processed. that is: all inputs and all of our outputs
// which aren't connected to another node.
// TODO: consider protected buffers: for instance for loaded raw input, cached before currently active node, ..
static inline void
free_inputs(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if((c->type == dt_token("read") ||
        c->type == dt_token("sink")) &&
        c->connected_mid >= 0)
    {
      fprintf(stderr, "freeing %"PRItkn" %"PRItkn" %lX\n",
          dt_token_str(node->name), dt_token_str(c->name), (uint64_t)c->mem);
      dt_vkfree(&graph->alloc, c->mem);
    }
    else if(c->type == dt_token("write") ||
            c->type == dt_token("source"))
    {
      fprintf(stderr, "ref count %"PRItkn" %"PRItkn" %d\n",
          dt_token_str(node->name), dt_token_str(c->name),
          c->connected_mid);
      if(c->connected_mid < 1)
      {
        fprintf(stderr, "freeing unconnected %"PRItkn" %"PRItkn" %lX\n",
            dt_token_str(node->name), dt_token_str(c->name), (uint64_t)c->mem);
        dt_vkfree(&graph->alloc, c->mem);
      }
    }
  }
}

// propagate full buffer size from source to sink
static void
modify_roi_out(dt_graph_t *graph, dt_module_t *module)
{
  if(module->so->modify_roi_out) return module->so->modify_roi_out(graph, module);
  // copy over roi from connector named "input" to all outputs ("write")
  int input = dt_module_get_connector(module, dt_token("input"));
  if(input < 0) return;
  dt_connector_t *c = module->connector+input;
  dt_roi_t *roi = &graph->module[c->connected_mid].connector[c->connected_cid].roi;
  c->roi = *roi; // also keep incoming roi in sync
  for(int i=0;i<module->num_connectors;i++)
  {
    if(module->connector[i].type == dt_token("write"))
    {
      module->connector[i].roi.full_wd = roi->full_wd;
      module->connector[i].roi.full_ht = roi->full_ht;
    }
  }
}

// request input region of interest from sink to source
static void
modify_roi_in(dt_graph_t *graph, dt_module_t *module)
{
  if(module->so->modify_roi_in) return module->so->modify_roi_in(graph, module);

  // propagate roi request on output module to our inputs ("read")
  int output = dt_module_get_connector(module, dt_token("output"));
  if(output == -1 && module->connector[0].type == dt_token("sink"))
  { // by default ask for it all:
    output = 0;
    dt_roi_t *r = &module->connector[0].roi;
    r->roi_wd = r->full_wd;
    r->roi_ht = r->full_ht;
    r->roi_scale = 1.0f;
  }
  if(output < 0) return;
  dt_roi_t *roi = &module->connector[output].roi;
  for(int i=0;i<module->num_connectors;i++)
  {
    if(module->connector[i].type == dt_token("read") ||
       module->connector[i].type == dt_token("sink"))
    {
      dt_connector_t *c = module->connector+i;
      c->roi = *roi;
      // make sure roi is good on the outgoing connector
      if(c->connected_mid >= 0 && c->connected_cid >= 0)
      {
        dt_roi_t *roi2 = &graph->module[c->connected_mid].connector[c->connected_cid].roi;
        *roi2 = *roi;
      }
    }
  }
}

// default callback for create nodes: pretty much copy the module
static void
create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  if(module->so->create_nodes) return module->so->create_nodes(graph, module);
  assert(graph->num_nodes < graph->max_nodes);
  const int nodeid = graph->num_nodes++;
  dt_node_t *node = graph->node + nodeid;

  node->name = module->name;
  node->kernel = module->name;
  node->num_connectors = module->num_connectors;
  for(int i=0;i<module->num_connectors;i++)
  {
    module->connected_nodeid[i] = nodeid;
    node->connector[i] = module->connector[i];
    // update the connection node id to point to the node inside the module
    // associated with the given output connector:
    if((node->connector[i].type == dt_token("read") ||
        node->connector[i].type == dt_token("sink")) &&
        module->connector[i].connected_mid >= 0)
      node->connector[i].connected_mid = graph->module[
        module->connector[i].connected_mid].connected_nodeid[i];
  }
}


void dt_graph_setup_pipeline(
    dt_graph_t *graph)
{
  // can be only one output sink node that determines ROI. but we can totally
  // have more than one sink to pull in nodes for. we have to execute some of
  // this multiple times. also we have a marker on nodes/modules that we
  // already traversed. there might also be cycles on the module level.
  uint8_t mark[200] = {0};
  assert(graph->num_modules < sizeof(mark));
{ // module scope
  dt_module_t *const arr = graph->module;
  // first pass: find output rois
  // just find first sink node:
  int sink_node_id = 0;
  for(int i=0;i<graph->num_modules;i++)
    if(graph->module[i].connector[0].type == dt_token("sink"))
    { sink_node_id = i; break; }
  // execute after all inputs have been traversed:
  // "int curr" will be the current node
  // walk all inputs and determine roi on all outputs
  int start_node_id = sink_node_id;
#define TRAVERSE_POST \
  modify_roi_out(graph, arr+curr);
#include "graph-traverse.inc"

  // now we don't always want the full size buffer but are interested in a
  // scaled or cropped sub-region. actually this step is performed
  // transparently in the sink module's modify_roi_in first thing in the
  // second pass.

  // 2nd pass: request input rois
  // and create nodes for all modules
  graph->num_nodes = 0; // delete all previous nodes XXX need to free some vk resources?
  start_node_id = sink_node_id;
  memset(mark, 0, sizeof(mark));
#define TRAVERSE_PRE\
  modify_roi_in(graph, arr+curr);
#define TRAVERSE_POST\
  create_nodes(graph, arr+curr);
  // TODO: in fact this should only be an error for default create nodes cases:
  // TODO: the others might break the cycle by pushing more nodes.
#define TRAVERSE_CYCLE\
  dt_log(s_log_pipe, "cycle %"PRItkn"->%"PRItkn"!", dt_token_str(arr[curr].name), dt_token_str(arr[el].name));\
  dt_module_connect(graph, -1,-1, curr, i);
#include "graph-traverse.inc"

  // TODO: when and how are module params updated and pointed to uniform buffers?

  // TODO: forward the output rois to other branches with sinks we didn't pull for:
  // XXX
} // end scope, done with modules

  assert(graph->num_nodes < sizeof(mark));
#if 1
{ // node scope
  dt_node_t *const arr = graph->node;
  int sink_node_id = 0;
  for(int i=0;i<graph->num_nodes;i++)
    if(graph->node[i].connector[0].type == dt_token("sink"))
    { sink_node_id = i; break; }
  int start_node_id = sink_node_id;
#if 0
  // TODO: while(not happy) {
  // TODO: 3rd pass: compute memory requirements
  // TODO: if not happy: cut input roi in half or what
  memset(mark, 0, sizeof(mark));
#define TRAVERSE_POST\
    alloc_outputs(allocator, arr+curr);\
    free_inputs (allocator, arr+curr);
#include "graph-traverse.inc"
  // }
  // TODO: do that one after the other for all chopped roi
#endif
  // finally: create vulkan command buffer
  // TODO: check quake code how to create pipelines. do we first collect info
  // about buffers, descriptor sets, and pipelines and then atomically build
  // the vulkan stuff in the core?
  memset(mark, 0, sizeof(mark));
  // XXX record_command_buffer(graph, arr+curr);
#define TRAVERSE_POST\
  alloc_outputs(graph, arr+curr);\
  free_inputs  (graph, arr+curr);
#define TRAVERSE_CYCLE\
  dt_log(s_log_pipe, "cycle %"PRItkn"->%"PRItkn"!", dt_token_str(arr[curr].name), dt_token_str(arr[el].name));\
  dt_node_connect(graph, -1,-1, curr, i);
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
