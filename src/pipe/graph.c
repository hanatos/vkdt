#include "graph.h"
#include "module.h"
#include "io.h"
#include "core/log.h"
#include "qvk/qvk.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

// convenience function to detect inputs
static inline int
dt_connector_input(dt_connector_t *c)
{
  return c->type == dt_token("read") || c->type == dt_token("sink");
}
static inline int
dt_connector_output(dt_connector_t *c)
{
  return c->type == dt_token("write") || c->type == dt_token("source");
}
static inline int
dt_node_source(dt_node_t *n)
{
  return n->connector[0].type == dt_token("source");
}
static inline int
dt_node_sink(dt_node_t *n)
{
  return n->connector[0].type == dt_token("sink");
}

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
  dt_vkalloc_init(&g->heap);
  dt_vkalloc_init(&g->heap_staging);
  g->uniform_size = 4096;

  VkCommandPoolCreateInfo cmd_pool_create_info = {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = qvk.queue_idx_compute,
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };
  QVK(vkCreateCommandPool(qvk.device, &cmd_pool_create_info, NULL, &g->command_pool));

  VkCommandBufferAllocateInfo cmd_buf_alloc_info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = g->command_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  QVK(vkAllocateCommandBuffers(qvk.device, &cmd_buf_alloc_info, &g->command_buffer));
  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    /* fence's initial state set to be signaled to make program not hang */
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  QVK(vkCreateFence(qvk.device, &fence_info, NULL, &g->command_fence));

  g->query_max = 100;
  g->query_cnt = 0;
  VkQueryPoolCreateInfo query_pool_info = {
    .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType  = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = g->query_max,
  };
  QVK(vkCreateQueryPool(qvk.device, &query_pool_info, NULL, &g->query_pool));
  g->query_pool_results = malloc(sizeof(uint64_t)*g->query_max);
}

