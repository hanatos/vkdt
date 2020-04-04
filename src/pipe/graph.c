#include "graph.h"
#include "graph-io.h"
#include "module.h"
#include "modules/api.h"
#include "modules/localsize.h"
#include "core/log.h"
#include "qvk/qvk.h"
#include "graph-print.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define IMG_LAYOUT(img, oli, nli) do {\
  VkImageLayout ol = VK_IMAGE_LAYOUT_ ## oli;\
  VkImageLayout nl = VK_IMAGE_LAYOUT_ ## nli;\
  if(ol == VK_IMAGE_LAYOUT_UNDEFINED || \
     ol == img->layout) {\
    if(img->layout != nl)\
      BARRIER_IMG_LAYOUT(img->image, ol, nl);\
  } else {\
    if(img->layout != nl)\
      BARRIER_IMG_LAYOUT(img->image, img->layout, nl);\
  }\
  img->layout = nl;\
} while(0)

void
dt_graph_init(dt_graph_t *g)
{
  memset(g, 0, sizeof(*g));

  g->frame_cnt = 1;

  // allocate module and node buffers:
  g->max_modules = 100;
  g->module = calloc(sizeof(dt_module_t), g->max_modules);
  g->max_nodes = 4000;
  g->node = calloc(sizeof(dt_node_t), g->max_nodes);
  dt_vkalloc_init(&g->heap, 1000, 1ul<<40); // bytesize doesn't matter
  dt_vkalloc_init(&g->heap_staging, 100, 1ul<<40);
  g->params_max = 16u<<20;
  g->params_end = 0;
  g->params_pool = calloc(sizeof(uint8_t), g->params_max);

  g->conn_image_max = 30*2*2000; // connector images for a lot of connectors
  g->conn_image_end = 0;
  g->conn_image_pool = calloc(sizeof(dt_connector_image_t), g->conn_image_max);

  VkCommandPoolCreateInfo cmd_pool_create_info = {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = g->queue_idx,
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
  };
  QVK(vkCreateFence(qvk.device, &fence_info, NULL, &g->command_fence));

  g->query_max = 2000;
  g->query_cnt = 0;
  VkQueryPoolCreateInfo query_pool_info = {
    .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType  = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = g->query_max,
  };
  QVK(vkCreateQueryPool(qvk.device, &query_pool_info, NULL, &g->query_pool));
  g->query_pool_results = malloc(sizeof(uint64_t)*g->query_max);
  g->query_name   = malloc(sizeof(dt_token_t)*g->query_max);
  g->query_kernel = malloc(sizeof(dt_token_t)*g->query_max);

  // grab default queue:
  g->queue = qvk.queue_compute;
  g->queue_idx = qvk.queue_idx_compute;

  g->lod_scale = 1;
  g->active_module = -1;
}

