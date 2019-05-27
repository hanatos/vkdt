#include "graph.h"
#include "module.h"
#include "io.h"
#include "core/log.h"
#include "qvk/qvk.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

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
    if     (cmd == dt_token("module")  && read_module_ascii(graph, c))     goto error;
    else if(cmd == dt_token("connect") && read_connection_ascii(graph, c)) goto error;
    else if(cmd == dt_token("param")   && read_param_ascii(graph, c))      goto error;
  }
  fclose(f);
  return 0;
error:
  dt_log(s_log_pipe|s_log_err, "failed in line %u: '%s'", lno, line);
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
      VkFormat format = dt_connector_vkformat(c);
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
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = qvk.queue_idx_graphics, // XXX ???
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
      };
      QVK(vkCreateImage(qvk.device, &images_create_info, NULL, &c->image));
      ATTACH_LABEL_VARIABLE(img, IMAGE);

      VkMemoryRequirements mem_req;
      vkGetImageMemoryRequirements(qvk.device, c->image, &mem_req);
      if(graph->memory_type_bits != ~0 && mem_req.memoryTypeBits != graph->memory_type_bits)
        dt_log(s_log_qvk|s_log_err, "memory type bits don't match!");
      graph->memory_type_bits = mem_req.memoryTypeBits;

      assert(!(mem_req.alignment & (mem_req.alignment - 1)));
      // XXX TODO: teach our allocator this?
      // total_size += mem_req.alignment - 1;
      // total_size &= ~(mem_req.alignment - 1);
      // total_size += mem_req.size;

      const size_t size = dt_connector_bufsize(c);
      c->mem = dt_vkalloc(&graph->alloc, mem_req.size);
      fprintf(stderr, "allocating %.1f/%.1f MB for %"PRItkn" %"PRItkn" "
          "%"PRItkn" %"PRItkn" "
          "-> %lX\n",
          mem_req.size/(1024.0*1024.0), size/(1024.0*1024.0), dt_token_str(node->name), dt_token_str(c->name),
          dt_token_str(c->chan), dt_token_str(c->format),
          (uint64_t)c->mem);
      // ATTACH_LABEL_VARIABLE_NAME(qvk.images[VKPT_IMG_##_name], IMAGE, #_name);
      c->offset = c->mem->offset;
    }
    else if(c->type == dt_token("read") ||
            c->type == dt_token("sink"))
    { // point our inputs to their counterparts:
      if(c->connected_mid >= 0)
      {
        dt_connector_t *c2 =
          graph->node[c->connected_mid].connector + c->connected_cid;
        c->mem        = c2->mem;
        c->image      = c2->image;
        // c->image_view = c2->image_view;
      }
    }
  }
}

// 2nd pass, now we have images and vkDeviceMemory
static inline void
alloc_outputs2(dt_graph_t *graph, dt_node_t *node)
{
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(c->type == dt_token("write") ||
       c->type == dt_token("source"))
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
      QVK(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &c->image_view));
      // ATTACH_LABEL_VARIABLE_NAME(qvk.images_views[VKPT_IMG_##_name], IMAGE_VIEW, #_name);

  // TODO create descriptor set per node
#if 0
  VkDescriptorSetAllocateInfo dset_info = {};
  dset_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  dset_info.descriptorPool = graph->pool;
  dset_info.descriptorSetCount = 1;
  dset_info.pSetLayouts = &node->dset_layout;

  // TODO store on node? or on graph? in one big array?
  VkDescriptorSet dset;
  QVK(vkAllocateDescriptorSets(qvk.device, &dset_info, &dset));
#endif

// TODO: this could be done after memory allocation, in alloc2:
#if 0 // even later: instantiate descriptor sets:
  VkDescriptorBufferInfo dset_infos[4] = {{0}};
  dset_infos[0].buffer = ub;
  dset_infos[0].range = VK_WHOLE_SIZE;

  dset_infos[1].buffer = bin;
  dset_infos[1].range = VK_WHOLE_SIZE;

  dset_infos[2].buffer = bout;
  dset_infos[2].range = VK_WHOLE_SIZE;

  dset_infos[3].buffer = bout2;
  dset_infos[3].range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writes[8] = {{0}};
  // ping:
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = ds;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &dset_infos[0];

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = ds;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].descriptorCount = 1;
  writes[1].pBufferInfo = &dset_infos[1];

  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = ds;
  writes[2].dstBinding = 2;
  writes[2].dstArrayElement = 0;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[2].descriptorCount = 1;
  writes[2].pBufferInfo = &dset_infos[2];
  ...

  vkUpdateDescriptorSets(qvk.device, 8, writes, 0, 0);
#endif

      // TODO: also see the label attachment thing if that's possible on intel
      // TODO: create descriptor set with layout binding as connector id (+uniform buf?)
      // vkUpdateDescriptorSets
