/*
 * Nuklear - 1.32.0 - public domain
 * no warrenty implied; use at your own risk.
 * authored from 2015-2016 by Micha Mettke
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_GLFW_VULKAN_H_
#define NK_GLFW_VULKAN_H_

#ifdef NK_GLFW_VULKAN_IMPLEMENTATION
#include "shd.h"
#include "qvk/qvk.h"
#include "gui/gui.h"
#include "core/lut.h"
#include "gui/font_metrics.h"
#include "pipe/res.h"
#endif

#include <assert.h>
#include <stddef.h>
#include <string.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

NK_API void
nk_glfw3_init(struct nk_context *ctx, VkRenderPass render_pass, GLFWwindow *win, VkDevice logical_device,
              VkPhysicalDevice physical_device,
              VkDeviceSize max_vertex_buffer, VkDeviceSize max_element_buffer);
NK_API void nk_glfw3_shutdown(void);
NK_API struct nk_user_font *nk_glfw3_font(int which);
NK_API int  nk_glfw3_font_load(const char *fontfile, int fontsize,
    VkCommandBuffer cmd, VkQueue graphics_queue);
NK_API void nk_glfw3_font_cleanup();
NK_API void nk_glfw3_input_begin(struct nk_context *ctx, GLFWwindow *win, const int enable_grab);
NK_API void nk_glfw3_input_end  (struct nk_context *ctx, GLFWwindow *win, const int enable_grab);
NK_API void nk_glfw3_create_cmd(
    struct nk_context *ctx,
    GLFWwindow *win,
    struct nk_buffer *cmd_extra,
    VkCommandBuffer cmd,
    enum nk_anti_aliasing AA,
    int frame, int max_frames);
NK_API void nk_glfw3_resize(GLFWwindow *win,
    uint32_t framebuffer_width,
    uint32_t framebuffer_height);
NK_API void nk_glfw3_keyboard_callback(GLFWwindow *w, int key, int scancode, int action, int mods);
NK_API void nk_glfw3_char_callback(GLFWwindow *win, unsigned int codepoint);
NK_API void nk_glfw3_scroll_callback(GLFWwindow *win, double xoff, double yoff);
NK_API void nk_glfw3_mouse_button_callback(GLFWwindow *win, int button, int action, int mods);
NK_API void nk_glfw3_mouse_position_callback(GLFWwindow *win, double x, double y);

NK_API void nk_glfw3_setup_display_colour_management(float g0[3], float M0[9], float g1[3], float M1[9], int xpos1, int bitdepth);
NK_API void nk_glfw3_win1_open(struct nk_context *ctx, VkRenderPass render_pass, GLFWwindow *win, VkDeviceSize max_vertex_buffer, VkDeviceSize max_element_buffer);
NK_API void nk_glfw3_win1_close();
#endif
/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_GLFW_VULKAN_IMPLEMENTATION
#undef NK_GLFW_VULKAN_IMPLEMENTATION
#include <stdlib.h>

#ifndef NK_GLFW_DOUBLE_CLICK_LO
#define NK_GLFW_DOUBLE_CLICK_LO 0.02
#endif
#ifndef NK_GLFW_DOUBLE_CLICK_HI
#define NK_GLFW_DOUBLE_CLICK_HI 0.25
#endif
#ifndef NK_GLFW_MAX_TEXTURES
#define NK_GLFW_MAX_TEXTURES 256
#endif

#define VK_COLOR_COMPONENT_MASK_RGBA                                           \
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                      \
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

struct nk_glfw_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

struct nk_glfw_device
{
  struct nk_buffer cmds;
  int max_vertex_buffer;
  int max_element_buffer;

  VkRenderPass render_pass;
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_memory;
  void *mapped_vertex;
  VkBuffer index_buffer;
  VkDeviceMemory index_memory;
  void *mapped_index;
  VkBuffer uniform_buffer;
  VkDeviceMemory uniform_memory;
  void *mapped_uniform;

  VkDescriptorPool descriptor_pool;
  VkDescriptorSetLayout uniform_descriptor_set_layout;
  VkDescriptorSet uniform_descriptor_set;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  int push_constant_size;
  int push_constant[128];
};

struct nk_glfw_win
{
  GLFWwindow *win;
  int width, height;
  double last_button_click;
  struct nk_context *ctx;
};

static struct nk_glfw
{
  struct nk_glfw_win    w0, w1;  // user input/window stuff
  struct nk_glfw_device d0, d1;  // each has their own render pass and such

  // common vulkan resources
  VkDevice logical_device;
  VkPhysicalDevice physical_device;
  VkSampler sampler;
  struct nk_draw_null_texture tex_null;
  struct nk_user_font font[3];
  dt_font_t dtfont;
  VkImage font_image;
  VkImageView font_image_view;
  VkDeviceMemory font_memory;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSetLayout font_dset_layout;
  VkDescriptorSet font_dset;
} glfw;

struct Mat4f {
    float m[16];
};

NK_API void nk_glfw3_setup_display_colour_management(
    float g0[3], float M0[9], float g1[3], float M1[9], int xpos1, int bitdepth)
{
  int xpos = 0;
  float maxval = powf(2.0f, bitdepth);
  for(int i=0;i<2;i++)
  {
    struct nk_glfw_device *dev = &glfw.d0;
    if(i) dev = &glfw.d1;
    dev->push_constant_size = (2*(3+9)+3)*sizeof(float);
    memcpy(dev->push_constant,     g0, 3*sizeof(float));
    memcpy(dev->push_constant +3,  g1, 3*sizeof(float));
    memcpy(dev->push_constant +6,  M0, 9*sizeof(float));
    memcpy(dev->push_constant+15,  M1, 9*sizeof(float));
    memcpy(dev->push_constant+24, &maxval, sizeof(maxval));
    memcpy(dev->push_constant+25, &xpos, sizeof(xpos));
    memcpy(dev->push_constant+26, &xpos1, sizeof(xpos1));
    dev->push_constant_size += sizeof(float); // keep one for font strength
  }
}

NK_INTERN uint32_t nk_glfw3_find_memory_index(
    VkPhysicalDevice physical_device, uint32_t type_filter,
    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    uint32_t i;

    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    for (i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    assert(0);
    return 0;
}

NK_INTERN VkResult
nk_glfw3_create_sampler()
{
  VkSamplerCreateInfo sampler_info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = NULL,
    .maxAnisotropy = 1.0,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = .0f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.0f,
    .maxLod = 10.0f,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
  };
  return vkCreateSampler(glfw.logical_device, &sampler_info, NULL, &glfw.sampler);
}

NK_INTERN VkResult
nk_glfw3_create_buffer_and_memory(
    struct nk_glfw_device *dev,
    VkBuffer *buffer,
    VkBufferUsageFlags usage,
    VkDeviceMemory *memory,
    VkDeviceSize size)
{
    VkMemoryRequirements mem_reqs;
    VkBufferCreateInfo buffer_info;
    VkMemoryAllocateInfo alloc_info;
    VkResult result;

    memset(&buffer_info, 0, sizeof(VkBufferCreateInfo));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(glfw.logical_device, &buffer_info, NULL, buffer);
    if(result != VK_SUCCESS) return result;

    vkGetBufferMemoryRequirements(glfw.logical_device, *buffer, &mem_reqs);

    memset(&alloc_info, 0, sizeof(VkMemoryAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        glfw.physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(glfw.logical_device, &alloc_info, NULL, memory);
    if(result != VK_SUCCESS) return result;
    result = vkBindBufferMemory(glfw.logical_device, *buffer, *memory, 0);
    return result;
}

NK_INTERN VkResult nk_glfw3_create_font_descriptor_pool()
{
  VkDescriptorPoolSize pool_sizes = {
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
  };
  VkDescriptorPoolCreateInfo pool_info = {
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = 1,
    .pPoolSizes = &pool_sizes,
    .maxSets = 1,
  };
  return vkCreateDescriptorPool(
      glfw.logical_device, &pool_info, NULL,
      &glfw.descriptor_pool);
}

NK_INTERN VkResult nk_glfw3_create_descriptor_pool(struct nk_glfw_device *dev)
{
  VkDescriptorPoolSize pool_sizes = {
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
  };
  VkDescriptorPoolCreateInfo pool_info = {
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = 1,
    .pPoolSizes = &pool_sizes,
    .maxSets = 1,
  };
  return vkCreateDescriptorPool(
      glfw.logical_device, &pool_info, NULL,
      &dev->descriptor_pool);
}

NK_INTERN VkResult
nk_glfw3_create_uniform_descriptor_set_layout(struct nk_glfw_device *dev)
{
  VkDescriptorSetLayoutBinding binding = {
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };
  VkDescriptorSetLayoutCreateInfo descriptor_set_info = {
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings = &binding,
  };
  return vkCreateDescriptorSetLayout(glfw.logical_device, &descriptor_set_info,
        NULL, &dev->uniform_descriptor_set_layout);
}

NK_INTERN VkResult
nk_glfw3_create_and_update_uniform_descriptor_set(struct nk_glfw_device *dev)
{
  VkDescriptorSetAllocateInfo allocate_info;
  VkDescriptorBufferInfo buffer_info;
  VkWriteDescriptorSet descriptor_write;
  VkResult result;

  memset(&allocate_info, 0, sizeof(VkDescriptorSetAllocateInfo));
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = dev->descriptor_pool;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &dev->uniform_descriptor_set_layout;

  result = vkAllocateDescriptorSets(glfw.logical_device, &allocate_info,
      &dev->uniform_descriptor_set);
  if(result != VK_SUCCESS) return result;

  memset(&buffer_info, 0, sizeof(VkDescriptorBufferInfo));
  buffer_info.buffer = dev->uniform_buffer;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(struct Mat4f);

  memset(&descriptor_write, 0, sizeof(VkWriteDescriptorSet));
  descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet = dev->uniform_descriptor_set;
  descriptor_write.dstBinding = 0;
  descriptor_write.dstArrayElement = 0;
  descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptor_write.descriptorCount = 1;
  descriptor_write.pBufferInfo = &buffer_info;

  vkUpdateDescriptorSets(glfw.logical_device, 1, &descriptor_write, 0, NULL);
  return VK_SUCCESS;
}

NK_INTERN VkResult
nk_glfw3_create_font_dset()
{
  VkDescriptorSetLayoutBinding binding = {
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_ALL,
  };
  VkDescriptorSetLayoutCreateInfo descriptor_set_info = {
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings = &binding,
  };
  VkResult result = vkCreateDescriptorSetLayout(
      glfw.logical_device, &descriptor_set_info,
      NULL, &glfw.font_dset_layout);
  if(result != VK_SUCCESS) return result;
  VkDescriptorSetAllocateInfo allocate_info = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool     = glfw.descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &glfw.font_dset_layout,
  };
  return vkAllocateDescriptorSets(glfw.logical_device, &allocate_info, &glfw.font_dset);
}

NK_INTERN VkResult
nk_glfw3_create_pipeline_layout(struct nk_glfw_device *dev)
{
  VkDescriptorSetLayout descriptor_set_layouts[2] = {
    dev->uniform_descriptor_set_layout, glfw.font_dset_layout };
  VkPushConstantRange pcrange = {
    .stageFlags = VK_SHADER_STAGE_ALL,
    .offset     = 0,
    .size       = dev->push_constant_size,
  };
  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 2,
    .pSetLayouts = descriptor_set_layouts,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pcrange,
  };

  return vkCreatePipelineLayout(glfw.logical_device, &pipeline_layout_info,
      NULL, &dev->pipeline_layout);
}

NK_INTERN VkPipelineShaderStageCreateInfo
nk_glfw3_create_shader(struct nk_glfw_device *dev, unsigned char *spv_shader,
                       uint32_t size, VkShaderStageFlagBits stage_bit)
{
    VkShaderModuleCreateInfo create_info;
    VkPipelineShaderStageCreateInfo shader_info = {0};
    VkShaderModule module = NULL;
    VkResult result;

    memset(&create_info, 0, sizeof(VkShaderModuleCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (const uint32_t *)spv_shader;
    result =
        vkCreateShaderModule(glfw.logical_device, &create_info, NULL, &module);
    if(result != VK_SUCCESS) return shader_info;

    memset(&shader_info, 0, sizeof(VkPipelineShaderStageCreateInfo));
    shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_info.stage = stage_bit;
    shader_info.module = module;
    shader_info.pName = "main";
    return shader_info;
}

NK_INTERN VkResult
nk_glfw3_create_pipeline(struct nk_glfw_device *dev)
{
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    VkPipelineRasterizationStateCreateInfo rasterization_state;
    VkPipelineColorBlendAttachmentState attachment_state = {
        VK_TRUE,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_MASK_RGBA,
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineMultisampleStateCreateInfo multisample_state;
    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                        VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkPipelineShaderStageCreateInfo shader_stages[2];
    VkVertexInputBindingDescription vertex_input_info;
    VkVertexInputAttributeDescription vertex_attribute_description[3];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkResult result;

    memset(&input_assembly_state, 0,
           sizeof(VkPipelineInputAssemblyStateCreateInfo));
    input_assembly_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    memset(&rasterization_state, 0,
           sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterization_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode = VK_CULL_MODE_NONE;
    rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.lineWidth = 1.0f;

    memset(&color_blend_state, 0, sizeof(VkPipelineColorBlendStateCreateInfo));
    color_blend_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments = &attachment_state;

    memset(&viewport_state, 0, sizeof(VkPipelineViewportStateCreateInfo));
    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    memset(&multisample_state, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisample_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    memset(&dynamic_state, 0, sizeof(VkPipelineDynamicStateCreateInfo));
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pDynamicStates = dynamic_states;
    dynamic_state.dynamicStateCount = 2;

    shader_stages[0] = nk_glfw3_create_shader(
        dev, gui_shd_gui_vert_spv,
        gui_shd_gui_vert_spv_len, VK_SHADER_STAGE_VERTEX_BIT);
    shader_stages[1] = nk_glfw3_create_shader(
        dev, gui_shd_gui_frag_spv,
        gui_shd_gui_frag_spv_len, VK_SHADER_STAGE_FRAGMENT_BIT);

    memset(&vertex_input_info, 0, sizeof(VkVertexInputBindingDescription));
    vertex_input_info.binding = 0;
    vertex_input_info.stride = sizeof(struct nk_glfw_vertex);
    vertex_input_info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    memset(&vertex_attribute_description, 0,
           sizeof(VkVertexInputAttributeDescription) * 3);
    vertex_attribute_description[0].location = 0;
    vertex_attribute_description[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attribute_description[0].offset =
        NK_OFFSETOF(struct nk_glfw_vertex, position);
    vertex_attribute_description[1].location = 1;
    vertex_attribute_description[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attribute_description[1].offset =
        NK_OFFSETOF(struct nk_glfw_vertex, uv);
    vertex_attribute_description[2].location = 2;
    vertex_attribute_description[2].format = VK_FORMAT_R8G8B8A8_UINT;
    vertex_attribute_description[2].offset =
        NK_OFFSETOF(struct nk_glfw_vertex, col);

    memset(&vertex_input, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_input_info;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = vertex_attribute_description;

    VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input,
      .pInputAssemblyState = &input_assembly_state,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterization_state,
      .pMultisampleState = &multisample_state,
      .pColorBlendState = &color_blend_state,
      .pDynamicState = &dynamic_state,
      .layout = dev->pipeline_layout,
      .renderPass = dev->render_pass,
      .basePipelineIndex = -1,
      .basePipelineHandle = NULL,
    };

    result = vkCreateGraphicsPipelines(glfw.logical_device, NULL, 1,
                                       &pipeline_info, NULL, &dev->pipeline);

    vkDestroyShaderModule(glfw.logical_device, shader_stages[0].module, NULL);
    vkDestroyShaderModule(glfw.logical_device, shader_stages[1].module, NULL);
    return result;
}

NK_INTERN VkResult
nk_glfw3_create_render_resources(
    struct nk_glfw_device *dev)
{
  VkResult result;
  result = nk_glfw3_create_descriptor_pool(dev);
  if(result != VK_SUCCESS) return result;
  result = nk_glfw3_create_uniform_descriptor_set_layout(dev);
  if(result != VK_SUCCESS) return result;
  result = nk_glfw3_create_and_update_uniform_descriptor_set(dev);
  if(result != VK_SUCCESS) return result;
  result = nk_glfw3_create_pipeline_layout(dev);
  if(result != VK_SUCCESS) return result;
  result = nk_glfw3_create_pipeline(dev);
  return result;
}

NK_API VkResult nk_glfw3_device_create(
    struct nk_glfw_device *dev,
    VkRenderPass render_pass,
    VkDeviceSize max_vertex_buffer, VkDeviceSize max_element_buffer)
{
  dev->max_vertex_buffer  = max_vertex_buffer;
  dev->max_element_buffer = max_element_buffer;
  dev->push_constant_size = (2*(3+9)+3+1)*sizeof(float);
  nk_buffer_init_default(&dev->cmds);
  dev->render_pass = render_pass;

  nk_glfw3_create_buffer_and_memory(
      dev, &dev->vertex_buffer,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      &dev->vertex_memory, max_vertex_buffer);
  nk_glfw3_create_buffer_and_memory(
      dev, &dev->index_buffer,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      &dev->index_memory, max_element_buffer);
  nk_glfw3_create_buffer_and_memory(
      dev, &dev->uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      &dev->uniform_memory, sizeof(struct Mat4f));

  vkMapMemory(glfw.logical_device, dev->vertex_memory, 0, max_vertex_buffer, 0, &dev->mapped_vertex);
  vkMapMemory(glfw.logical_device, dev->index_memory, 0, max_element_buffer, 0, &dev->mapped_index);
  vkMapMemory(glfw.logical_device, dev->uniform_memory, 0, sizeof(struct Mat4f), 0, &dev->mapped_uniform);

  return nk_glfw3_create_render_resources(dev);
}

NK_INTERN uint64_t
nk_glfw3_device_upload_atlas(
    VkCommandBuffer cmd,
    VkQueue graphics_queue,
    const void *image, int width,
    int height)
{
    VkImageCreateInfo image_info;
    VkResult result;
    VkMemoryRequirements mem_reqs;
    VkMemoryAllocateInfo alloc_info;
    VkBufferCreateInfo buffer_info;
    uint8_t *data = 0;
    VkCommandBufferBeginInfo begin_info;
    VkImageMemoryBarrier image_memory_barrier;
    VkBufferImageCopy buffer_copy_region;
    VkFence fence;
    VkFenceCreateInfo fence_create;
    VkSubmitInfo submit_info;
    VkImageViewCreateInfo image_view_info;
    struct {
        VkDeviceMemory memory;
        VkBuffer buffer;
    } staging_buffer;

    memset(&image_info, 0, sizeof(VkImageCreateInfo));
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    // image_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = (uint32_t)width;
    image_info.extent.height = (uint32_t)height;
    image_info.extent.depth = 1;
    const int mip_levels = 4;
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(glfw.logical_device, &image_info, NULL, &glfw.font_image);
    if(result != VK_SUCCESS) return 0;

    vkGetImageMemoryRequirements(
        glfw.logical_device, glfw.font_image, &mem_reqs);

    memset(&alloc_info, 0, sizeof(VkMemoryAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        glfw.physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(glfw.logical_device, &alloc_info, NULL,
                              &glfw.font_memory);
    if(result != VK_SUCCESS) return 0;
    result = vkBindImageMemory(glfw.logical_device, glfw.font_image,
                               glfw.font_memory, 0);
    if(result != VK_SUCCESS) return 0;

    memset(&buffer_info, 0, sizeof(VkBufferCreateInfo));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = alloc_info.allocationSize;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(glfw.logical_device, &buffer_info, NULL,
                            &staging_buffer.buffer);
    if(result != VK_SUCCESS) return 0;
    vkGetBufferMemoryRequirements(glfw.logical_device, staging_buffer.buffer,
                                  &mem_reqs);

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        glfw.physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(glfw.logical_device, &alloc_info, NULL,
                              &staging_buffer.memory);
    if(result != VK_SUCCESS) return 0;
    result = vkBindBufferMemory(glfw.logical_device, staging_buffer.buffer,
                                staging_buffer.memory, 0);
    if(result != VK_SUCCESS) return 0;

    result = vkMapMemory(glfw.logical_device, staging_buffer.memory, 0, alloc_info.allocationSize, 0, (void **)&data);
    if(result != VK_SUCCESS) return 0;
    if(image)
    { // if something went wrong, render uninitialised memory instead.
      // memcpy(data, image, width * height * 4 * sizeof(float));
      for(int k=0;k<width*height;k++)
      {
        ((uint8_t*)data)[4*k+0] = ((uint8_t*)image)[3*k+0];
        ((uint8_t*)data)[4*k+1] = ((uint8_t*)image)[3*k+1];
        ((uint8_t*)data)[4*k+2] = ((uint8_t*)image)[3*k+2];
        ((uint8_t*)data)[4*k+3] = 255;
      }
    }
    vkUnmapMemory(glfw.logical_device, staging_buffer.memory);

    memset(&begin_info, 0, sizeof(VkCommandBufferBeginInfo));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    /*
    use the same command buffer as for render as we are regenerating the
    buffer during render anyway
    */
    result = vkBeginCommandBuffer(cmd, &begin_info);
    if(result != VK_SUCCESS) return 0;

    memset(&image_memory_barrier, 0, sizeof(VkImageMemoryBarrier));
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.image = glfw.font_image;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.levelCount = mip_levels;
    image_memory_barrier.subresourceRange.layerCount = 1;
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &image_memory_barrier);

    memset(&buffer_copy_region, 0, sizeof(VkBufferImageCopy));
    buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_copy_region.imageSubresource.layerCount = 1;
    buffer_copy_region.imageExtent.width = (uint32_t)width;
    buffer_copy_region.imageExtent.height = (uint32_t)height;
    buffer_copy_region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(
        cmd, staging_buffer.buffer, glfw.font_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_copy_region);

  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .image = glfw.font_image,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseMipLevel = 0,
  };

  for (uint32_t i = 1; i < mip_levels; i++)
  {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, NULL,
        0, NULL,
        1, &barrier);
    VkImageBlit blit = {
      .srcOffsets = {{ 0, 0, 0 }, { width, height, 1 }},
      .srcSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = i - 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
      .dstOffsets = {{ 0, 0, 0 }, { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 }},
      .dstSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = i,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    vkCmdBlitImage(cmd,
        glfw.font_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        glfw.font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_LINEAR);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, NULL,
        0, NULL,
        1, &barrier);
    if(width  > 1) width  /= 2;
    if(height > 1) height /= 2;
  }
  barrier.subresourceRange.baseMipLevel = mip_levels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
      0, NULL,
      0, NULL,
      1, &barrier);

    result = vkEndCommandBuffer(cmd);
    if(result != VK_SUCCESS) return 0;

    memset(&fence_create, 0, sizeof(VkFenceCreateInfo));
    fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    result = vkCreateFence(glfw.logical_device, &fence_create, NULL, &fence);
    if(result != VK_SUCCESS) return 0;

    memset(&submit_info, 0, sizeof(VkSubmitInfo));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    result = vkQueueSubmit(graphics_queue, 1, &submit_info, fence);
    if(result != VK_SUCCESS) return 0;
    result =
        vkWaitForFences(glfw.logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if(result != VK_SUCCESS) return 0;

    vkDestroyFence(glfw.logical_device, fence, NULL);

    vkDestroyBuffer(glfw.logical_device, staging_buffer.buffer, NULL);
    vkFreeMemory(glfw.logical_device, staging_buffer.memory, NULL);

    memset(&image_view_info, 0, sizeof(VkImageViewCreateInfo));
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = glfw.font_image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = image_info.format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.layerCount = 1;
    image_view_info.subresourceRange.levelCount = mip_levels;

    result = vkCreateImageView(glfw.logical_device, &image_view_info, NULL,
                               &glfw.font_image_view);
    if(result != VK_SUCCESS) return 0;

    VkDescriptorImageInfo descriptor_image_info = {
      .sampler     = glfw.sampler,
      .imageView   = glfw.font_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet descriptor_write = {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = glfw.font_dset,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo      = &descriptor_image_info,
    };
    vkUpdateDescriptorSets(glfw.logical_device, 1, &descriptor_write, 0, NULL);
    return (uint64_t)glfw.font_dset;
}

NK_INTERN void nk_glfw3_destroy_render_resources(struct nk_glfw_device *dev)
{
  vkDestroyPipeline(glfw.logical_device, dev->pipeline, NULL);
  vkDestroyPipelineLayout(glfw.logical_device, dev->pipeline_layout, NULL);
  vkDestroyDescriptorSetLayout(glfw.logical_device, dev->uniform_descriptor_set_layout, NULL);
  vkDestroyDescriptorPool(glfw.logical_device, dev->descriptor_pool, NULL);
}

NK_API void
nk_glfw3_resize(
    GLFWwindow *w,
    uint32_t width,
    uint32_t height)
{
  if(w == glfw.w1.win) { glfw.w1.width = width; glfw.w1.height = height; }
  else                 { glfw.w0.width = width; glfw.w0.height = height; }
}

NK_API void nk_glfw3_device_destroy(struct nk_glfw_device *dev)
{
  nk_glfw3_destroy_render_resources(dev);

  vkUnmapMemory(glfw.logical_device, dev->vertex_memory);
  vkUnmapMemory(glfw.logical_device, dev->index_memory);
  vkUnmapMemory(glfw.logical_device, dev->uniform_memory);

  vkDestroyBuffer(glfw.logical_device, dev->vertex_buffer, NULL);
  vkDestroyBuffer(glfw.logical_device, dev->index_buffer, NULL);
  vkDestroyBuffer(glfw.logical_device, dev->uniform_buffer, NULL);

  vkFreeMemory(glfw.logical_device, dev->vertex_memory, NULL);
  vkFreeMemory(glfw.logical_device, dev->index_memory, NULL);
  vkFreeMemory(glfw.logical_device, dev->uniform_memory, NULL);

  nk_buffer_free(&dev->cmds);
}

NK_API
void nk_glfw3_shutdown(void)
{
  vkDeviceWaitIdle(glfw.logical_device);
  nk_glfw3_font_cleanup();
  vkDestroyDescriptorSetLayout(glfw.logical_device, glfw.font_dset_layout, NULL);
  vkDestroyDescriptorPool(glfw.logical_device, glfw.descriptor_pool, NULL);
  vkDestroySampler(glfw.logical_device, glfw.sampler, NULL);
  nk_glfw3_device_destroy(&glfw.d0);
  if(glfw.w1.win) nk_glfw3_device_destroy(&glfw.d1);

  memset(&glfw, 0, sizeof(glfw));
}

NK_API void nk_glfw3_font_cleanup()
{
  vkDestroyImage(glfw.logical_device, glfw.font_image, 0);
  vkDestroyImageView(glfw.logical_device, glfw.font_image_view, 0);
  vkFreeMemory(glfw.logical_device, glfw.font_memory, 0);
  dt_font_cleanup(&glfw.dtfont);
}

float dt_font_text_width(nk_handle handle, float height, const char *text, int text_len)
{
  int glyph_len = 1, pos = 0;
  float text_width = 0;
  while (glyph_len && pos < text_len)
  {
    nk_rune codepoint;
    glyph_len = nk_utf_decode(text + pos, &codepoint, text_len - pos);
    if(!glyph_len) break;
    // TODO query kern table if appropriate
    int idx = dt_font_glyph(&glfw.dtfont, codepoint);
    const dt_font_glyph_t *g = glfw.dtfont.glyph + idx;
    const float sx = g->height_to_em*height;
    text_width += sx * g->advance;
    pos += glyph_len;
  }
  return text_width;
}

void dt_font_query_glyph(
    nk_handle handle,
    float font_height,
    struct nk_user_font_glyph *glyph,
    nk_rune codepoint,
    nk_rune next_codepoint)
{
  int idx = dt_font_glyph(&glfw.dtfont, codepoint);
  // numbers come to us in EM units, font_height in nk is line height (asc+desc)
  const dt_font_glyph_t *g = glfw.dtfont.glyph + idx;
  const float sx = g->height_to_em*font_height;
  const float sy = g->height_to_em*font_height;
  glyph->offset.x = sx*g->pbox_x;
  glyph->offset.y = sy + sy*g->pbox_y;
  glyph->width    = sx*g->pbox_w;
  glyph->height   = sy*g->pbox_h;
  glyph->xadvance = sx*g->advance;
  // these are atlas texture uv coordinates
  glyph->uv[0].x = g->tbox_x;
  glyph->uv[0].y = g->tbox_y;
  glyph->uv[1].x = g->tbox_x + g->tbox_w;
  glyph->uv[1].y = g->tbox_y + g->tbox_h;
}

NK_API struct nk_user_font *nk_glfw3_font(
    int which)
{
  return glfw.font+which;
}

NK_API int nk_glfw3_font_load(
    const char *fontfile,
    int fontsize,
    VkCommandBuffer cmd,
    VkQueue graphics_queue)
{
  int err = 0;
  uint64_t texid = 0;
  dt_lut_header_t header;
  char tmp[PATH_MAX] = {0};
  snprintf(tmp, sizeof(tmp), "data/%s_msdf.lut", fontfile);
  FILE *f = dt_graph_open_resource(0, 0, tmp, "rb");
  if(!f)
  {
    fprintf(stderr, "[nk] could not find msdf font lut `%s'!\n", fontfile);
    err = 1;
  }
  else
  {
    if(fread(&header, sizeof(dt_lut_header_t), 1, f) != 1 ||
        header.version < dt_lut_header_version ||
        header.channels != 3 || // 4 ||
        header.datatype != dt_lut_header_ui8) // f32)
    {
      fprintf(stderr, "[nk] could not load msdf font lut from `%s'!\n", tmp);
      fclose(f);
      err = 2;
    }
    else
    {
      // void *image = malloc(sizeof(float)*4*header.wd*header.ht);
      // fread(image, sizeof(float), 4*header.wd*header.ht, f);
      void *image = malloc(sizeof(uint8_t)*3*header.wd*header.ht);
      fread(image, sizeof(uint8_t), 3*header.wd*header.ht, f);
      fclose(f);
      texid = nk_glfw3_device_upload_atlas(cmd, graphics_queue, image, header.wd, header.ht);
      free(image);
    }
  }
  if(err)
    texid = nk_glfw3_device_upload_atlas(cmd, graphics_queue, 0, 32, 32);
  glfw.font[0].userdata.ptr = &glfw;
  glfw.font[0].height = fontsize;
  glfw.font[0].width = &dt_font_text_width;
  glfw.font[0].query = &dt_font_query_glyph;
  glfw.font[0].texture.ptr = (void*)texid;
  glfw.font[1] = glfw.font[0];
  glfw.font[2] = glfw.font[0];
  glfw.font[1].height = floorf(1.5*fontsize);
  glfw.font[2].height = 2*fontsize;

  snprintf(tmp, sizeof(tmp), "data/%s_metrics.lut", fontfile);
  f = dt_graph_open_resource(0, 0, tmp, "rb");
  if(!f)
  {
    fprintf(stderr, "[nk] could not find font metrics lut `%s'!\n", tmp);
    err = 3;
  }
  dt_font_read(&glfw.dtfont, f);
  glfw.dtfont.default_glyph = dt_font_glyph(&glfw.dtfont, '*');
  if(f) fclose(f);
  return err;
}

NK_API void nk_glfw3_input_begin(struct nk_context *ctx, GLFWwindow *w, const int enable_grab)
{
  nk_input_begin(ctx);
  if(enable_grab)
  { /* optional grabbing behavior */
    if (ctx->input.mouse.grab)
      glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else if (ctx->input.mouse.ungrab)
      glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

NK_API void nk_glfw3_input_end(struct nk_context *ctx, GLFWwindow *w, const int enable_grab)
{
  nk_input_end(ctx);
  if(enable_grab)
  {
    if (ctx->input.mouse.grabbed)
    { // wayland does not support setting cursor position, but CURSOR_DISABLED handles it transparently. so we leave this in for xorg:
      float xscale, yscale;
      dt_gui_content_scale(w, &xscale, &yscale);
      glfwSetCursorPos(w, ctx->input.mouse.prev.x/xscale, ctx->input.mouse.prev.y/yscale);
      ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
      ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }
  }
}

// this is shit but removing it would require api changes to nuklear (i.e. expose the inner of the loop here as an NK_API function)
static inline nk_flags
nk_convert_extra(struct nk_context *ctx, struct nk_buffer *buf, struct nk_buffer *cmds,
    struct nk_buffer *vertices, struct nk_buffer *elements,
    const struct nk_convert_config *config)
{
    nk_flags res = NK_CONVERT_SUCCESS;
    NK_ASSERT(ctx);
    NK_ASSERT(cmds);
    NK_ASSERT(vertices);
    NK_ASSERT(elements);
    NK_ASSERT(config);
    NK_ASSERT(config->vertex_layout);
    NK_ASSERT(config->vertex_size);
    if (!ctx || !cmds || !vertices || !elements || !config || !config->vertex_layout)
        return NK_CONVERT_INVALID_PARAM;

    nk_byte *buffer = buf->memory.ptr;
    if (0 >= buf->allocated) return 0;
    const struct nk_command *cmd = nk_ptr_add_const(struct nk_command, buffer, 0);

    // everything is already setup, we run this after converting the rest of nuklear
    while(cmd)
    {
#ifdef NK_INCLUDE_COMMAND_USERDATA
        ctx->draw_list.userdata = cmd->userdata;
#endif
        switch (cmd->type) {
        case NK_COMMAND_NOP: break;
        case NK_COMMAND_SCISSOR: {
            const struct nk_command_scissor *s = (const struct nk_command_scissor*)cmd;
            nk_draw_list_add_clip(&ctx->draw_list, nk_rect(s->x, s->y, s->w, s->h));
        } break;
        case NK_COMMAND_LINE: {
            const struct nk_command_line *l = (const struct nk_command_line*)cmd;
            nk_draw_list_stroke_line(&ctx->draw_list, nk_vec2(l->begin.x, l->begin.y),
                nk_vec2(l->end.x, l->end.y), l->color, l->line_thickness);
        } break;
        case NK_COMMAND_CURVE: {
            const struct nk_command_curve *q = (const struct nk_command_curve*)cmd;
            nk_draw_list_stroke_curve(&ctx->draw_list, nk_vec2(q->begin.x, q->begin.y),
                nk_vec2(q->ctrl[0].x, q->ctrl[0].y), nk_vec2(q->ctrl[1].x,
                q->ctrl[1].y), nk_vec2(q->end.x, q->end.y), q->color,
                config->curve_segment_count, q->line_thickness);
        } break;
        case NK_COMMAND_RECT: {
            const struct nk_command_rect *r = (const struct nk_command_rect*)cmd;
            nk_draw_list_stroke_rect(&ctx->draw_list, nk_rect(r->x, r->y, r->w, r->h),
                r->color, (float)r->rounding, r->line_thickness);
        } break;
        case NK_COMMAND_RECT_FILLED: {
            const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled*)cmd;
            nk_draw_list_fill_rect(&ctx->draw_list, nk_rect(r->x, r->y, r->w, r->h),
                r->color, (float)r->rounding);
        } break;
        case NK_COMMAND_RECT_MULTI_COLOR: {
            const struct nk_command_rect_multi_color *r = (const struct nk_command_rect_multi_color*)cmd;
            nk_draw_list_fill_rect_multi_color(&ctx->draw_list, nk_rect(r->x, r->y, r->w, r->h),
                r->left, r->top, r->right, r->bottom);
        } break;
        case NK_COMMAND_CIRCLE: {
            const struct nk_command_circle *c = (const struct nk_command_circle*)cmd;
            nk_draw_list_stroke_circle(&ctx->draw_list, nk_vec2((float)c->x + (float)c->w/2,
                (float)c->y + (float)c->h/2), (float)c->w/2, c->color,
                config->circle_segment_count, c->line_thickness);
        } break;
        case NK_COMMAND_CIRCLE_FILLED: {
            const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
            nk_draw_list_fill_circle(&ctx->draw_list, nk_vec2((float)c->x + (float)c->w/2,
                (float)c->y + (float)c->h/2), (float)c->w/2, c->color,
                config->circle_segment_count);
        } break;
        case NK_COMMAND_ARC: {
            const struct nk_command_arc *c = (const struct nk_command_arc*)cmd;
            nk_draw_list_path_line_to(&ctx->draw_list, nk_vec2(c->cx, c->cy));
            nk_draw_list_path_arc_to(&ctx->draw_list, nk_vec2(c->cx, c->cy), c->r,
                c->a[0], c->a[1], config->arc_segment_count);
            nk_draw_list_path_stroke(&ctx->draw_list, c->color, NK_STROKE_CLOSED, c->line_thickness);
        } break;
        case NK_COMMAND_ARC_FILLED: {
            const struct nk_command_arc_filled *c = (const struct nk_command_arc_filled*)cmd;
            nk_draw_list_path_line_to(&ctx->draw_list, nk_vec2(c->cx, c->cy));
            nk_draw_list_path_arc_to(&ctx->draw_list, nk_vec2(c->cx, c->cy), c->r,
                c->a[0], c->a[1], config->arc_segment_count);
            nk_draw_list_path_fill(&ctx->draw_list, c->color);
        } break;
        case NK_COMMAND_TRIANGLE: {
            const struct nk_command_triangle *t = (const struct nk_command_triangle*)cmd;
            nk_draw_list_stroke_triangle(&ctx->draw_list, nk_vec2(t->a.x, t->a.y),
                nk_vec2(t->b.x, t->b.y), nk_vec2(t->c.x, t->c.y), t->color,
                t->line_thickness);
        } break;
        case NK_COMMAND_TRIANGLE_FILLED: {
            const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled*)cmd;
            nk_draw_list_fill_triangle(&ctx->draw_list, nk_vec2(t->a.x, t->a.y),
                nk_vec2(t->b.x, t->b.y), nk_vec2(t->c.x, t->c.y), t->color);
        } break;
        case NK_COMMAND_POLYGON: {
            int i;
            const struct nk_command_polygon*p = (const struct nk_command_polygon*)cmd;
            for (i = 0; i < p->point_count; ++i) {
                struct nk_vec2 pnt = nk_vec2((float)p->points[i].x, (float)p->points[i].y);
                nk_draw_list_path_line_to(&ctx->draw_list, pnt);
            }
            nk_draw_list_path_stroke(&ctx->draw_list, p->color, NK_STROKE_CLOSED, p->line_thickness);
        } break;
        case NK_COMMAND_POLYGON_FILLED: {
            int i;
            const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled*)cmd;
            for (i = 0; i < p->point_count; ++i) {
                struct nk_vec2 pnt = nk_vec2((float)p->points[i].x, (float)p->points[i].y);
                nk_draw_list_path_line_to(&ctx->draw_list, pnt);
            }
            nk_draw_list_path_fill(&ctx->draw_list, p->color);
        } break;
        case NK_COMMAND_POLYLINE: {
            int i;
            const struct nk_command_polyline *p = (const struct nk_command_polyline*)cmd;
            for (i = 0; i < p->point_count; ++i) {
                struct nk_vec2 pnt = nk_vec2((float)p->points[i].x, (float)p->points[i].y);
                nk_draw_list_path_line_to(&ctx->draw_list, pnt);
            }
            nk_draw_list_path_stroke(&ctx->draw_list, p->color, NK_STROKE_OPEN, p->line_thickness);
        } break;
        case NK_COMMAND_TEXT: {
            const struct nk_command_text *t = (const struct nk_command_text*)cmd;
            nk_draw_list_add_text(&ctx->draw_list, t->font, nk_rect(t->x, t->y, t->w, t->h),
                t->string, t->length, t->height, t->foreground);
        } break;
        case NK_COMMAND_IMAGE: {
            const struct nk_command_image *i = (const struct nk_command_image*)cmd;
            nk_draw_list_add_image(&ctx->draw_list, i->img, nk_rect(i->x, i->y, i->w, i->h), i->col);
        } break;
        case NK_COMMAND_CUSTOM: {
            const struct nk_command_custom *c = (const struct nk_command_custom*)cmd;
            c->callback(&ctx->draw_list, c->x, c->y, c->w, c->h, c->callback_data);
        } break;
        default: break;
        }
        if (cmd->next >= buf->allocated) return 0;
        cmd = nk_ptr_add_const(struct nk_command, buffer, cmd->next);
    }
    res |= (cmds->needed > cmds->allocated + (cmds->memory.size - cmds->size)) ? NK_CONVERT_COMMAND_BUFFER_FULL: 0;
    res |= (vertices->needed > vertices->allocated) ? NK_CONVERT_VERTEX_BUFFER_FULL: 0;
    res |= (elements->needed > elements->allocated) ? NK_CONVERT_ELEMENT_BUFFER_FULL: 0;
    return res;
}