void
dt_graph_cleanup(dt_graph_t *g)
{
  for(int i=0;i<g->num_modules;i++)
    if(g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
  dt_vkalloc_cleanup(&g->heap);
  dt_vkalloc_cleanup(&g->heap_staging);
  // TODO: destroy command buffers and pool
  // TODO: destroy descriptor pool
  vkDestroyFence(qvk.device, g->command_fence, 0);
  vkDestroyQueryPool(qvk.device, g->query_pool, NULL);
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
  if(modid0 <= -1 || modid1 <= -1 || modid0 >= graph->num_modules || modid1 >= graph->num_modules)
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
    dt_log(s_log_pipe, "[read connect] connection failed: error %d: %s", err, dt_connector_error_str(err));
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
  uint32_t lno = 0;
  while(!feof(f))
  {
    fscanf(f, "%[^\n]", line);
    if(fgetc(f) == EOF) break; // read \n
    char *c = line;
    lno++;
    dt_token_t cmd = dt_read_token(c, &c);
    if     (cmd == dt_token("module"))  { if(read_module_ascii(graph, c))     goto error;}
    else if(cmd == dt_token("connect")) { if(read_connection_ascii(graph, c)) goto error;}
    else if(cmd == dt_token("param"))   { if(read_param_ascii(graph, c))      goto error;}
    else goto error;
  }
  fclose(f);
  return 0;
error:
  dt_log(s_log_pipe|s_log_err, "failed in line %u: '%s'", lno, line);
  fclose(f);
  return 1;
}


static inline VkResult
alloc_outputs(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(dt_connector_output(c))
    { // allocate our output buffers
      VkFormat format = dt_connector_vkformat(c);
      dt_log(s_log_pipe, "%d x %d %"PRItkn, c->roi.roi_wd, c->roi.roi_ht, dt_token_str(node->name));
      VkImageCreateInfo images_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
          .width  = c->roi.roi_wd,
          .height = c->roi.roi_ht,
          .depth  = 1
        },
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT
          | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
          | VK_IMAGE_USAGE_TRANSFER_DST_BIT
          | VK_IMAGE_USAGE_SAMPLED_BIT
          | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
        // we will potentially need access to these images from both graphics and compute pipeline:
        .sharingMode           = VK_SHARING_MODE_CONCURRENT, //VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = qvk.queue_idx_compute,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
      };
      if(c->image) vkDestroyImage(qvk.device, c->image, VK_NULL_HANDLE);
      QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &c->image));
      ATTACH_LABEL_VARIABLE(img, IMAGE);

      VkMemoryRequirements mem_req;
      vkGetImageMemoryRequirements(qvk.device, c->image, &mem_req);
      if(graph->memory_type_bits != ~0 && mem_req.memoryTypeBits != graph->memory_type_bits)
        dt_log(s_log_qvk|s_log_err, "memory type bits don't match!");
      graph->memory_type_bits = mem_req.memoryTypeBits;

      assert(!(mem_req.alignment & (mem_req.alignment - 1)));

      const size_t size = dt_connector_bufsize(c);
      c->mem = dt_vkalloc(&graph->heap, mem_req.size, mem_req.alignment);
      dt_log(s_log_pipe, "allocating %.1f/%.1f MB for %"PRItkn" %"PRItkn" "
          "%"PRItkn" %"PRItkn,
          mem_req.size/(1024.0*1024.0), size/(1024.0*1024.0), dt_token_str(node->name), dt_token_str(c->name),
          dt_token_str(c->chan), dt_token_str(c->format));
      // ATTACH_LABEL_VARIABLE_NAME(qvk.images[VKPT_IMG_##_name], IMAGE, #_name);
      c->offset = c->mem->offset;
      c->mem->ref = 1;  // one user: us

      if(c->type == dt_token("source"))
      {
        // allocate staging buffer for uploading to the just allocated image
        VkBufferCreateInfo buffer_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = size,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &c->staging));

        VkMemoryRequirements buf_mem_req;
        vkGetBufferMemoryRequirements(qvk.device, c->staging, &buf_mem_req);
        if(graph->memory_type_bits_staging != ~0 && buf_mem_req.memoryTypeBits != graph->memory_type_bits_staging)
          dt_log(s_log_qvk|s_log_err, "staging memory type bits don't match!");
        graph->memory_type_bits_staging = buf_mem_req.memoryTypeBits;
        c->mem_staging    = dt_vkalloc(&graph->heap_staging, buf_mem_req.size, buf_mem_req.alignment);
        c->offset_staging = c->mem_staging->offset;
        c->size_staging   = c->mem_staging->size;
        c->mem->ref++; // add one more so we can run the pipeline starting from after upload easily
      }
    }
    else if(dt_connector_input(c))
    { // point our inputs to their counterparts:
      if(c->connected_mid >= 0)
      {
        dt_connector_t *c2 =
          graph->node[c->connected_mid].connector + c->connected_cid;
        c->mem   = c2->mem;
        c->image = c2->image;
        c->mem->ref++; // register our usage
        // image view will follow in alloc_outputs2
        if(c->type == dt_token("sink"))
        {
          // allocate staging buffer for downloading from connected input
          VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = dt_connector_bufsize(c),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
          };
          QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &c->staging));

          VkMemoryRequirements buf_mem_req;
          vkGetBufferMemoryRequirements(qvk.device, c->staging, &buf_mem_req);
          if(graph->memory_type_bits_staging != ~0 && buf_mem_req.memoryTypeBits != graph->memory_type_bits_staging)
            dt_log(s_log_qvk|s_log_err, "staging memory type bits don't match!");
          graph->memory_type_bits_staging = buf_mem_req.memoryTypeBits;
          c->mem_staging    = dt_vkalloc(&graph->heap_staging, buf_mem_req.size, buf_mem_req.alignment);
          c->offset_staging = c->mem_staging->offset;
          c->size_staging   = c->mem_staging->size;
        }
      }
    }
  }
  return VK_SUCCESS;
}