void
dt_graph_cleanup(dt_graph_t *g)
{
  QVK(vkDeviceWaitIdle(qvk.device));
  for(int i=0;i<g->num_modules;i++)
    if(g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
  dt_vkalloc_cleanup(&g->heap);
  dt_vkalloc_cleanup(&g->heap_staging);
  for(int i=0;i<g->conn_image_end;i++)
  {
    if(g->conn_image_pool[i].image)      vkDestroyImage(qvk.device,     g->conn_image_pool[i].image, VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, g->conn_image_pool[i].image_view, VK_NULL_HANDLE);
    g->conn_image_pool[i].image = 0;
    g->conn_image_pool[i].image_view = 0;
  }
  for(int i=0;i<g->num_nodes;i++)
  {
    for(int j=0;j<g->node[i].num_connectors;j++)
    {
      dt_connector_t *c = g->node[i].connector+j;
      if(c->staging) vkDestroyBuffer(qvk.device, c->staging, VK_NULL_HANDLE);
    }
    vkDestroyPipelineLayout     (qvk.device, g->node[i].pipeline_layout,  0);
    vkDestroyPipeline           (qvk.device, g->node[i].pipeline,         0);
    vkDestroyDescriptorSetLayout(qvk.device, g->node[i].dset_layout,      0);
    vkDestroyFramebuffer        (qvk.device, g->node[i].draw_framebuffer, 0);
    vkDestroyRenderPass         (qvk.device, g->node[i].draw_render_pass, 0);
  }
  vkDestroyDescriptorPool(qvk.device, g->dset_pool, 0);
  vkDestroyDescriptorSetLayout(qvk.device, g->uniform_dset_layout, 0);
  vkDestroyBuffer(qvk.device, g->uniform_buffer, 0);
  vkFreeMemory(qvk.device, g->vkmem, 0);
  vkFreeMemory(qvk.device, g->vkmem_staging, 0);
  vkFreeMemory(qvk.device, g->vkmem_uniform, 0);
  g->vkmem_size = g->vkmem_staging_size = g->vkmem_uniform_size = 0;
  vkDestroyFence(qvk.device, g->command_fence, 0);
  vkDestroyQueryPool(qvk.device, g->query_pool, 0);
  vkDestroyCommandPool(qvk.device, g->command_pool, 0);
  free(g->module);
  free(g->node);
  free(g->params_pool);
  free(g->conn_image_pool);
  free(g->query_pool_results);
  free(g->query_name);
  free(g->query_kernel);
}

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

VkResult
dt_graph_create_shader_module(
    dt_token_t node,
    dt_token_t kernel,
    const char *type,
    VkShaderModule *shader_module)
{
  // create the compute shader stage
  char filename[1024] = {0};
  snprintf(filename, sizeof(filename), "modules/%"PRItkn"/%"PRItkn".%s.spv",
      dt_token_str(node), dt_token_str(kernel), type);

  size_t len;
  void *data = read_file(filename, &len);
  if(!data) return VK_ERROR_INVALID_EXTERNAL_HANDLE;

  VkShaderModuleCreateInfo sm_info = {
    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = len,
    .pCode    = data
  };
  QVKR(vkCreateShaderModule(qvk.device, &sm_info, 0, shader_module));
  free(data);
  return VK_SUCCESS;
}

// allocate output buffers, also create vulkan pipeline and load spir-v portion
// of the compute shader.
// TODO: need to disentangle allocation and vulkan code here, too
// TODO: because the tiling memory allocation will want to run alloc/free many times
// TODO: but only once create the vulkan images at the end.
static inline VkResult
alloc_outputs(dt_graph_t *graph, dt_node_t *node)
{
  // create descriptor bindings and pipeline:
  // TODO: check runflags for this?
  // we'll bind our buffers in the same order as in the connectors file.
  uint32_t drawn_connector_cnt = 0;
  uint32_t drawn_connector[DT_MAX_CONNECTORS];
  VkDescriptorSetLayoutBinding bindings[DT_MAX_CONNECTORS] = {{0}};
  for(int i=0;i<node->num_connectors;i++)
  {
    bindings[i].binding = i;
    if(dt_connector_input(node->connector+i))
    {
      graph->dset_cnt_image_read += MAX(1, node->connector[i].array_length);
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else
    {
      graph->dset_cnt_image_write += MAX(1, node->connector[i].array_length);
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      if(node->type == s_node_graphics)
        drawn_connector[drawn_connector_cnt++] = i;
    }
    // this would be storage buffers:
    // graph->dset_cnt_buffer ++;
    bindings[i].descriptorCount = MAX(1, node->connector[i].array_length);
    bindings[i].stageFlags = VK_SHADER_STAGE_ALL;//COMPUTE_BIT;
    bindings[i].pImmutableSamplers = 0;
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
        .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
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
    };
    VkPushConstantRange pcrange = {
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset     = 0,
      .size       = node->push_constant_size,
    };
    VkPipelineLayoutCreateInfo layout_info = {
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = LENGTH(dset_layout),
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
      QVKR(dt_graph_create_shader_module(node->name, node->kernel, "vert", &shader_module_vert));
      QVKR(dt_graph_create_shader_module(node->name, node->kernel, "frag", &shader_module_frag));
      VkResult geom = dt_graph_create_shader_module(node->name, node->kernel, "geom", &shader_module_geom);

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
      vkDestroyShaderModule(qvk.device, shader_module_geom, 0);
      vkDestroyShaderModule(qvk.device, shader_module_frag, 0);
    }
    else
    { // create the compute shader stage
      VkShaderModule shader_module;
      QVKR(dt_graph_create_shader_module(node->name, node->kernel, "comp", &shader_module));

      // TODO: cache pipelines on module->so ?
      VkPipelineShaderStageCreateInfo stage_info = {
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
        .pSpecializationInfo = 0,
        .pName               = "main", // arbitrary entry point symbols are supported by glslangValidator, but need extra compilation, too. i think it's easier to structure code via includes then.
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
    }
  } // done with pipeline

  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(dt_connector_output(c))
    { // allocate our output buffers
      VkFormat format = dt_connector_vkformat(c);
      // dt_log(s_log_pipe, "%d x %d %"PRItkn"_%"PRItkn, c->roi.wd, c->roi.ht, dt_token_str(node->name), dt_token_str(node->kernel));
      int bc1 = c->format == dt_token("bc1");
      VkImageCreateInfo images_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
          .width  = c->roi.wd,
          .height = c->roi.ht,
          .depth  = 1
        },
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = 
          bc1 ?
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
        .queueFamilyIndexCount = 0,//2,
        .pQueueFamilyIndices   = 0,//queues,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
      };
      for(int f=0;f<c->frames;f++)
      for(int k=0;k<MAX(1,c->array_length);k++)
      {
        dt_connector_image_t *img = dt_graph_connector_image(graph,
            node - graph->node, i, k, f);
        if(img->image) vkDestroyImage(qvk.device, img->image, VK_NULL_HANDLE);
        QVKR(vkCreateImage(qvk.device, &images_create_info, NULL, &img->image));
#if 0 // nice debugging, but leaks memory
#ifdef QVK_ENABLE_VALIDATION
        char *name = malloc(100); // leak this
        snprintf(name, 100, "%"PRItkn"_%"PRItkn"_%"PRItkn"@%d", dt_token_str(node->module->name), dt_token_str(node->kernel), dt_token_str(c->name), f);
        VkDebugMarkerObjectNameInfoEXT name_info = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
          .object = (uint64_t) img->image,
          .objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT,
          .pObjectName = name,
        };
        qvkDebugMarkerSetObjectNameEXT(qvk.device, &name_info);
#endif
#endif

        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(qvk.device, img->image, &mem_req);
        if(graph->memory_type_bits != ~0 && mem_req.memoryTypeBits != graph->memory_type_bits)
          dt_log(s_log_qvk|s_log_err, "memory type bits don't match!");
        graph->memory_type_bits = mem_req.memoryTypeBits;

        assert(!(mem_req.alignment & (mem_req.alignment - 1)));

        const size_t size = dt_connector_bufsize(c);
        if(c->frames == 2 || c->type == dt_token("source")) // allocate protected memory
          img->mem = dt_vkalloc_feedback(&graph->heap, mem_req.size, mem_req.alignment);
        else
          img->mem = dt_vkalloc(&graph->heap, mem_req.size, mem_req.alignment);
        // dt_log(s_log_pipe, "allocating %.1f/%.1f MB for %"PRItkn" %"PRItkn" "
        //     "%"PRItkn" %"PRItkn,
        //     mem_req.size/(1024.0*1024.0), size/(1024.0*1024.0), dt_token_str(node->name), dt_token_str(c->name),
        //     dt_token_str(c->chan), dt_token_str(c->format));
        assert(img->mem);
        // ATTACH_LABEL_VARIABLE_NAME(qvk.images[VKPT_IMG_##_name], IMAGE, #_name);
        img->offset = img->mem->offset;
        img->size   = img->mem->size;
        // reference counting. we can't just do a ref++ here because we will
        // free directly after and wouldn't know which node later on still relies
        // on this buffer. hence we ran a reference counting pass before this, and
        // init the reference counter now accordingly:
        img->mem->ref = c->connected_mi;

        if(c->type == dt_token("source"))
        {
          assert(c->array_length <= 1);
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
          // TODO: better and more general caching:
          img->mem->ref++; // add one more so we can run the pipeline starting from after upload easily
          c->mem_staging->ref++; // ref staging memory so we don't overwrite it before the pipe starts (will be written in read_source() in the module)
        }
      }
    }
    else if(dt_connector_input(c))
    { // point our inputs to their counterparts:
      if(c->connected_mi >= 0)
      {
        // point to conn_image of connected output directly:
        node->conn_image[i] = graph->node[c->connected_mi].conn_image[c->connected_mc];
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
          c->mem_staging->ref++; // so we don't overwrite it before the pipe ends, because write_sink is called for all modules once at the end
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
  for(int i=0;i<node->num_connectors;i++) if(dt_connector_output(node->connector+i))
  { // allocate our output buffers
    dt_connector_t *c = node->connector+i;
    VkFormat format = dt_connector_vkformat(c);
    for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
    {
      dt_connector_image_t *img = dt_graph_connector_image(graph,
          node - graph->node, i, k, f);

      // only actually backed by memory if we have all these frames.
      // else we'll just point to the same image for all frames:
      vkBindImageMemory(qvk.device, img->image, graph->vkmem, img->offset);

      // put all newly created images into right layout
      VkCommandBuffer cmd_buf = graph->command_buffer;
      IMG_LAYOUT(img, UNDEFINED, SHADER_READ_ONLY_OPTIMAL);

      VkImageViewCreateInfo images_view_create_info = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = format,
        .image      = img->image,
        .subresourceRange = {
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel   = 0,
          .levelCount     = 1,
          .baseArrayLayer = 0,
          .layerCount     = 1
        },
      };
      if(img->image_view) vkDestroyImageView(qvk.device, img->image_view, VK_NULL_HANDLE);
      QVKR(vkCreateImageView(qvk.device, &images_view_create_info, NULL, &img->image_view));
    }
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
    QVKR(vkAllocateDescriptorSets(qvk.device, &dset_info_u, &node->uniform_dset));
    VkDescriptorBufferInfo uniform_info[] = {{
      .buffer      = graph->uniform_buffer,
      .offset      = 0,
      .range       = graph->uniform_global_size,
    },{
      .buffer      = graph->uniform_buffer,
      .offset      = node->module->uniform_size ? node->module->uniform_offset : 0,
      .range       = node->module->uniform_size ? node->module->uniform_size : graph->uniform_global_size,
    }};
    VkWriteDescriptorSet buf_dset[] = {{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = node->uniform_dset,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = uniform_info+0,
    },{
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = node->uniform_dset,
      .dstBinding      = 1,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = uniform_info+1,
    }};
    vkUpdateDescriptorSets(qvk.device, 2, buf_dset, 0, NULL);
  }

  VkDescriptorImageInfo img_info[DT_GRAPH_MAX_FRAMES*DT_MAX_CONNECTORS*25] = {{0}};
  VkWriteDescriptorSet  img_dset[DT_GRAPH_MAX_FRAMES*DT_MAX_CONNECTORS] = {{0}};
  int cur_dset = 0, node_dset = 0;
  uint32_t drawn_connector_cnt = 0;
  uint32_t drawn_connector[DT_MAX_CONNECTORS];
  for(int i=0;i<node->num_connectors;i++)
  {
    dt_connector_t *c = node->connector+i;
    if(dt_connector_output(c))
    { // allocate our output buffers
      for(int f=0;f<DT_GRAPH_MAX_FRAMES;f++)
      {
        int ii = cur_dset;
        for(int k=0;k<MAX(1,c->array_length);k++)
        {
          dt_connector_image_t *img = dt_graph_connector_image(graph,
              node - graph->node, i, k, MIN(f, c->frames-1));

          int iii = cur_dset++;
          assert(iii < sizeof(img_info)/sizeof(img_info[0]));
          img_info[iii].sampler     = VK_NULL_HANDLE;
          img_info[iii].imageView   = img->image_view;
          assert(img->image_view);
          img_info[iii].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        int dset = node_dset++;
        img_dset[dset].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        img_dset[dset].dstSet          = node->dset[f];
        img_dset[dset].dstBinding      = i;
        img_dset[dset].dstArrayElement = 0;
        img_dset[dset].descriptorCount = MAX(c->array_length, 1);
        img_dset[dset].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        img_dset[dset].pImageInfo      = img_info + ii;
      }

      if(c->type == dt_token("source"))
        vkBindBufferMemory(qvk.device, c->staging, graph->vkmem_staging, c->offset_staging);

      if(node->type == s_node_graphics)
        drawn_connector[drawn_connector_cnt++] = i;
    }
    else if(dt_connector_input(c))
    { // point our inputs to their counterparts:
      if(c->connected_mi >= 0)
      {
        for(int f=0;f<DT_GRAPH_MAX_FRAMES;f++)
        {
          int ii = cur_dset;
          for(int k=0;k<MAX(1,c->array_length);k++)
          {
            int frame = MIN(f, 
              graph->node[c->connected_mi].connector[c->connected_mc].frames-1);
            if(c->flags & s_conn_feedback)
            { // feedback connections cross the frame wires:
              frame = 1-f;
              // this should be ensured during connection:
              assert(c->frames == 2); 
              assert(graph->node[c->connected_mi].connector[c->connected_mc].frames == 2);
            }
            // the image struct is shared between in and out connectors, but we 
            // acces the frame either straight or crossed, depending on feedback mode.
            dt_connector_image_t *img  = dt_graph_connector_image(graph,
                node - graph->node, i, k, frame);
            int iii = cur_dset++;
            img_info[iii].sampler     = c->type == dt_token("sink") ? qvk.tex_sampler_nearest : qvk.tex_sampler;
            img_info[iii].imageView   = img->image_view;
            assert(img->image_view);
            img_info[iii].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          }
          int dset = node_dset++;
          img_dset[dset].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          img_dset[dset].dstSet          = node->dset[f];
          img_dset[dset].dstBinding      = i;
          img_dset[dset].dstArrayElement = 0;
          img_dset[dset].descriptorCount = MAX(c->array_length, 1);
          img_dset[dset].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
          img_dset[dset].pImageInfo      = img_info + ii;
        }
      }
      else
      { // else sorry not connected, buffer will not be bound.
        // unconnected inputs are a problem however:
        dt_log(s_log_err | s_log_pipe, "kernel %"PRItkn"_%"PRItkn"_%"PRItkn":%"PRItkn" is not connected!",
            dt_token_str(node->name), dt_token_str(node->module->inst),
            dt_token_str(node->kernel), dt_token_str(node->connector[i].name));
      }
      if(c->type == dt_token("sink"))
        vkBindBufferMemory(qvk.device, c->staging, graph->vkmem_staging, c->offset_staging);
    }
  }
  if(node->dset_layout)
    vkUpdateDescriptorSets(qvk.device, node_dset, img_dset, 0, NULL);

  if(drawn_connector_cnt)
  { // create framebuffer
    VkImageView attachment[DT_MAX_CONNECTORS];
    for(int i=0;i<drawn_connector_cnt;i++)
    { // make sure there is no array and make sure they are same res!
      assert(node->connector[drawn_connector[i]].array_length <= 1);
      assert(node->connector[drawn_connector[i]].roi.wd == 
             node->connector[drawn_connector[0]].roi.wd);
      assert(node->connector[drawn_connector[i]].roi.ht == 
             node->connector[drawn_connector[0]].roi.ht);

      dt_connector_image_t *img  = dt_graph_connector_image(graph,
          node - graph->node, drawn_connector[i], 0, 0);
      attachment[i] = img->image_view;
    }
    VkFramebufferCreateInfo fb_create_info = {
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = node->draw_render_pass,
      .attachmentCount = drawn_connector_cnt,
      .pAttachments    = attachment,
      .width           = node->connector[drawn_connector[0]].roi.wd,
      .height          = node->connector[drawn_connector[0]].roi.ht,
      .layers          = 1,
    };
    QVKR(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, &node->draw_framebuffer));
  }
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
    if(c->type == dt_token("read") && c->connected_mi >= 0 &&
     !(c->flags & s_conn_feedback))
    { // only free "read", not "sink" which we keep around for display
      // dt_log(s_log_pipe, "freeing input %"PRItkn"_%"PRItkn" %"PRItkn,
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(c->name));
      for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
        dt_vkfree(&graph->heap, 
            dt_graph_connector_image(graph, node-graph->node, i, k, f)->mem);
      // note that we keep the offset and VkImage etc around, we'll be using
      // these in consecutive runs through the pipeline and only clean up at
      // the very end. we just instruct our allocator that we're done with
      // this portion of the memory.
    }
    else if(dt_connector_output(c))
    {
      // dt_log(s_log_pipe, "freeing output ref count %"PRItkn"_%"PRItkn" %"PRItkn" %d %d",
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(c->name),
      //     c->connected_mi, c->mem->ref);
      for(int f=0;f<c->frames;f++) for(int k=0;k<MAX(1,c->array_length);k++)
        dt_vkfree(&graph->heap,
            dt_graph_connector_image(graph, node-graph->node, i, k, f)->mem);
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
}