NK_API
void nk_glfw3_create_cmd(
    struct nk_context        *ctx,
    GLFWwindow               *window,
    struct nk_buffer         *cmd_extra,
    VkCommandBuffer           command_buffer,
    enum nk_anti_aliasing     AA,
    int                       frame,
    int                       max_frames)
{
  struct nk_glfw_device *dev = &glfw.d0;
  struct nk_glfw_win    *win = &glfw.w0;
  if(window == glfw.w1.win) { dev = &glfw.d1; win = &glfw.w1; }
  struct Mat4f projection = {
    {2.0f,  0.0f,  0.0f, 0.0f,
     0.0f, -2.0f,  0.0f, 0.0f,
     0.0f,  0.0f, -1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f}};
  projection.m[0] /= win->width;
  projection.m[5] /= win->height;

  memcpy(dev->mapped_uniform, &projection, sizeof(projection));

  VkViewport viewport = {
    .width    = (float)win->width,
    .height   = (float)win->height,
    .maxDepth = 1.0f,
  };
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, dev->pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      dev->pipeline_layout, 0, 1,
      &dev->uniform_descriptor_set, 0, NULL);

  /* fill convert configuration */
  static const struct nk_draw_vertex_layout_element vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_glfw_vertex, position)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_glfw_vertex, uv)},
    {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_glfw_vertex, col)},
    {NK_VERTEX_LAYOUT_END}};
  struct nk_convert_config config = {
    .vertex_layout = vertex_layout,
    .vertex_size = sizeof(struct nk_glfw_vertex),
    .vertex_alignment = NK_ALIGNOF(struct nk_glfw_vertex),
    .tex_null = glfw.tex_null,
    .circle_segment_count = 22,
    .curve_segment_count = 22,
    .arc_segment_count = 22,
    .global_alpha = 1.0f,
    .shape_AA = AA,
    .line_AA = AA,
  };

  /* setup buffers to load vertices and elements */
  struct nk_buffer vbuf, ebuf;
  const int num = max_frames;
  nk_buffer_init_fixed(&vbuf, dev->mapped_vertex+frame*dev->max_vertex_buffer /num, (size_t)dev->max_vertex_buffer/num);
  nk_buffer_init_fixed(&ebuf, dev->mapped_index +frame*dev->max_element_buffer/num, (size_t)dev->max_element_buffer/num);
  nk_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);
  if(cmd_extra) nk_convert_extra(ctx, cmd_extra, &dev->cmds, &vbuf, &ebuf, &config);

  VkDeviceSize voffset = frame*dev->max_vertex_buffer/num;
  VkDeviceSize ioffset = frame*dev->max_element_buffer/num;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &dev->vertex_buffer, &voffset);
  vkCmdBindIndexBuffer(command_buffer, dev->index_buffer, ioffset, VK_INDEX_TYPE_UINT16);

  int xpos, ypos;
  glfwGetWindowPos(win->win, &xpos, &ypos);
  memcpy(dev->push_constant+25, &xpos, sizeof(xpos));
  vkCmdPushConstants(command_buffer, dev->pipeline_layout,
      VK_SHADER_STAGE_ALL, 0, dev->push_constant_size, dev->push_constant);

  VkDescriptorSet current_texture = 0;
  uint32_t index_offset = 0;
  const struct nk_draw_command *cmd;
  // bind font texture first, just to keep stuff initialised in case
  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      dev->pipeline_layout, 1, 1,
      (VkDescriptorSet*)&glfw.font_dset,
      0, NULL);
  nk_draw_foreach(cmd, ctx, &dev->cmds)
  { // iterate over draw commands and issue as vulkan draw call
    if(cmd->texture.ptr && cmd->texture.ptr != current_texture)
    {
      vkCmdBindDescriptorSets(
          command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
          dev->pipeline_layout, 1, 1,
          (VkDescriptorSet*)&cmd->texture.ptr,
          0, NULL);
      current_texture = cmd->texture.ptr; // i think this is already checked before pushing the cmd
    }
    if(!cmd->elem_count) continue;

    float str = cmd->strength;
    if(!cmd->texture.ptr) str = -1.0f;
    vkCmdPushConstants(command_buffer, dev->pipeline_layout,
        VK_SHADER_STAGE_ALL, dev->push_constant_size-sizeof(float),
        sizeof(float), &str);

    VkRect2D scissor = { // anti-aliasing needs one more pixel to the left/top:
      .offset.x      = (int32_t)(NK_MAX(cmd->clip_rect.x-1, 0.f)),
      .offset.y      = (int32_t)(NK_MAX(cmd->clip_rect.y-1, 0.f)),
      .extent.width  = (uint32_t)(cmd->clip_rect.w+1),
      .extent.height = (uint32_t)(cmd->clip_rect.h+1),
    };
    // hack to draw rect with bg colour without clipping to window:
    if(index_offset < 30) // no idea what require the first many indices here
      scissor = (VkRect2D){.offset.x=0,.offset.y=0,.extent.width=100000,.extent.height=100000};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdDrawIndexed(command_buffer, cmd->elem_count, 1, index_offset, 0, 0);
    index_offset += cmd->elem_count;
  }
  nk_clear(ctx);
}