#if 0
      #define IMG_DO(_name, ...) \
    [VKPT_IMG_##_name] = { \
        .sampler     = VK_NULL_HANDLE, \
        .imageView   = qvk.images_views[VKPT_IMG_##_name], \
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL \
    },
    VkDescriptorImageInfo desc_output_img_info[] = {
        LIST_IMAGES
    };
#undef IMG_DO

    VkDescriptorImageInfo img_info[] = {
#define IMG_DO(_name, ...) \
        [VKPT_IMG_##_name] = { \
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
            .imageView   = qvk.images_views[VKPT_IMG_##_name], \
            .sampler     = tex_sampler, \
        },

        LIST_IMAGES
    };
#undef IMG_DO

    /* create information to update descriptor sets */
    VkWriteDescriptorSet output_img_write[] = {
#define IMG_DO(_name, _binding, ...) \
        [VKPT_IMG_##_name] = { \
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, \
            .dstSet          = qvk.desc_set_textures, \
            .dstBinding      = BINDING_OFFSET_IMAGES + _binding, \
            .dstArrayElement = 0, \
            .descriptorCount = 1, \
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
            .pImageInfo      = desc_output_img_info + VKPT_IMG_##_name, \
        },
  LIST_IMAGES
#undef IMG_DO
#define IMG_DO(_name, _binding, ...) \
        [VKPT_IMG_##_name + NUM_VKPT_IMAGES] = { \
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, \
            .dstSet          = qvk.desc_set_textures, \
            .dstBinding      = BINDING_OFFSET_TEXTURES + _binding, \
            .dstArrayElement = 0, \
            .descriptorCount = 1, \
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
            .pImageInfo      = img_info + VKPT_IMG_##_name, \
        },
    LIST_IMAGES
#undef IMG_DO
    };

    vkUpdateDescriptorSets(qvk.device, LENGTH(output_img_write), output_img_write, 0, NULL);
#endif
    }
    else if(c->type == dt_token("read") ||
            c->type == dt_token("sink"))
    { // point our inputs to their counterparts:
      if(c->connected_mid >= 0)
      {
        dt_connector_t *c2 = graph->node[c->connected_mid]
          .connector+c->connected_cid;
        c->image      = c2->image;      // can't hurt to copy again
        c->image_view = c2->image_view;
      }
    }
  }
}

// free all buffers which we are done with now that the node
// has been processed. that is: all inputs and all of our outputs
// which aren't connected to another node.
// TODO: consider protected buffers: for instance for loaded raw input, cached before currently active node, ..
// TODO: probably facilitate via reference counting, the cache would be a user, too.
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
      // FIXME: we need a reference count on this buffer, there might be others
      // FIXME: reading from the same resource in case the graph has a fork here
      fprintf(stderr, "freeing %"PRItkn" %"PRItkn" %lX\n",
          dt_token_str(node->name), dt_token_str(c->name), (uint64_t)c->mem);
      dt_vkfree(&graph->alloc, c->mem);
      // note that we keep the offset and VkImage etc around, we'll be using
      // these in consecutive runs through the pipeline and only clean up at
      // the very end. we just instruct our allocator that we're done with
      // this portion of the memory.
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

static int
record_command_buffer(dt_graph_t *graph, dt_node_t *node)
{
  // TODO: steal from svgf vulkan implementation:
  // compute barriers on input images corresponding to vkmem allocations
  // TODO: what are our images and how do we allocate them?
  // push profiler start
  // bind pipeline
  // bind descriptor sets
  // push constants
  // dispatch pipeline
  // stop profiler query
  return 0;
}

// TODO: put into qvk_utils?
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
static int
create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  if(module->so->create_nodes) return module->so->create_nodes(graph, module);
  assert(graph->num_nodes < graph->max_nodes);
  const int nodeid = graph->num_nodes++;
  dt_node_t *node = graph->node + nodeid;

  node->name = module->name;
  node->kernel = dt_token("main");
  node->num_connectors = module->num_connectors;

  // we'll bind our buffers in the same order as in the connectors file.
  // binding 0 will be the global uniform buffer, containing a well specified
  // region of interest bit, as well as the floating point array of module
  // params.
  VkDescriptorSetLayoutBinding bindings[DT_MAX_CONNECTORS+1] =
  { { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0 }, };
  graph->dset_cnt_uniform++;
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

    bindings[i].binding = i+1;
    if(node->connector[i].type == dt_token("read") ||
       node->connector[i].type == dt_token("sink"))
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

  // load spv kernel and pipeline
#if 0 // load platform dependent constants and layout, in case we need it:
  uint8_t shader_const_data[1024];
  size_t shader_const_data_size = 0;
  VkSpecializationMapEntry shader_const_map[1024]; // {id, start, size}
  uint32_t shader_const_map_cnt = 0;
  VkSpecializationInfo info = {
    .mapEntryCount = shader_const_map_cnt,
    .pMapEntries = shader_const_map,
    .dataSize = shader_const_data_size,
    .pData = shader_const_data
  };