// propagate full buffer size from source to sink
static void
modify_roi_out(dt_graph_t *graph, dt_module_t *module)
{
  int input = dt_module_get_connector(module, dt_token("input"));
  dt_connector_t *c = 0;
  module->img_param = (dt_image_params_t){{0}};
  if(input >= 0)
  { // first copy image metadata if we have a unique "input" connector
    c = module->connector + input;
    module->img_param = graph->module[c->connected_mi].img_param;
  }
  for(int i=0;i<module->num_connectors;i++)
  { // keep incoming roi in sync:
    dt_connector_t *c = module->connector+i;
    if(dt_connector_input(c) && c->connected_mi >= 0 && c->connected_mc >= 0)
    {
      dt_roi_t *roi = &graph->module[c->connected_mi].connector[c->connected_mc].roi;
      c->roi = *roi;
    }
  }
  if(module->so->modify_roi_out)
  {
    module->so->modify_roi_out(graph, module);
    // mark roi in of all outputs as uninitialised:
    for(int i=0;i<module->num_connectors;i++)
      if(dt_connector_output(module->connector+i))
        module->connector[i].roi.scale = -1.0f;
    return;
  }
  // default implementation:
  // mark roi in of all outputs as uninitialised:
  for(int i=0;i<module->num_connectors;i++)
    if(dt_connector_output(module->connector+i))
      module->connector[i].roi.scale = -1.0f;
  // copy over roi from connector named "input" to all outputs ("write")
  dt_roi_t roi = {0};
  if(input < 0)
  {
    // minimal default output size (for generator nodes with no callback)
    // might warn about this.
    roi.full_wd = 1024;
    roi.full_ht = 1024;
  }
  else
  {
    roi = graph->module[c->connected_mi].connector[c->connected_mc].roi;
    c->roi = roi; // also keep incoming roi in sync
  }
  for(int i=0;i<module->num_connectors;i++)
  {
    if(module->connector[i].type == dt_token("write"))
    {
      module->connector[i].roi.full_wd = roi.full_wd;
      module->connector[i].roi.full_ht = roi.full_ht;
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
      r->scale = 1.0f;
      // this is the performance/LOD switch for faster processing
      // on low end computers. needs to be wired somehow in gui/config.
      if(module->connector[0].type == dt_token("sink") &&
         module->inst == dt_token("main"))
      { // scale to fit into requested roi
        if(graph->output_wd > 0 || graph->output_ht > 0)
        {
          r->scale = MAX(
              r->full_wd / (float) graph->output_wd,
              r->full_ht / (float) graph->output_ht);
        }
      }
      r->wd = r->full_wd/r->scale;
      r->ht = r->full_ht/r->scale;
      r->x = r->y = 0;
    }
    if(output < 0)
    {
      dt_log(s_log_pipe|s_log_err, "input roi on %"PRItkn"_%"PRItkn" uninitialised!",
          dt_token_str(module->name), dt_token_str(module->inst));
      return;
    }
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
      if(c->connected_mi >= 0 && c->connected_mc >= 0)
      {
        dt_roi_t *roi = &graph->module[c->connected_mi].connector[c->connected_mc].roi;
        if(graph->module[c->connected_mi].connector[c->connected_mc].type == dt_token("source"))
        { // sources don't negotiate their size, they just give what they have
          roi->wd = roi->full_wd;
          roi->ht = roi->full_ht;
          c->roi = *roi; // TODO: this may mean that we need a resample node if the module can't handle it!
          // TODO: insert manually here
        }
        // if roi->scale > 0 it has been inited before and we're late to the party!
        // in this case, reverse the process:
        else if(roi->scale > 0.0f)
          c->roi = *roi; // TODO: this may mean that we need a resample node if the module can't handle it!
          // TODO: insert manually here
        else
          *roi = c->roi;
        // propagate flags:
        graph->module[c->connected_mi].connector[c->connected_mc].flags |= c->flags;
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
  VkCommandBuffer cmd_buf = graph->command_buffer;

  // runflag will be 1 if we ask to upload source explicitly (the first time around)
  if((*runflag == 0) && dt_node_source(node))
  {
    for(int i=0;i<node->num_connectors;i++)
    { // this is completely retarded and just to make the layout match what we expect below
      if(dt_connector_output(node->connector+i))
      {
        if(node->type == s_node_graphics)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, 0, graph->frame),
              SHADER_READ_ONLY_OPTIMAL,
              COLOR_ATTACHMENT_OPTIMAL);
        else for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->frame),
              SHADER_READ_ONLY_OPTIMAL,
              GENERAL);
      }
    }
    return VK_SUCCESS;
  }
  // TODO: extend the runflag to only switch on modules *after* cached input/changed parameters

  // sanity check: are all input connectors bound?
  for(int i=0;i<node->num_connectors;i++)
    if(dt_connector_input(node->connector+i))
      if(node->connector[i].connected_mi == -1)
      {
        dt_log(s_log_err, "input %"PRItkn":%"PRItkn" not connected!",
            dt_token_str(node->name), dt_token_str(node->connector[i].name));
        return VK_INCOMPLETE;
      }

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
    // IMG_LAYOUT(img, GENERAL, TRANSFER_SRC_OPTIMAL);
    IMG_LAYOUT(img, UNDEFINED, TRANSFER_SRC_OPTIMAL);
    BARRIER_IMG_LAYOUT(graph->thumbnail_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdCopyImage(cmd_buf,
        img->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        graph->thumbnail_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &cp);
    BARRIER_IMG_LAYOUT(graph->thumbnail_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return VK_SUCCESS;
  }

  // barriers and image layout transformations:
  // wait for our input images and transfer them to read only.
  // also wait for our output buffers to be transferred into general layout.
  // output is moved from UNDEFINED in the hope that the content can be discarded for
  // increased efficiency in this case.
  for(int i=0;i<node->num_connectors;i++)
  {
    if(dt_connector_input(node->connector+i))
    {
      // this needs to prepare the frame we're actually reading.
      // for feedback connections, this is crossed over.
      if(!node->module->so->write_sink)
        for(int k=0;k<MAX(1,node->connector[i].array_length);k++)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k,
                (node->connector[i].flags & s_conn_feedback) ?
                1-(graph->frame & 1) :
                graph->frame),
              GENERAL,
              SHADER_READ_ONLY_OPTIMAL);
      else
        IMG_LAYOUT(
            dt_graph_connector_image(graph, node-graph->node, i, 0, graph->frame),
            GENERAL,
            TRANSFER_SRC_OPTIMAL);
    }
    else if(dt_connector_output(node->connector+i))
    {
      // wait for layout transition on the output back to general:
      if(node->connector[i].flags & s_conn_clear)
      {
        IMG_LAYOUT(
            dt_graph_connector_image(graph, node-graph->node, i, 0, graph->frame),
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
            dt_graph_connector_image(graph, node-graph->node, i, 0, graph->frame)->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &col,
            1,
            &range);
        if(node->type == s_node_graphics)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, 0, graph->frame),
              TRANSFER_DST_OPTIMAL,
              COLOR_ATTACHMENT_OPTIMAL);
        else for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->frame),
              TRANSFER_DST_OPTIMAL,
              GENERAL);
      }
      else
      {
        if(node->type == s_node_graphics)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, 0, graph->frame),
              UNDEFINED,//SHADER_READ_ONLY_OPTIMAL,
              COLOR_ATTACHMENT_OPTIMAL);
        else for(int k=0;k<MAX(node->connector[i].array_length,1);k++)
          IMG_LAYOUT(
              dt_graph_connector_image(graph, node-graph->node, i, k, graph->frame),
              UNDEFINED,//SHADER_READ_ONLY_OPTIMAL,
              GENERAL);
      }
    }
  }

  const uint32_t wd = node->connector[0].roi.wd;
  const uint32_t ht = node->connector[0].roi.ht;
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
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->frame)->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        node->connector[0].staging,
        1, &regions);
    BARRIER_COMPUTE_BUFFER(node->connector[0].staging);
  }
  else if(dt_node_source(node))
  {
    // push profiler start
    if(graph->query_cnt < graph->query_max)
    {
      vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          graph->query_pool, graph->query_cnt);
      graph->query_name  [graph->query_cnt  ] = node->name;
      graph->query_kernel[graph->query_cnt++] = node->kernel;
    }
    IMG_LAYOUT(
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->frame),
        UNDEFINED,
        TRANSFER_DST_OPTIMAL);
    vkCmdCopyBufferToImage(
        cmd_buf,
        node->connector[0].staging,
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->frame)->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &regions);
    IMG_LAYOUT(
        dt_graph_connector_image(graph, node-graph->node, 0, 0, graph->frame),
        TRANSFER_DST_OPTIMAL,
        SHADER_READ_ONLY_OPTIMAL);
    // get a profiler timestamp:
    if(graph->query_cnt < graph->query_max)
    {
      vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          graph->query_pool, graph->query_cnt);
      graph->query_name  [graph->query_cnt  ] = node->name;
      graph->query_kernel[graph->query_cnt++] = node->kernel;
    }
  }

  // only non-sink and non-source nodes have a pipeline:
  if(!node->pipeline) return VK_SUCCESS;

  // push profiler start
  if(graph->query_cnt < graph->query_max)
  {
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        graph->query_pool, graph->query_cnt);
    graph->query_name  [graph->query_cnt  ] = node->name;
    graph->query_kernel[graph->query_cnt++] = node->kernel;
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

  // add our global uniforms:
  VkDescriptorSet desc_sets[] = {
    node->uniform_dset,
    node->dset[graph->frame % DT_GRAPH_MAX_FRAMES],
  };

  if(draw != -1)
  {
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, node->pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
      node->pipeline_layout, 0, LENGTH(desc_sets), desc_sets, 0, 0);
  }
  else
  {
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, node->pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
      node->pipeline_layout, 0, LENGTH(desc_sets), desc_sets, 0, 0);
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
    const float *p_draw = dt_module_param_float(node->module, pi);
    if(p_draw[0] > 0)
      vkCmdDraw(cmd_buf, (int)p_draw[0], 1, 0, 0);
    vkCmdEndRenderPass(cmd_buf);
  }

  // get a profiler timestamp:
  if(graph->query_cnt < graph->query_max)
  {
    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        graph->query_pool, graph->query_cnt);
    graph->query_name  [graph->query_cnt  ] = node->name;
    graph->query_kernel[graph->query_cnt++] = node->kernel;
  }
  return VK_SUCCESS;
}