NK_API void nk_glfw3_keyboard_callback(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  struct nk_context   *ctx = glfw.w0.ctx;
  if(w == glfw.w1.win) ctx = glfw.w1.ctx;

  const enum nk_keys translate[GLFW_KEY_LAST+1] = {
    [GLFW_KEY_DELETE]    = NK_KEY_DEL,
    [GLFW_KEY_ENTER]     = NK_KEY_ENTER,
    [GLFW_KEY_BACKSPACE] = NK_KEY_BACKSPACE,
    [GLFW_KEY_UP]        = NK_KEY_UP,
    [GLFW_KEY_DOWN]      = NK_KEY_DOWN,
    [GLFW_KEY_LEFT]      = NK_KEY_LEFT,
    [GLFW_KEY_RIGHT]     = NK_KEY_RIGHT,
    [GLFW_KEY_HOME]      = NK_KEY_TEXT_START,
    [GLFW_KEY_END]       = NK_KEY_TEXT_END,
    // [GLFW_KEY_END/HOME]       = NK_KEY_SCROLL_START / SCROLL_END
    [GLFW_KEY_PAGE_DOWN] = NK_KEY_SCROLL_DOWN,
    [GLFW_KEY_PAGE_UP]   = NK_KEY_SCROLL_UP,
    [GLFW_KEY_LEFT_SHIFT]  = NK_KEY_SHIFT,
    [GLFW_KEY_RIGHT_SHIFT] = NK_KEY_SHIFT,
  };
  enum nk_keys nkey = translate[key];

  if((mods & GLFW_MOD_CONTROL) && (action == GLFW_PRESS))
  {
    if(key == GLFW_KEY_C) nk_input_key(ctx, NK_KEY_COPY, nk_true);
    if(key == GLFW_KEY_V) nk_input_key(ctx, NK_KEY_PASTE, nk_true);
    if(key == GLFW_KEY_X) nk_input_key(ctx, NK_KEY_CUT, nk_true);
    if(key == GLFW_KEY_Z) nk_input_key(ctx, NK_KEY_TEXT_UNDO, nk_true);
    if(key == GLFW_KEY_R) nk_input_key(ctx, NK_KEY_TEXT_REDO, nk_true);
    if(key == GLFW_KEY_LEFT)  nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT,  nk_true);
    if(key == GLFW_KEY_RIGHT) nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, nk_true);
    if(key == GLFW_KEY_B) nk_input_key(ctx, NK_KEY_TEXT_LINE_START, nk_true);
    if(key == GLFW_KEY_E) nk_input_key(ctx, NK_KEY_TEXT_LINE_END, nk_true);
    if(key == GLFW_KEY_A) nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, nk_true);
  }
