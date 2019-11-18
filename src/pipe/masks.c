#include "masks.h"
#include "global.h"
#include "modules/api.h"
#include "core/log.h"
#include "qvk/qvk.h"

VkResult
dt_masks_create_pipeline(
    dt_graph_t *graph,   // associated graph
    dt_node_t  *node,    // node to create draw call pipeline for
    const int   ci)      // connector index with output framebuffer
{
  // TODO: cache shader modules on global struct? (we'll reuse the draw kernels for all strokes)
  VkShaderModule shader_module_vert, shader_module_geom, shader_module_frag;
  QVKR(dt_graph_create_shader_module(node->name, dt_token("dvert"), &shader_module_vert));
  QVKR(dt_graph_create_shader_module(node->name, dt_token("dgeom"), &shader_module_geom));
  QVKR(dt_graph_create_shader_module(node->name, dt_token("dfrag"), &shader_module_frag));

  // vertex shader, geometry shader, fragment shader
  VkPipelineShaderStageCreateInfo shader_info[] = {
    {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_VERTEX_BIT,
      .module = shader_module_vert,
      .pName  = "main",
    }, {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_GEOMETRY_BIT,
      .module = shader_module_geom,
      .pName  = "main",
    }, {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = shader_module_frag,
      .pName  = "main",
    },
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount   = 0,
    .pVertexBindingDescriptions      = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions    = NULL,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology               = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
    .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport = {
    .x        = 0.0f,
    .y        = 0.0f,
    .width    = (float) node->connector[ci].roi.wd,
    .height   = (float) node->connector[ci].roi.ht,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
    .offset = { 0, 0 },
    .extent = {
      .width  = node->connector[ci].roi.wd,
      .height = node->connector[ci].roi.ht,
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
    .cullMode                = VK_CULL_MODE_BACK_BIT,
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
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp        = VK_BLEND_OP_MAX,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
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
    .stageCount = LENGTH(shader_info),
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
  // ATTACH_LABEL_VARIABLE(node->pipeline, PIPELINE);

  // TODO: can we bind this as a framebuffer and as an output image at the same time?
  // TODO: if not we'll need to alter the descriptor set above.
  VkFramebufferCreateInfo fb_create_info = {
    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass      = node->draw_render_pass,
    .attachmentCount = 1,
    .pAttachments    = &node->connector[ci].array[0].image_view,
    .width           =  node->connector[ci].roi.wd,
    .height          =  node->connector[ci].roi.ht,
    .layers          = 1,
  };

  QVKR(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, &node->connector[ci].framebuffer));
  // ATTACH_LABEL_VARIABLE(draw_framebuffer, FRAMEBUFFER);

  // TODO: keep cached for others
  vkDestroyShaderModule(qvk.device, shader_module_vert, 0);
  vkDestroyShaderModule(qvk.device, shader_module_geom, 0);
  vkDestroyShaderModule(qvk.device, shader_module_frag, 0);

  return VK_SUCCESS;
}


void
dt_masks_record_draw(
    dt_graph_t *graph,
    dt_node_t  *node,   // node with draw kernels
    const int   ci)     // output connector index
{
  VkClearValue clear_color = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } };
  
  VkRenderPassBeginInfo render_pass_info = {
    .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass        = node->draw_render_pass,
    .framebuffer       = node->connector[ci].framebuffer,
    .renderArea.offset = { 0, 0 },
    .renderArea.extent = { .width = node->connector[ci].roi.wd, .height = node->connector[ci].roi.ht },
    .clearValueCount   = 1,
    .pClearValues      = &clear_color
  };

  VkDescriptorSet desc_sets[] = {
    graph->uniform_dset,
    node->dset,
  };

  // this is already uploaded in the uniform buffer, but we need the count
  // which is stored in the first element.
  const int pi = dt_module_get_param(node->module->so, dt_token("draw"));
  const float *p_draw = dt_module_param_float(node->module, pi);

  VkCommandBuffer cmd_buf = graph->command_buffer;

  vkCmdBeginRenderPass(cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindDescriptorSets(cmd_buf,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      node->pipeline_layout,
      0, LENGTH(desc_sets), desc_sets, 0, 0);
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, node->pipeline);
  // vert count, instance count, first vert, first instance
  vkCmdDraw(cmd_buf, p_draw[0], 1, 0, 0);
  vkCmdEndRenderPass(cmd_buf);
}

void
dt_masks_cleanup(
    dt_graph_t *graph,
    dt_node_t  *node,
    const int   ci)
{
  // pipeline and images will be dealt with as usual
  vkDestroyFramebuffer(qvk.device, node->connector[ci].framebuffer, 0);
  vkDestroyRenderPass(qvk.device, node->draw_render_pass, 0);
}