static void
count_references(dt_graph_t *graph, dt_node_t *node)
{
  // dt_log(s_log_pipe, "counting references for %"PRItkn"_%"PRItkn,
  //     dt_token_str(node->name),
  //     dt_token_str(node->kernel));
  for(int c=0;c<node->num_connectors;c++)
  {
    if(dt_connector_input(node->connector+c))
    {
      // up reference counter of the connector that owns the output
      int mi = node->connector[c].connected_mi;
      int mc = node->connector[c].connected_mc;
      if(mi != -1) graph->node[mi].connector[mc].connected_mi++;
      // dt_log(s_log_pipe, "references %d on output %"PRItkn"_%"PRItkn":%"PRItkn,
      //     graph->node[mi].connector[mc].connected_mi,
      //     dt_token_str(graph->node[mi].name),
      //     dt_token_str(graph->node[mi].kernel),
      //     dt_token_str(graph->node[mi].connector[mc].name));
    }
    else
    {
      // output cannot know all modules connected to it, so
      // it stores the reference counter instead.
      // we increase it here because every node needs its own outputs for processing.
      node->connector[c].connected_mi++;
      // dt_log(s_log_pipe, "references %d on output %"PRItkn"_%"PRItkn":%"PRItkn,
      //     node->connector[c].connected_mi,
      //     dt_token_str(node->name),
      //     dt_token_str(node->kernel),
      //     dt_token_str(node->connector[c].name));
    }
  }
}