#if 0 // ???
  else
  {
    nk_input_key(ctx, NK_KEY_COPY, 0);
    nk_input_key(ctx, NK_KEY_PASTE, 0);
    nk_input_key(ctx, NK_KEY_CUT, 0);
    nk_input_key(ctx, NK_KEY_SHIFT, 0);
  }
#endif

  if(nkey == NK_KEY_NONE) return;

  if(action == GLFW_REPEAT)
  {
    nk_input_key(ctx, nkey, nk_false);
    nk_input_key(ctx, nkey, nk_true);
  }
  else if(action == GLFW_PRESS)
  {
    nk_input_key(ctx, nkey, nk_true);
  }
  else if(action == GLFW_RELEASE)
  {
    nk_input_key(ctx, nkey, nk_false);
  }
}

NK_API void nk_glfw3_char_callback(GLFWwindow *w, unsigned int codepoint)
{
  struct nk_context   *ctx = glfw.w0.ctx;
  if(w == glfw.w1.win) ctx = glfw.w1.ctx;
  nk_input_unicode(ctx, codepoint);
}

NK_API void nk_glfw3_scroll_callback(
    GLFWwindow *w, double xoff, double yoff)
{
  struct nk_context   *ctx = glfw.w0.ctx;
  if(w == glfw.w1.win) ctx = glfw.w1.ctx;
  nk_input_scroll(ctx, (struct nk_vec2){xoff, yoff});
}