// 2nd pass, now we have images and vkDeviceMemory, let's bind images and buffers
// to memory and create descriptor sets
static inline VkResult
alloc_outputs2(dt_graph_t *graph, dt_node_t *node)
{
  if(node->dset_layout)
  { // this is not set for sink nodes
    // create descriptor set per node
    VkDescriptorSetAllocateInfo dset_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = graph->dset_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &node->dset_layout,
    };
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &node->dset));
  }

  VkDescriptorImageInfo img_info[DT_MAX_CONNECTORS] = {{0}};
  VkWriteDescriptorSet img_dset[DT_MAX_CONNECTORS] = {{0}};
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(dt_connector_output(c))
    { // allocate our output buffers
      VkFormat format = dt_connector_vkformat(c);
      vkBindImageMemory(qvk.device, c->image, graph->vkmem, c->offset);

      VkImageViewCreateInfo images_view_create_info = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = format,
        .image      = c->image,
        .subresourceRange = {
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel   = 0,
          .levelCount     = 1,
          .baseArrayLayer = 0,
          .layerCount     = 1
        },
        .components = {
          VK_COMPONENT_SWIZZLE_IDENTITY,
        },
      };
      if(c->image_view) vkDestroyImageView(qvk.device, c->image_view, VK_NULL_HANDLE);
      QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &c->image_view));
      // ATTACH_LABEL_VARIABLE_NAME(qvk.images_views[VKPT_IMG_##_name], IMAGE_VIEW, #_name);

      img_info[i].sampler     = VK_NULL_HANDLE;
      img_info[i].imageView   = c->image_view;
      img_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      img_dset[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      img_dset[i].dstSet          = node->dset;
      img_dset[i].dstBinding      = i;
      img_dset[i].dstArrayElement = 0;
      img_dset[i].descriptorCount = 1;
      img_dset[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      img_dset[i].pImageInfo      = img_info + i;

      if(c->type == dt_token("source"))
        vkBindBufferMemory(qvk.device, c->staging, graph->vkmem_staging, c->offset_staging);
    }
    else if(dt_connector_input(c))
    { // point our inputs to their counterparts:
      if(c->connected_mid >= 0)
      {
        dt_connector_t *c2 = graph->node[c->connected_mid]
          .connector+c->connected_cid;
        c->image      = c2->image;      // can't hurt to copy again
        c->image_view = c2->image_view;
        img_info[i].sampler     = qvk.tex_sampler_nearest;
        img_info[i].imageView   = c->image_view;
        img_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_dset[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[i].dstSet          = node->dset;
        img_dset[i].dstBinding      = i;
        img_dset[i].dstArrayElement = 0;
        img_dset[i].descriptorCount = 1;
        img_dset[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        img_dset[i].pImageInfo      = img_info + i;
      } // else sorry not connected, buffer will not be bound
      if(c->type == dt_token("sink"))
        vkBindBufferMemory(qvk.device, c->staging, graph->vkmem_staging, c->offset_staging);
    }
  }
  if(node->dset_layout)
    vkUpdateDescriptorSets(qvk.device, node->num_connectors, img_dset, 0, NULL);
  return VK_SUCCESS;
}

// free all buffers which we are done with now that the node
// has been processed. that is: all inputs and all of our outputs
// which aren't connected to another node.
// note that vkfree doesn't in fact free anything in case the reference count is > 0
static inline void
free_inputs(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(dt_connector_input(c) && c->connected_mid >= 0)
    {
      dt_log(s_log_pipe, "freeing %"PRItkn" %"PRItkn,
          dt_token_str(node->name), dt_token_str(c->name));
      dt_vkfree(&graph->heap, c->mem);
      // note that we keep the offset and VkImage etc around, we'll be using
      // these in consecutive runs through the pipeline and only clean up at
      // the very end. we just instruct our allocator that we're done with
      // this portion of the memory.
    }
    else if(dt_connector_output(c))
    {
      dt_log(s_log_pipe, "ref count %"PRItkn" %"PRItkn" %d",
          dt_token_str(node->name), dt_token_str(c->name),
          c->connected_mid);
      if(c->connected_mid < 1)
      {
        dt_log(s_log_pipe, "freeing unconnected %"PRItkn" %"PRItkn,
            dt_token_str(node->name), dt_token_str(c->name));
        dt_vkfree(&graph->heap, c->mem);
      }
    }
    // staging memory for sources or sinks only needed during execution once
    if(c->mem_staging)
      dt_vkfree(&graph->heap_staging, c->mem_staging);
  }
}