// default callback for create nodes: pretty much copy the module.
// does no vulkan work, just graph connections. shall not fail.
static void
create_nodes(dt_graph_t *graph, dt_module_t *module, uint64_t *uniform_offset)
{
  const uint64_t u_offset = *uniform_offset;
  uint64_t u_size = module->committed_param_size ?
    module->committed_param_size :
    module->param_size;
  u_size = (u_size + qvk.uniform_alignment-1) & -qvk.uniform_alignment;
  *uniform_offset += u_size;
  const int nodes_begin = graph->num_nodes;
  // TODO: if roi size/scale does not match, insert resample node!
  // TODO: where? inside create_nodes? or we fix it afterwards?
  if(module->so->create_nodes)
  {
    module->so->create_nodes(graph, module);
  }
  else
  {
    assert(graph->num_nodes < graph->max_nodes);
    const int nodeid = graph->num_nodes++;
    dt_node_t *node = graph->node + nodeid;

    *node = (dt_node_t) {
      .name           = module->name,
      .kernel         = dt_token("main"), // file name
      .num_connectors = module->num_connectors,
      .module         = module,
      // make sure we don't follow garbage pointers. pure sink or source nodes
      // don't run a pipeline. and hence have no descriptor sets to construct. they
      // do, however, have input or output images allocated for them.
      .pipeline       = 0,
      .dset_layout    = 0,
    };

    // compute shader or graphics pipeline?
    char filename[1024] = {0};
    snprintf(filename, sizeof(filename), "modules/%"PRItkn"/main.vert.spv",
        dt_token_str(module->name));
    struct stat statbuf;
    if(stat(filename, &statbuf)) node->type = s_node_compute;
    else node->type = s_node_graphics;

    // determine kernel dimensions:
    int output = dt_module_get_connector(module, dt_token("output"));
    // output == -1 means we must be a sink, anyways there won't be any more glsl
    // to be run here!
    if(output >= 0)
    {
      dt_roi_t *roi = &module->connector[output].roi;
      node->wd = roi->wd;
      node->ht = roi->ht;
      node->dp = 1;
    }

    for(int i=0;i<module->num_connectors;i++)
      dt_connector_copy(graph, module, i, nodeid, i);
  }

  module->uniform_offset = u_offset;
  module->uniform_size   = u_size;
  // now init frame count, repoint connection image storage
  for(int n=nodes_begin;n<graph->num_nodes;n++)
  {
    for(int i=0;i<graph->node[n].num_connectors;i++)
    {
      // TODO: also try to do this kind of =1 dance with array_length?
      if(graph->node[n].connector[i].frames == 0)
      {
        if(graph->node[n].connector[i].flags & s_conn_feedback)
          graph->node[n].connector[i].frames = 2;
        else
          graph->node[n].connector[i].frames = 1;
      }
      if(dt_connector_output(graph->node[n].connector+i))
      { // only allocate memory for output connectors
        graph->node[n].conn_image[i] = graph->conn_image_end;
        graph->conn_image_end += MAX(1,graph->node[n].connector[i].array_length)
          * graph->node[n].connector[i].frames;
        assert(graph->conn_image_end <= graph->conn_image_max);
      }
      else graph->node[n].conn_image[i] = -1;
    }
  }
}