NK_API void
nk_glfw3_mouse_position_callback(
    GLFWwindow *w, double x, double y)
{
  struct nk_context   *ctx = glfw.w0.ctx;
  if(w == glfw.w1.win) ctx = glfw.w1.ctx;
  nk_input_motion(ctx, (int)x, (int)y);
}

NK_API void
nk_glfw3_mouse_button_callback(
    GLFWwindow *w, int button,
    int action, int mods)
{
  struct nk_context   *ctx = glfw.w0.ctx;
  if(w == glfw.w1.win) ctx = glfw.w1.ctx;

  double x, y;
  glfwGetCursorPos(w, &x, &y);
  float xscale, yscale;
  dt_gui_content_scale(w, &xscale, &yscale);
  x *= xscale; y *= yscale;
  if (button == GLFW_MOUSE_BUTTON_LEFT && action != GLFW_PRESS)
  {
    struct nk_glfw_win  *win = &glfw.w0;
    if(w == glfw.w1.win) win = &glfw.w1;
    double dt = glfwGetTime() - win->last_button_click;
    if(dt > NK_GLFW_DOUBLE_CLICK_LO && dt < NK_GLFW_DOUBLE_CLICK_HI)
    {
      nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, nk_true);
      nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, nk_false);
    }
    win->last_button_click = glfwGetTime();
  }