// propagate full buffer size from source to sink
static void
modify_roi_out(dt_graph_t *graph, dt_module_t *module)
{
  if(module->so->modify_roi_out)
  { // keep incoming roi in sync:
    for(int i=0;i<module->num_connectors;i++)
    {
      dt_connector_t *c = module->connector+i;
      if(dt_connector_input(c) && c->connected_mid >= 0 && c->connected_cid >= 0)
      {
        dt_roi_t *roi = &graph->module[c->connected_mid].connector[c->connected_cid].roi;
        c->roi = *roi;
      }
    }
    return module->so->modify_roi_out(graph, module);
  }
  // default implementation:
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
  if(module->so->modify_roi_in)
  {
    module->so->modify_roi_in(graph, module);
  }
  else
  {
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

    // all input connectors get the same roi as our output:
    for(int i=0;i<module->num_connectors;i++)
    {
      dt_connector_t *c = module->connector+i;
      if(dt_connector_input(c)) c->roi = *roi;
    }
  }

  // in any case copy over to output roi of connected modules:
  for(int i=0;i<module->num_connectors;i++)
  {
    dt_connector_t *c = module->connector+i;
    if(dt_connector_input(c))
    {
      // make sure roi is good on the outgoing connector
      if(c->connected_mid >= 0 && c->connected_cid >= 0)
      {
        dt_roi_t *roi = &graph->module[c->connected_mid].connector[c->connected_cid].roi;
        *roi = c->roi;
      }
    }
  }
}

// convenience function for debugging in gdb:
void dt_token_print(dt_token_t t)
{
  fprintf(stderr, "token: %"PRItkn"\n", dt_token_str(t));
}

static VkResult
record_command_buffer(dt_graph_t *graph, dt_node_t *node, int *runflag)
{
  // TODO: run flags and active module
  if(node->name == dt_token("demosaic"))
    *runflag = 1;
  if(!*runflag) return VK_SUCCESS; // nothing to do yet

  VkCommandBuffer cmd_buf = graph->command_buffer;
  // compute barriers on input images:
  if(dt_node_sink(node))
  { // XXX does this break export? we want it to display stuff in graphics queue
    for(int i=0;i<node->num_connectors;i++)
      if(dt_connector_input(node->connector+i))
        if(node->connector[i].connected_mid >= 0)
          BARRIER_COMPUTE_SINK(node->connector[i].image);
  }
  else
  {
    for(int i=0;i<node->num_connectors;i++)
      if(dt_connector_input(node->connector+i))
        if(node->connector[i].connected_mid >= 0)
          BARRIER_COMPUTE(node->connector[i].image);
  }

  const uint32_t wd = node->connector[0].roi.roi_wd;
  const uint32_t ht = node->connector[0].roi.roi_ht;
  VkBufferImageCopy regions = {
    .bufferOffset      = 0,
    .bufferRowLength   = 0,
    .bufferImageHeight = 0,
    .imageSubresource  = {
      .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel        = 0,
      .baseArrayLayer  = 0,
      .layerCount      = 1
    },
    .imageOffset = {0},
    .imageExtent = {
      .width  = wd,
      .height = ht,
      .depth  = 1,
    },
  };
  if(dt_node_sink(node) && node->module->so->write_sink)
  { // only schedule copy back if the node actually asks for it
    vkCmdCopyImageToBuffer(
        cmd_buf,
        node->connector[0].image,
        VK_IMAGE_LAYOUT_GENERAL,
        node->connector[0].staging,
        1, &regions);
    BARRIER_COMPUTE_BUFFER(node->connector[0].staging);
  }
  else if(dt_node_source(node))
  {
    vkCmdCopyBufferToImage(
        cmd_buf,
        node->connector[0].staging,
        node->connector[0].image,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1, &regions);
    BARRIER_COMPUTE(node->connector[0].image);
  }

  // only non-sink and non-source nodes have a pipeline:
  if(!node->pipeline) return VK_SUCCESS;

  // add our global uniforms:
  VkDescriptorSet desc_sets[] = {
    graph->uniform_dset,
    node->dset,
  };

  // push profiler start
  if(graph->query_cnt < graph->query_max)
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        graph->query_pool, graph->query_cnt++);

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, node->pipeline);
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
    node->pipeline_layout, 0, LENGTH(desc_sets), desc_sets, 0, 0);

  // update some buffers:
  // vkCmdPushConstants(cmd_buf, pipeline_layout_atrous,
      // VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);
  uint8_t uniform_buf[4096]; // max would be 65k
  // can only be 65k or else we need to upload through staging etc
  // offset and size have to be multiples of 4
  size_t pos = 0;
  for(int i=0;i<node->num_connectors;i++)
  {
    memcpy(uniform_buf + pos, &node->connector[i].roi, sizeof(dt_roi_t));
    pos += ((sizeof(dt_roi_t)+15)/16) * 16; // needs vec4 alignment
  }
  vkCmdUpdateBuffer(cmd_buf, graph->uniform_buffer, 0, pos, uniform_buf);
  BARRIER_COMPUTE_BUFFER(graph->uniform_buffer);

  vkCmdDispatch(cmd_buf,
      (node->wd + 31) / 32,
      (node->ht + 31) / 32,
       node->dp);

  // get a profiler timestamp:
  if(graph->query_cnt < graph->query_max)
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        graph->query_pool, graph->query_cnt++);
  return VK_SUCCESS;
}