VkResult dt_graph_run(
    dt_graph_t     *graph,
    dt_graph_run_t  run)
{
  clock_t clock_beg = clock();

  // only waiting for the gui thread to draw our output, and only
  // if we intend to clean it up behind their back
  if(graph->gui_attached &&
    (run & (s_graph_run_alloc | s_graph_run_create_nodes)))
    QVKR(vkDeviceWaitIdle(qvk.device));

  if(run & s_graph_run_alloc)
  {
    if(!graph->uniform_dset_layout)
    { // init layout of uniform descriptor set:
      VkDescriptorSetLayoutBinding bindings[] = {{
        .binding = 0, // global uniform, frame number etc
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
      },{
        .binding = 1, // module local uniform, params struct
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
      }};
      VkDescriptorSetLayoutCreateInfo dset_layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings,
      };
      QVKR(vkCreateDescriptorSetLayout(qvk.device, &dset_layout_info, 0, &graph->uniform_dset_layout));
    }
  }
  graph->query_cnt = 0;

{ // module scope
  // find list of modules in post order
  uint32_t modid[100];
  assert(sizeof(modid)/sizeof(modid[0]) >= graph->num_modules);

  int cnt = 0;
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
#define TRAVERSE_POST \
  modid[cnt++] = curr;
#include "graph-traverse.inc"

  // ==============================================
  // first pass: find output rois
  // ==============================================
  // execute after all inputs have been traversed:
  // "int curr" will be the current node
  // walk all inputs and determine roi on all outputs
  if(run & s_graph_run_roi)
    for(int i=0;i<cnt;i++)
      modify_roi_out(graph, graph->module + modid[i]);

  // if the roi out is 0, probably reading some sources went wrong and we need
  // to abort right here!
  if(graph->module[graph->num_modules-1].connector[0].roi.full_wd == 0)
  {
    dt_log(s_log_err, "roi of last module %"PRItkn" did not get initialised!",
        dt_token_str(graph->module[graph->num_modules-1].name));
    return VK_INCOMPLETE;
  }

  // now we don't always want the full size buffer but are interested in a
  // scaled or cropped sub-region. actually this step is performed
  // transparently in the sink module's modify_roi_in first thing in the
  // second pass.

  // ==============================================
  // 2nd pass: request input rois
  // and create nodes for all modules
  // ==============================================
  if(run & (s_graph_run_roi | s_graph_run_create_nodes))
  {
    // delete all previous nodes
    for(int i=0;i<graph->conn_image_end;i++)
    {
      if(graph->conn_image_pool[i].image)      vkDestroyImage(qvk.device,     graph->conn_image_pool[i].image, VK_NULL_HANDLE);
      if(graph->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, graph->conn_image_pool[i].image_view, VK_NULL_HANDLE);
    }
    graph->num_nodes = 0;
    graph->conn_image_end = 0;
    for(int i=0;i<graph->num_nodes;i++)
    {
      for(int j=0;j<graph->node[i].num_connectors;j++)
      {
        dt_connector_t *c = graph->node[i].connector+j;
        if(c->staging) vkDestroyBuffer(qvk.device, c->staging, VK_NULL_HANDLE);
      }
      vkDestroyPipelineLayout     (qvk.device, graph->node[i].pipeline_layout,  0);
      vkDestroyPipeline           (qvk.device, graph->node[i].pipeline,         0);
      vkDestroyDescriptorSetLayout(qvk.device, graph->node[i].dset_layout,      0);
      vkDestroyFramebuffer        (qvk.device, graph->node[i].draw_framebuffer, 0);
      vkDestroyRenderPass         (qvk.device, graph->node[i].draw_render_pass, 0);
    }
    graph->uniform_global_size = qvk.uniform_alignment; // global data, aligned
    uint64_t uniform_offset = graph->uniform_global_size;
    for(int i=cnt-1;i>=0;i--)
      modify_roi_in(graph, graph->module+modid[i]);
    for(int i=0;i<cnt;i++)
      create_nodes(graph, graph->module+modid[i], &uniform_offset);
    // make sure connectors are zero inited:
    memset(graph->conn_image_pool, 0, sizeof(dt_connector_image_t)*graph->conn_image_end);
    graph->uniform_size = uniform_offset;

    // these feedback connectors really cause a lot of trouble.
    // we need to initialise the input connectors now after full graph traversal.
    // or else the connection performed during node creation will copy some node id
    // from the connected module that has not yet been initialised.
    // we work around this by storing the *module* id and connector in
    // dt_connector_copy() for feedback connectors, and repoint it here.
    // i suppose we could always do that, not only for feedback connectors, to
    // simplify control logic.
    for(int ni=0;ni<graph->num_nodes;ni++)
    {
      dt_node_t *n = graph->node + ni;
      for(int i=0;i<n->num_connectors;i++)
      {
        if(n->connector[i].associated_i >= 0) // needs repointing
        {
          int n0, c0;
          int mi0 = n->connector[i].associated_i;
          int mc0 = n->connector[i].associated_c;
          if(dt_connector_input(n->connector+i))
          { // walk node->module->module->node
            int mi1 = graph->module[mi0].connector[mc0].connected_mi;
            int mc1 = graph->module[mi0].connector[mc0].connected_mc;
            n0 = graph->module[mi1].connector[mc1].associated_i;
            c0 = graph->module[mi1].connector[mc1].associated_c;
          }
          else // if(dt_connector_output(n->connector+i))
          { // walk node->module->node
            n0 = graph->module[mi0].connector[mc0].associated_i;
            c0 = graph->module[mi0].connector[mc0].associated_c;
          }
          n->connector[i].connected_mi = n0;
          n->connector[i].connected_mc = c0;
        }
      }
    }
  }
} // end scope, done with modules

  // if no more action than generating the output roi was requested, exit now:
  if(run < s_graph_run_create_nodes<<1) return VK_SUCCESS;