#ifdef __APPLE__
  if (button == GLFW_MOUSE_BUTTON_LEFT && ((mods & GLFW_MOD_CONTROL) == 0))
#else
  if (button == GLFW_MOUSE_BUTTON_LEFT)
#endif
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y, action == GLFW_PRESS);
#ifdef __APPLE__
  if (button == GLFW_MOUSE_BUTTON_LEFT && ((mods & GLFW_MOD_CONTROL) != 0))
#else
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
#endif
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y, action == GLFW_PRESS);

  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y, action == GLFW_PRESS);
}

NK_INTERN void nk_glfw3_clipboard_paste(
    nk_handle usr,
    struct nk_text_edit *edit)
{
  GLFWwindow *w = (GLFWwindow*)usr.ptr;
  const char *text = glfwGetClipboardString(w);
  if (text) nk_textedit_paste(edit, text, nk_strlen(text));
}

NK_INTERN void nk_glfw3_clipboard_copy(
    nk_handle usr, const char *text,
    int len)
{
  GLFWwindow *w = (GLFWwindow*)usr.ptr;
  char *str = 0;
  if (!len) return;
  str = (char *)malloc((size_t)len + 1);
  if (!str) return;
  memcpy(str, text, (size_t)len);
  str[len] = '\0';
  glfwSetClipboardString(w, str);
  free(str);
}