// TODO: put into qvk_utils?
// TODO: dedup with what's in qvk.c
static inline void *
read_file(const char *filename, size_t *len)
{
  FILE *f = fopen(filename, "rb");
  if(!f)
  {
    dt_log(s_log_qvk|s_log_err, "failed to read shader '%s': %s!",
        filename, strerror(errno));
    return 0;
  }
  fseek(f, 0, SEEK_END);
  const size_t filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *file = malloc(filesize+1);

  size_t rd = fread(file, sizeof(char), filesize, f);
  file[filesize] = 0;
  if(rd != filesize)
  {
    free(file);
    file = 0;
    fclose(f);
    return 0;
  }
  if(len) *len = filesize;
  fclose(f);
  return file;
}
// end TODO: put into qvk_utils

// default callback for create nodes: pretty much copy the module.
// also create vulkan pipeline and load spir-v portion of the compute shader.
static VkResult
create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  if(module->so->create_nodes) return module->so->create_nodes(graph, module);
  assert(graph->num_nodes < graph->max_nodes);
  const int nodeid = graph->num_nodes++;
  dt_node_t *node = graph->node + nodeid;
  int ctxnid = -1;
  dt_node_t *ctxn = 0;

  node->name = module->name;
  node->kernel = dt_token("main");
  node->num_connectors = module->num_connectors;
  node->module = module;

  // make sure we don't follow garbage pointers. pure sink or source nodes
  // don't run a pipeline. and hence have no descriptor sets to construct. they
  // do, however, have input or output images allocated for them.
  node->pipeline = 0;
  node->dset_layout = 0;
  node->wd = node->ht = node->dp = 0;
  // determine kernel dimensions:
  int output = dt_module_get_connector(module, dt_token("output"));
  // output == -1 means we must be a sink, anyways there won't be any more glsl
  // to be run here!
  if(output >= 0)
  {
    dt_roi_t *roi = &module->connector[output].roi;
    node->wd = roi->roi_wd;
    node->ht = roi->roi_ht;
    node->dp = 1;
  }

  // TODO: if there is a ctx buffer, we'll need to run it through
  // TODO: the module, too. as a default case, trick the node into
  // TODO: running twice with a different roi
#if 0
  // do we need to output a context buffer?
  for(int i=0;i<module->num_connectors;i++)
  {
    if(module->connector[i].roi.ctx_wd > 0)
    {
      ctxnid = graph->num_nodes++;
      ctxn = graph->node + ctxnid;
      break;
    }
  }
#endif

  // we'll bind our buffers in the same order as in the connectors file.
  VkDescriptorSetLayoutBinding bindings[DT_MAX_CONNECTORS] = {{0}};
  for(int i=0;i<module->num_connectors;i++)
  {
    module->connected_nodeid[i] = nodeid;
    node->connector[i] = module->connector[i];
    // update the connection node id to point to the node inside the module
    // associated with the given output connector:
    if(dt_connector_input(node->connector+i) &&
        module->connector[i].connected_mid >= 0)
      node->connector[i].connected_mid = graph->module[
        module->connector[i].connected_mid].connected_nodeid[i];

    bindings[i].binding = i;
    if(dt_connector_input(node->connector+i))
    {
      graph->dset_cnt_image_read ++;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else
    {
      graph->dset_cnt_image_write ++;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    // this would be storage buffers:
    // graph->dset_cnt_buffer ++;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_ALL; // or just compute? // VK_SHADER_STAGE_COMPUTE_BIT
    bindings[i].pImmutableSamplers = 0;
  }

  // create a descriptor set layout
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = module->num_connectors,
    .pBindings    = bindings,
  };
  QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &node->dset_layout));

  // a sink or a source does not need a pipeline to be run.
  // note, however, that a sink does need a descriptor set, as we want to bind
  // it to imgui textures later on.
  if(dt_node_sink(node) || dt_node_source(node))
    return VK_SUCCESS;

  // create the pipeline layout
  
  VkDescriptorSetLayout dset_layout[] = {
    graph->uniform_dset_layout,
    node->dset_layout,
  };
  VkPipelineLayoutCreateInfo layout_info = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount         = LENGTH(dset_layout),
    .pSetLayouts            = dset_layout,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges    = 0,
  };
  QVKR(vkCreatePipelineLayout(qvk.device, &layout_info, 0, &node->pipeline_layout));

  // create the compute shader stage
  char filename[1024] = {0};
  snprintf(filename, sizeof(filename), "modules/%"PRItkn"/%"PRItkn".spv",
      dt_token_str(node->name), dt_token_str(node->kernel));

  size_t len;
  void *data = read_file(filename, &len);
  if(!data) return VK_ERROR_INVALID_EXTERNAL_HANDLE;

  VkShaderModule shader_module;
  VkShaderModuleCreateInfo sm_info = {
    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode    = data
  };
  QVKR(vkCreateShaderModule(qvk.device, &sm_info, 0, &shader_module));
  free(data);

  // TODO: cache on module->so ?
  VkPipelineShaderStageCreateInfo stage_info = {
    .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
    .pSpecializationInfo = 0,//&info;
    .pName               = "main", // XXX really?
    .module              = shader_module,
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

  return VK_SUCCESS;
}