{ // node scope
  int cnt = 0;
  dt_node_t *const arr = graph->node;
  const int arr_cnt = graph->num_nodes;
  uint32_t nodeid[2000];
  assert(sizeof(nodeid)/sizeof(nodeid[0]) >= graph->num_nodes);
#define TRAVERSE_POST \
  nodeid[cnt++] = curr;
#include "graph-traverse.inc"

  // ==============================================
  // 1st pass alloc and free
  // ==============================================
  if(run & s_graph_run_alloc)
  {
    // nuke reference counters:
    for(int n=0;n<graph->num_nodes;n++)
      for(int c=0;c<graph->node[n].num_connectors;c++)
        if(dt_connector_output(graph->node[n].connector+c))
          graph->node[n].connector[c].connected_mi = 0;
    // perform reference counting on the final connected node graph.
    // this is needed for memory allocation later:
    for(int i=0;i<cnt;i++)
      count_references(graph, graph->node+nodeid[i]);
    // free pipeline resources if previously allocated anything:
    dt_vkalloc_nuke(&graph->heap);
    dt_vkalloc_nuke(&graph->heap_staging);
    graph->dset_cnt_image_read = 0;
    graph->dset_cnt_image_write = 0;
    graph->dset_cnt_buffer = 0;
    graph->dset_cnt_uniform = 1; // we have one global uniform for roi + params
    graph->memory_type_bits = ~0u;
    graph->memory_type_bits_staging = ~0u;
    for(int i=0;i<cnt;i++)
    {
      QVKR(alloc_outputs(graph, graph->node+nodeid[i]));
      free_inputs       (graph, graph->node+nodeid[i]);
    }
  }

  if(graph->heap.vmsize > graph->vkmem_size)
  {
    run |= s_graph_run_upload_source; // new mem means new source
    if(graph->vkmem)
    {
      QVKR(vkDeviceWaitIdle(qvk.device));
      vkFreeMemory(qvk.device, graph->vkmem, 0);
    }
    // image data to pass between nodes
    VkMemoryAllocateInfo mem_alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->heap.vmsize,
      .memoryTypeIndex = qvk_get_memory_type(graph->memory_type_bits,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info, 0, &graph->vkmem));
    graph->vkmem_size = graph->heap.vmsize;
  }

  if(graph->heap_staging.vmsize > graph->vkmem_staging_size)
  {
    run |= s_graph_run_upload_source; // new mem means new source
    if(graph->vkmem_staging)
    {
      QVKR(vkDeviceWaitIdle(qvk.device));
      vkFreeMemory(qvk.device, graph->vkmem_staging, 0);
    }
    // staging memory to copy to and from device
    VkMemoryAllocateInfo mem_alloc_info_staging = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = graph->heap_staging.vmsize,
      .memoryTypeIndex = qvk_get_memory_type(graph->memory_type_bits_staging,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
          VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info_staging, 0, &graph->vkmem_staging));
    graph->vkmem_staging_size = graph->heap_staging.vmsize;
  }

  if(graph->vkmem_uniform_size < graph->uniform_size)
  {
    if(graph->vkmem_uniform)
    {
      QVKR(vkDeviceWaitIdle(qvk.device));
      vkFreeMemory(qvk.device, graph->vkmem_uniform, 0);
    }
    // uniform data to pass parameters
    VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = graph->uniform_size,
      .usage = // VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    QVKR(vkCreateBuffer(qvk.device, &buffer_info, 0, &graph->uniform_buffer));
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(qvk.device, graph->uniform_buffer, &mem_req);
    VkMemoryAllocateInfo mem_alloc_info_uniform = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = mem_req.size,//graph->uniform_size,
      .memoryTypeIndex = qvk_get_memory_type(mem_req.memoryTypeBits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
          VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    };
    QVKR(vkAllocateMemory(qvk.device, &mem_alloc_info_uniform, 0, &graph->vkmem_uniform));
    graph->vkmem_uniform_size = graph->uniform_size;
    vkBindBufferMemory(qvk.device, graph->uniform_buffer, graph->vkmem_uniform, 0);
  }

  if(run & s_graph_run_alloc)
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
        .descriptorCount = 1+DT_GRAPH_MAX_FRAMES*(graph->num_nodes+graph->dset_cnt_uniform),
      }};

      VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = LENGTH(pool_sizes),
        .pPoolSizes    = pool_sizes,
        .maxSets       = DT_GRAPH_MAX_FRAMES*(
            graph->dset_cnt_image_read + graph->dset_cnt_image_write
          + graph->num_nodes
          + graph->dset_cnt_buffer     + graph->dset_cnt_uniform),
      };
      if(graph->dset_pool)
        vkDestroyDescriptorPool(qvk.device, graph->dset_pool, VK_NULL_HANDLE);
      QVKR(vkCreateDescriptorPool(qvk.device, &pool_info, 0, &graph->dset_pool));
      graph->dset_cnt_image_read_alloc = graph->dset_cnt_image_read;
      graph->dset_cnt_image_write_alloc = graph->dset_cnt_image_write;
      graph->dset_cnt_buffer_alloc = graph->dset_cnt_buffer;
      graph->dset_cnt_uniform_alloc = graph->dset_cnt_uniform;
    }
  }

  // not really needed, vkBeginCommandBuffer will reset our cmd buf
  // vkResetCommandPool(qvk.device, graph->command_pool, 0);

  // begin command buffer
  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT would allow simultaneous execution while still pending.
    // not sure about our images, i suppose they will need sync/double buffering in this case
  };
  QVKR(vkBeginCommandBuffer(graph->command_buffer, &begin_info));
  vkCmdResetQueryPool(graph->command_buffer, graph->query_pool, 0, graph->query_max);

  // upload all source data to staging memory
  threads_mutex_t *mutex = 0;// graph->io_mutex; // no speed impact, maybe not needed
  if(mutex) threads_mutex_lock(mutex);
  if(run & s_graph_run_upload_source)
  {
    uint8_t *mapped = 0;
    QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE, 0, (void**)&mapped));
    for(int n=0;n<graph->num_nodes;n++)
    { // for all source nodes:
      dt_node_t *node = graph->node + n;
      if(dt_node_source(node))
      {
        if(node->module->so->read_source) // TODO: detect error code!
          node->module->so->read_source(node->module,
              mapped + node->connector[0].offset_staging);
        else
          dt_log(s_log_err|s_log_pipe, "source node '%"PRItkn"' has no read_source() callback!",
              dt_token_str(node->name));
      }
    }
    vkUnmapMemory(qvk.device, graph->vkmem_staging);
  }
  if(mutex) threads_mutex_unlock(mutex);

  // ==============================================
  // 2nd pass finish alloc and record commmand buf
  // ==============================================
  if(run & s_graph_run_alloc)
    for(int i=0;i<cnt;i++)
      QVKR(alloc_outputs2(graph, graph->node+nodeid[i]));

  if(run & s_graph_run_alloc)
    for(int i=0;i<cnt;i++)
      QVKR(alloc_outputs3(graph, graph->node+nodeid[i]));
  int runflag = (run & s_graph_run_upload_source) ? 1 : 0;
  if(run & s_graph_run_record_cmd_buf)
    for(int i=0;i<cnt;i++)
      QVKR(record_command_buffer(graph, graph->node+nodeid[i], &runflag));
} // end scope, done with nodes

  if(run & s_graph_run_alloc)
  {
    dt_log(s_log_mem, "images : peak rss %g MB vmsize %g MB",
        graph->heap.peak_rss/(1024.0*1024.0),
        graph->heap.vmsize  /(1024.0*1024.0));

    dt_log(s_log_mem, "staging: peak rss %g MB vmsize %g MB",
        graph->heap_staging.peak_rss/(1024.0*1024.0),
        graph->heap_staging.vmsize  /(1024.0*1024.0));
  }

  QVKR(vkEndCommandBuffer(graph->command_buffer));

  // now upload uniform data before submitting command buffer
{ // module traversal
  dt_module_t *const arr = graph->module;
  const int arr_cnt = graph->num_modules;
  uint8_t *uniform_mem = 0;
  QVKR(vkMapMemory(qvk.device, graph->vkmem_uniform, 0,
        graph->uniform_size, 0, (void**)&uniform_mem));
  ((uint32_t *)uniform_mem)[0] = graph->frame;
#define TRAVERSE_POST \
  dt_module_t *mod = arr+curr;\
  if(mod->so->commit_params)\
    mod->so->commit_params(graph, mod);\
  if(mod->committed_param_size)\
    memcpy(uniform_mem + mod->uniform_offset, mod->committed_param, mod->committed_param_size);\
  else if(mod->param_size)\
    memcpy(uniform_mem + mod->uniform_offset, mod->param, mod->param_size);
#include "graph-traverse.inc"
  vkUnmapMemory(qvk.device, graph->vkmem_uniform);
}

  clock_t clock_end = clock();
  dt_log(s_log_perf, "record cmd buf:\t%8.3f ms", 1000.0*(clock_end - clock_beg)/(double)CLOCKS_PER_SEC);

  VkSubmitInfo submit = {
    .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers    = &graph->command_buffer,
  };

  if(run & s_graph_run_record_cmd_buf)
  {
    if(graph->queue == qvk.queue_graphics)
      threads_mutex_lock(&qvk.queue_mutex);
    vkResetFences(qvk.device, 1, &graph->command_fence);
    QVKR(vkQueueSubmit(graph->queue, 1, &submit, graph->command_fence));
    if(run & s_graph_run_wait_done) // timeout in nanoseconds, 30 is about 1s
      QVKR(vkWaitForFences(qvk.device, 1, &graph->command_fence, VK_TRUE, 1ul<<40));
    if(graph->queue == qvk.queue_graphics)
      threads_mutex_unlock(&qvk.queue_mutex);
  }
  
  if(run & s_graph_run_download_sink)
  {
    for(int n=0;n<graph->num_nodes;n++)
    { // for all sink nodes:
      dt_node_t *node = graph->node + n;
      if(dt_node_sink(node))
      {
        if(node->module->so->write_sink)
        {
          uint8_t *mapped = 0;
          QVKR(vkMapMemory(qvk.device, graph->vkmem_staging, 0, VK_WHOLE_SIZE,
                0, (void**)&mapped));
          node->module->so->write_sink(node->module,
              mapped + node->connector[0].offset_staging);
          vkUnmapMemory(qvk.device, graph->vkmem_staging);
        }
      }
    }
  }

  QVKR(vkGetQueryPoolResults(qvk.device, graph->query_pool,
        0, graph->query_cnt,
        sizeof(graph->query_pool_results[0]) * graph->query_max,
        graph->query_pool_results,
        sizeof(graph->query_pool_results[0]),
        VK_QUERY_RESULT_64_BIT));

  uint64_t accum_time = 0;
  for(int i=0;i<graph->query_cnt;i+=2)
  {
    dt_log(s_log_perf, "%"PRItkn"_%"PRItkn":\t%8.3f ms",
        dt_token_str(graph->query_name  [i]),
        dt_token_str(graph->query_kernel[i]),
        (graph->query_pool_results[i+1]-
        graph->query_pool_results[i])* 1e-6 * qvk.ticks_to_nanoseconds);
    if(graph->query_name[i] == dt_token("shared") || graph->query_name[i] == dt_token("burst"))
      accum_time += graph->query_pool_results[i+1]- graph->query_pool_results[i];
  }
  dt_log(s_log_perf, "total burst:\t%8.3f ms",
      accum_time * 1e-6 * qvk.ticks_to_nanoseconds);
  if(graph->query_cnt)
    dt_log(s_log_perf, "total time:\t%8.3f ms",
        (graph->query_pool_results[graph->query_cnt-1]-graph->query_pool_results[0])*1e-6 * qvk.ticks_to_nanoseconds);
  // reset run flags:
  graph->runflags = 0;
  return VK_SUCCESS;
}