#endif

  // create a descriptor set layout
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = module->num_connectors + 1,
    .pBindings    = bindings,
  };
  QVK(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &node->dset_layout));

  // create the pipeline layout
  VkPipelineLayoutCreateInfo layout_info = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount         = 1,
    .pSetLayouts            = &node->dset_layout,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges    = 0,
  };
  QVK(vkCreatePipelineLayout(qvk.device, &layout_info, 0, &node->pipeline_layout));

  // create the compute shader stage
  char filename[1024] = {0};
  snprintf(filename, sizeof(filename), "modules/%"PRItkn"/%"PRItkn".spv",
      dt_token_str(node->name), dt_token_str(node->kernel));

  size_t len;
  void *data = read_file(filename, &len);
  if(!data) return 1;

  VkShaderModule shader_module;
  VkShaderModuleCreateInfo sm_info = {
    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode    = data
  };
  QVK(vkCreateShaderModule(qvk.device, &sm_info, 0, &shader_module));
  free(data);

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
  QVK(vkCreateComputePipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &node->pipeline));

  // we don't need the module any more
  vkDestroyShaderModule(qvk.device, stage_info.module, 0);

  // TODO: set some init_good flag on the node
  return 0;
}


// TODO: rip apart into pieces that only update the essential minimum.
// that is: only change params like roi offsets and node push
// constants or uniform buffers, or input image.

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
  graph->dset_cnt_image_read = 0;
  graph->dset_cnt_image_write = 0;
  graph->dset_cnt_buffer = 0;
  graph->dset_cnt_uniform = 0;
  // TODO: nuke descriptor set pool?
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


  // free pipeline resources if previously allocated anything:
  dt_vkalloc_nuke(&graph->alloc);
  // TODO: also goes with potential leftovers from vulkan!
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

  // 1st pass alloc and free, 2nd pass alloc2 and record_command_buffer
  graph->memory_type_bits = ~0u;
  memset(mark, 0, sizeof(mark));
#define TRAVERSE_POST\
  alloc_outputs(graph, arr+curr);\
  free_inputs  (graph, arr+curr);
#define TRAVERSE_CYCLE\
  dt_log(s_log_pipe, "cycle %"PRItkn"->%"PRItkn"!", dt_token_str(arr[curr].name), dt_token_str(arr[el].name));\
  dt_node_connect(graph, -1,-1, curr, i);
#include "graph-traverse.inc"

  // TODO: reuse previous allocation if any and big enough
  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = graph->alloc.vmsize,
    .memoryTypeIndex = qvk_get_memory_type(graph->memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  QVK(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->vkmem));

  // TODO: redo if updates:
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
    .poolSizeCount = sizeof(pool_sizes)/sizeof(pool_sizes[0]),
    .pPoolSizes    = pool_sizes,
    .maxSets       = graph->dset_cnt_image_read + graph->dset_cnt_image_write
                   + graph->dset_cnt_buffer     + graph->dset_cnt_uniform,
  };
  QVK(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &graph->dset_pool));

  // XXX maybe we should have all these on the qvk struct?
  // XXX it seems we either have our own pool of we use the global
  // XXX command buffer
#if 0
  // create command buffer (TODO: if not happened yet)
  VkCommandBufferAllocateInfo cbuf_info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = qvk.command_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  QVK(vkAllocateCommandBuffers(qvk.device, &cbuf_info, &graph->command_buffer));

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  QVK(vkBeginCommandBuffer(graph->command_buffer, &begin_info));
#endif

  memset(mark, 0, sizeof(mark));
#define TRAVERSE_POST\
  alloc_outputs2(graph, arr+curr);\
  record_command_buffer(graph, arr+curr);
#define TRAVERSE_CYCLE\
  dt_log(s_log_pipe, "cycle %"PRItkn"->%"PRItkn"!", dt_token_str(arr[curr].name), dt_token_str(arr[el].name));\
  dt_node_connect(graph, -1,-1, curr, i);
#include "graph-traverse.inc"

} // end scope, done with nodes
  dt_log(s_log_pipe, "peak rss %g MB vmsize %g MB", graph->alloc.peak_rss/(1024.0*1024.0), graph->alloc.vmsize/(1024.0*1024.0));

#if 0
  QVK(vkEndCommandBuffer(graph->command_buffer));

  VkSubmitInfo submit = {
    .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers    = &graph->command_buffer,
  };

  // TODO: create all fences/barries once
  VkFence fence;
  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
  };
  QVK(vkCreateFence(qvk.device, &fence_info, 0, &fence));

  QVK(vkQueueSubmit(qvk.queue_compute, 1, &submit, fence));

  // TODO: timeout?
  QVK(vkWaitForFences(qvk.device, 1, &fence, VK_TRUE, 1ul<<40));
  vkDestroyFence(qvk.device, fence, 0);
#endif

  //TODO: now can copy back result, call function in sink nodes
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