// TODO: rip apart into pieces that only update the essential minimum.
// that is: only change params like roi offsets and node push
// constants or uniform buffers, or input image.

// tasks:
// vulkan:
// memory allocations, images, imageviews, dset pool, descriptor sets
// record command buffer
// pipe:
// modules: roi out, roi in, create nodes (may depend on params, but not usually!)
// nodes allocate memory
// upload sources
// download sinks, if needed on cpu
// passes:
// 1) roi out
// 2) roi in + create nodes
// -- tiling/mem check goes here
// 3) alloc + free (images)
// 4) alloc2 (dset, imageviews) + record command buf (upload params)
// 5) upload, queue, wait, download

// change events:
// new input image (5)
// - upload source, re-run the rest unchanged
// changed parameters (4) (2) (1)
// - cached input image for active module? run only after this
// - need to re-create nodes (wavelet scales, demosaic mode)?
// - did even roi out change based on params
// changed roi zoom/pan (.) (4) (2)
// - have full image? nothing to do
// - negotiate roi
// - need to re-create nodes?
// - allocation sizes unchanged (pan)? just re-run
// - alloc changed (lens distortions)? re-alloc mem and images
// attached new display/output
// - keep nodes, add new ones (or just re-create)
// - re-alloc

VkResult dt_graph_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run)
{
  // can be only one output sink node that determines ROI. but we can totally
  // have more than one sink to pull in nodes for. we have to execute some of
  // this multiple times. also we have a marker on nodes/modules that we
  // already traversed. there might also be cycles on the module level.
  uint8_t mark[200] = {0};
  assert(graph->num_modules < sizeof(mark));
  // just find first sink node:
  int sink_module_id = 0;
  for(int i=0;i<graph->num_modules;i++)
    if(graph->module[i].connector[0].type == dt_token("sink"))
    { sink_module_id = i; break; }

  if(run & s_graph_run_alloc_dset)
  {
    // init layout of uniform descriptor set:
    VkDescriptorSetLayoutBinding bindings = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL,
    };
    VkDescriptorSetLayoutCreateInfo dset_layout_info = {
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings    = &bindings,
    };
    QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &graph->uniform_dset_layout));
  }
  graph->query_cnt = 0;

{ // module scope
  dt_module_t *const arr = graph->module;
  // ==============================================
  // first pass: find output rois
  // ==============================================
  // execute after all inputs have been traversed:
  // "int curr" will be the current node
  // walk all inputs and determine roi on all outputs
  int start_node_id = sink_module_id;
  if(run & s_graph_run_roi_out)
  {
#define TRAVERSE_POST \
    modify_roi_out(graph, arr+curr);
#include "graph-traverse.inc"
  }

  // now we don't always want the full size buffer but are interested in a
  // scaled or cropped sub-region. actually this step is performed
  // transparently in the sink module's modify_roi_in first thing in the
  // second pass.

  // ==============================================
  // 2nd pass: request input rois
  // and create nodes for all modules
  // ==============================================
  if(run & (s_graph_run_roi_out| s_graph_run_create_nodes))
  {
    // XXX TODO: if we do this, we may want to alloc_dset, too?
    graph->dset_cnt_image_read = 0;
    graph->dset_cnt_image_write = 0;
    graph->dset_cnt_buffer = 0;
    graph->dset_cnt_uniform = 1; // we have one global uniform for roi + params
    graph->num_nodes = 0; // delete all previous nodes XXX need to free some vk resources?
    // TODO: nuke descriptor set pool?
    start_node_id = sink_module_id;
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
  }

  // TODO: when and how are module params updated and pointed to uniform buffers?

  // TODO: forward the output rois to other branches with sinks we didn't pull for:
  // XXX
} // end scope, done with modules


  assert(graph->num_nodes < sizeof(mark));

  // free pipeline resources if previously allocated anything:
  dt_vkalloc_nuke(&graph->heap);
  dt_vkalloc_nuke(&graph->heap_staging);
  // TODO: also goes with potential leftovers from vulkan!
  // TODO: clean memory allocation and descriptor pool