NK_API void
nk_glfw3_win1_open(
    struct nk_context *ctx,
    VkRenderPass render_pass,
    GLFWwindow *win,
    VkDeviceSize max_vertex_buffer,
    VkDeviceSize max_element_buffer)
{
  glfw.w1.win = win;
  glfw.w1.ctx = ctx;
  ctx->clip.copy  = nk_glfw3_clipboard_copy;
  ctx->clip.paste = nk_glfw3_clipboard_paste;
  ctx->clip.userdata = nk_handle_ptr(win);
  glfw.w1.last_button_click = 0;

  glfwGetFramebufferSize(win, &glfw.w1.width, &glfw.w1.height);

  nk_glfw3_device_create(
      &glfw.d1,
      render_pass,
      max_vertex_buffer,
      max_element_buffer);
}

NK_API void
nk_glfw3_win1_close()
{
  glfw.w1.win = 0;
  nk_glfw3_device_destroy(&glfw.d1);
}

NK_API void
nk_glfw3_init(
    struct nk_context *ctx,
    VkRenderPass render_pass,
    GLFWwindow *win,
    VkDevice logical_device,
    VkPhysicalDevice physical_device,
    VkDeviceSize max_vertex_buffer,
    VkDeviceSize max_element_buffer)
{
  memset(&glfw, 0, sizeof(struct nk_glfw));
  glfw.w0.win = win;
  glfw.w0.ctx = ctx;
  ctx->clip.copy  = nk_glfw3_clipboard_copy;
  ctx->clip.paste = nk_glfw3_clipboard_paste;
  ctx->clip.userdata = nk_handle_ptr(win);
  glfw.w0.last_button_click = 0;

  glfwGetFramebufferSize(win, &glfw.w0.width, &glfw.w0.height);

  glfw.logical_device = logical_device;
  glfw.physical_device = physical_device;
  nk_glfw3_create_sampler();
  nk_glfw3_create_font_descriptor_pool();
  nk_glfw3_create_font_dset();
  nk_glfw3_device_create(
      &glfw.d0,
      render_pass,
      max_vertex_buffer,
      max_element_buffer);
}

#endif