dt_node_t *
dt_graph_get_display(
    dt_graph_t *g,
    dt_token_t  which)
{
  // TODO: cache somewhere on graph?
  for(int n=0;n<g->num_nodes;n++)
  {
    if((g->node[n].module->name == dt_token("display") ||
        g->node[n].module->name == dt_token("thumb")) &&
        g->node[n].module->inst == which)
      return g->node+n;
  }
  return 0;
}

dt_connector_image_t*
dt_graph_connector_image(
    dt_graph_t *graph,
    int         nid,    // node id
    int         cid,    // connector id
    int         array,  // array index
    int         frame)  // frame number
{
  if(graph->node[nid].conn_image[cid] == -1)
    dt_log(s_log_err, "requesting unconnected image buffer!");
  int nid2 = nid, cid2 = cid;
  if(dt_connector_input(graph->node[nid].connector+cid))
  {
    cid2 = graph->node[nid].connector[cid].connected_mc;
    nid2 = graph->node[nid].connector[cid].connected_mi;
  }
  frame %= graph->node[nid2].connector[cid2].frames;
  return graph->conn_image_pool +
    graph->node[nid].conn_image[cid] + MAX(1,graph->node[nid].connector[cid].array_length) * frame + array;
}

void dt_graph_reset(dt_graph_t *g)
{
  g->gui_attached = 0;
  g->active_module = 0;
  g->lod_scale = 0;
  g->runflags = 0;
  g->frame = 0;
  g->output_wd = 0;
  g->output_ht = 0;
  g->thumbnail_image = 0;
  g->query_cnt = 0;
  g->params_end = 0;
  g->history_end = 0;
  for(int i=0;i<g->num_modules;i++)
    if(g->module[i].so->cleanup)
      g->module[i].so->cleanup(g->module+i);
  for(int i=0;i<g->conn_image_end;i++)
  {
    if(g->conn_image_pool[i].image)      vkDestroyImage(qvk.device,     g->conn_image_pool[i].image, VK_NULL_HANDLE);
    if(g->conn_image_pool[i].image_view) vkDestroyImageView(qvk.device, g->conn_image_pool[i].image_view, VK_NULL_HANDLE);
    g->conn_image_pool[i].image = 0;
    g->conn_image_pool[i].image_view = 0;
  }
  for(int i=0;i<g->num_nodes;i++)
  {
    for(int j=0;j<g->node[i].num_connectors;j++)
    {
      dt_connector_t *c = g->node[i].connector+j;
      if(c->staging) vkDestroyBuffer(qvk.device, c->staging, VK_NULL_HANDLE);
    }
    vkDestroyPipelineLayout     (qvk.device, g->node[i].pipeline_layout,  0);
    vkDestroyPipeline           (qvk.device, g->node[i].pipeline,         0);
    vkDestroyDescriptorSetLayout(qvk.device, g->node[i].dset_layout,      0);
    vkDestroyFramebuffer        (qvk.device, g->node[i].draw_framebuffer, 0);
    vkDestroyRenderPass         (qvk.device, g->node[i].draw_render_pass, 0);
  }
  g->conn_image_end = 0;
  g->num_nodes = 0;
  g->num_modules = 0;
}