{ // node scope
  dt_node_t *const arr = graph->node;
  int sink_node_id = 0;
  for(int i=0;i<graph->num_nodes;i++)
    if(graph->node[i].connector[0].type == dt_token("sink"))
    { sink_node_id = i; break; }
  int start_node_id = sink_node_id;

  // ==============================================
  // TODO: tiling:
  // ==============================================
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

  // ==============================================
  // 1st pass alloc and free, detect cycles
  // ==============================================
  if(run & s_graph_run_alloc_free)
  {
    graph->memory_type_bits = ~0u;
    graph->memory_type_bits_staging = ~0u;
    memset(mark, 0, sizeof(mark));
#define TRAVERSE_POST\
    QVKR(alloc_outputs(graph, arr+curr));\
    free_inputs       (graph, arr+curr);
#define TRAVERSE_CYCLE\
    dt_log(s_log_pipe, "cycle %"PRItkn"->%"PRItkn"!", dt_token_str(arr[curr].name), dt_token_str(arr[el].name));\
    dt_node_connect(graph, -1,-1, curr, i);
#include "graph-traverse.inc"
  }

  // TODO: reuse previous allocation if any and big enough
  // XXX check sizes and runflags!
  if(!graph->vkmem)
  {
    // image data to pass between nodes
    VkMemoryAllocateInfo mem_alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->heap.vmsize,
      .memoryTypeIndex = qvk_get_memory_type(graph->memory_type_bits,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->vkmem));

    // XXX TODO: separate check for staging mem
    // staging memory to copy to and from device
    VkMemoryAllocateInfo mem_alloc_info_staging = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->heap_staging.vmsize,
      .memoryTypeIndex = qvk_get_memory_type(graph->memory_type_bits_staging,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info_staging, 0, &graph->vkmem_staging));

    // TODO: XXX separate check for uniforms
    // uniform data to pass parameters
    VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = graph->uniform_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &graph->uniform_buffer));
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(qvk.device, graph->uniform_buffer, &mem_req);
    VkMemoryAllocateInfo mem_alloc_info_uniform = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->uniform_size,
      .memoryTypeIndex = qvk_get_memory_type(mem_req.memoryTypeBits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info_uniform, 0, &graph->vkmem_uniform));
    vkBindBufferMemory(qvk.device, graph->uniform_buffer, graph->vkmem_uniform, 0);
  }

  if(run & s_graph_run_alloc_dset)
  {
    // TODO: redo if updates:
    if(graph->dset_pool)
    {
      // TODO: if already allocated and large enough, do this:
      QVKR(vkResetDescriptorPool(qvk.device, graph->dset_pool, 0));
    }
    else
      // TODO: if not big enough destroy first here
    {
      // create descriptor pool:
      VkDescriptorPoolSize pool_sizes[] = {{
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = graph->dset_cnt_image_read,
      }, {
        .type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = graph->dset_cnt_image_write,
      }, {
        .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = graph->dset_cnt_buffer,
      }, {
        .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = graph->dset_cnt_uniform,
      }};

      VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = LENGTH(pool_sizes),
        .pPoolSizes    = pool_sizes,
        .maxSets       = graph->dset_cnt_image_read + graph->dset_cnt_image_write
          + graph->dset_cnt_buffer     + graph->dset_cnt_uniform,
      };
      if(graph->dset_pool)
        vkDestroyDescriptorPool(qvk.device, graph->dset_pool, VK_NULL_HANDLE);
      QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &graph->dset_pool));
    }

    // uniform descriptor
    VkDescriptorSetAllocateInfo dset_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = graph->dset_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &graph->uniform_dset_layout,
    };
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info, &graph->uniform_dset));
    VkDescriptorBufferInfo uniform_info = {
      .buffer      = graph->uniform_buffer,
      .range       = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet buf_dset = {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = graph->uniform_dset,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = &uniform_info,
    };
    vkUpdateDescriptorSets(qvk.device, 1, &buf_dset, 0, NULL);
  }

  vkResetCommandPool(qvk.device, graph->command_pool, 0);

  // begin command buffer
  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  QVKR(vkBeginCommandBuffer(graph->command_buffer, &begin_info));

  // upload all source data to staging memory
  if(run & s_graph_run_upload_source)
  {
    uint8_t *mapped = 0;
    QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void**)&mapped));
    for(int n=0;n<graph->num_nodes;n++)
    { // for all source nodes:
      dt_node_t *node = graph->node + n;
      if(dt_node_source(node))
      {
        if(node->module->so->read_source)
          node->module->so->read_source(node->module,
              mapped + node->connector[0].offset_staging);
        else
          dt_log(s_log_err|s_log_pipe, "source node '%"PRItkn"' has no read_source() callback!",
              dt_token_str(node->name));
      }
    }
    vkUnmapMemory(qvk.device, graph->vkmem_staging);
  }

  // ==============================================
  // 2nd pass finish alloc and record commmand buf
  // ==============================================
  memset(mark, 0, sizeof(mark));
  int runflag = run & s_graph_run_upload_source ? 1 : 0;
#define TRAVERSE_POST\
  if(run & s_graph_run_alloc_dset)     QVKR(alloc_outputs2(graph, arr+curr));\
  if(run & s_graph_run_record_cmd_buf) QVKR(record_command_buffer(graph, arr+curr, &runflag));
#include "graph-traverse.inc"

} // end scope, done with nodes
  dt_log(s_log_pipe, "images : peak rss %g MB vmsize %g MB",
      graph->heap.peak_rss/(1024.0*1024.0),
      graph->heap.vmsize  /(1024.0*1024.0));

  dt_log(s_log_pipe, "staging: peak rss %g MB vmsize %g MB",
      graph->heap_staging.peak_rss/(1024.0*1024.0),
      graph->heap_staging.vmsize  /(1024.0*1024.0));

  QVKR(vkEndCommandBuffer(graph->command_buffer));

  VkSubmitInfo submit = {
    .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers    = &graph->command_buffer,
  };

  QVKR(vkQueueSubmit(qvk.queue_compute, 1, &submit, graph->command_fence));
  if(run & s_graph_run_wait_done)
    QVKR(vkWaitForFences(qvk.device, 1, &graph->command_fence, VK_TRUE, 1ul<<4));
  
  if(run & s_graph_run_download_sink)
  {
    for(int n=0;n<graph->num_nodes;n++)
    { // for all sink nodes:
      dt_node_t *node = graph->node + n;
      if(dt_node_sink(node))
      {
        // XXX hack! TODO implement proper display node
        graph->output = node;
        if(node->module->so->write_sink)
        {
          uint8_t *mapped = 0;
          QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE,
                0, (void**)&mapped));
          node->module->so->write_sink(node->module,
              mapped + node->connector[0].offset_staging);
          vkUnmapMemory(qvk.device, graph->vkmem_staging);
        }
        else
          dt_log(s_log_err|s_log_pipe, "sink node '%"PRItkn"' has no write_sink() callback!",
              dt_token_str(node->name));
      }
    }
  }

  QVKR(vkGetQueryPoolResults(qvk.device, graph->query_pool,
        0, graph->query_cnt,
        sizeof(graph->query_pool_results[0]) * graph->query_max,
        graph->query_pool_results,
        sizeof(graph->query_pool_results[0]),
        VK_QUERY_RESULT_64_BIT));
  for(int i=0;i<graph->query_cnt;i+=2)
  {
    dt_log(s_log_pipe, "query %d: %8.2g ms", i,
        (graph->query_pool_results[i+1]-
        graph->query_pool_results[i])* 1e-6);
  }
  // clean up after ourselves
  vkCmdResetQueryPool(graph->command_buffer, graph->query_pool, 0, graph->query_cnt);
  return VK_SUCCESS;
}
