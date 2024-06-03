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
NK_API void nk_glfw3_font_stash_begin(struct nk_font_atlas **atlas);
NK_API void nk_glfw3_font_stash_end(struct nk_context *ctx, VkCommandBuffer cmd, VkQueue graphics_queue);
NK_API void nk_glfw3_font_cleanup();
NK_API void nk_glfw3_new_frame(struct nk_context *ctx);
NK_API void nk_glfw3_create_cmd(
    struct nk_context *ctx,
    VkCommandBuffer cmd,
    enum nk_anti_aliasing AA,
    int frame);
NK_API void nk_glfw3_resize(uint32_t framebuffer_width,
                            uint32_t framebuffer_height);
NK_API void nk_glfw3_device_destroy(void);
NK_API VkResult nk_glfw3_device_create(
    VkRenderPass render_pass,
    VkDevice logical_device, VkPhysicalDevice physical_device,
    VkDeviceSize max_vertex_buffer, VkDeviceSize max_element_buffer);

NK_API void nk_glfw3_keyboard_callback(struct nk_context *ctx, GLFWwindow *w, int key, int scancode, int action, int mods);
NK_API void nk_glfw3_char_callback(GLFWwindow *win, unsigned int codepoint);
NK_API void nk_glfw3_scroll_callback(GLFWwindow *win, double xoff, double yoff);
NK_API void nk_glfw3_mouse_button_callback(GLFWwindow *win, int button, int action, int mods);

NK_API void nk_glfw3_setup_display_colour_management(float *g0, float *M0, float *g1, float *M1, int xpos1, int bitdepth);
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

#ifndef NK_GLFW_TEXT_MAX
#define NK_GLFW_TEXT_MAX 256
#endif
#ifndef NK_GLFW_DOUBLE_CLICK_LO
#define NK_GLFW_DOUBLE_CLICK_LO 0.02
#endif
#ifndef NK_GLFW_DOUBLE_CLICK_HI
#define NK_GLFW_DOUBLE_CLICK_HI 0.2
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
  struct nk_draw_null_texture tex_null;
  int max_vertex_buffer;
  int max_element_buffer;

  VkDevice logical_device;
  VkPhysicalDevice physical_device;
  VkRenderPass render_pass;
  VkSampler sampler;
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
  VkDescriptorSetLayout texture_descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkImage font_image;
  VkDescriptorSet font_dset;
  VkImageView font_image_view;
  VkDeviceMemory font_memory;

  int push_constant_size;
  int push_constant[128];
};

static struct nk_glfw
{
  GLFWwindow *win;
  int width, height;
  int display_width, display_height;
  struct nk_glfw_device vulkan;
  struct nk_font_atlas atlas;
  struct nk_vec2 fb_scale;
  unsigned int text[NK_GLFW_TEXT_MAX];
  int text_len;
  struct nk_vec2 scroll;
  double last_button_click;
  int is_double_click_down;
  struct nk_vec2 double_click_pos;

  // key repeat
  int key_repeat[10];

  // colour management
  float gamma0[3];
  float gamma1[3];
  float M0[9];
  float M1[9];
  int winposx, borderx;
  float maxval;
} glfw;

struct Mat4f {
    float m[16];
};

NK_API void nk_glfw3_setup_display_colour_management(float *g0, float *M0, float *g1, float *M1, int xpos1, int bitdepth)
{
  int xpos, ypos;
  glfwGetWindowPos(glfw.win, &xpos, &ypos);
  memcpy(glfw.gamma0, g0, sizeof(glfw.gamma0));
  memcpy(glfw.gamma1, g1, sizeof(glfw.gamma1));
  memcpy(glfw.M0, M0, sizeof(glfw.M0));
  memcpy(glfw.M1, M1, sizeof(glfw.M1));
  glfw.borderx = xpos1;
  glfw.maxval = powf(2.0f, bitdepth);
  glfw.vulkan.push_constant_size = (2*(3+9)+3)*sizeof(float);
  memcpy(glfw.vulkan.push_constant,     glfw.gamma0, sizeof(glfw.gamma0));
  memcpy(glfw.vulkan.push_constant +3,  glfw.gamma1, sizeof(glfw.gamma1));
  memcpy(glfw.vulkan.push_constant +6,  glfw.M0, sizeof(glfw.M0));
  memcpy(glfw.vulkan.push_constant+15,  glfw.M1, sizeof(glfw.M1));
  memcpy(glfw.vulkan.push_constant+24, &glfw.maxval, sizeof(glfw.maxval));
  memcpy(glfw.vulkan.push_constant+25, &xpos, sizeof(xpos));
  memcpy(glfw.vulkan.push_constant+26, &glfw.borderx, sizeof(glfw.borderx));
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
nk_glfw3_create_sampler(struct nk_glfw_device *dev)
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
    .mipLodBias = 0.0f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.0f,
    .maxLod = 0.0f,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
  };
  return vkCreateSampler(dev->logical_device, &sampler_info, NULL, &dev->sampler);
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

    result = vkCreateBuffer(dev->logical_device, &buffer_info, NULL, buffer);
    if(result != VK_SUCCESS) return result;

    vkGetBufferMemoryRequirements(dev->logical_device, *buffer, &mem_reqs);

    memset(&alloc_info, 0, sizeof(VkMemoryAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        dev->physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(dev->logical_device, &alloc_info, NULL, memory);
    if(result != VK_SUCCESS) return result;
    result = vkBindBufferMemory(dev->logical_device, *buffer, *memory, 0);
    return result;
}

NK_INTERN VkResult nk_glfw3_create_descriptor_pool(struct nk_glfw_device *dev)
{
  VkDescriptorPoolSize pool_sizes[2];
  VkDescriptorPoolCreateInfo pool_info;

  memset(&pool_sizes, 0, sizeof(VkDescriptorPoolSize) * 2);
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 1;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = 1; // font atlas

  memset(&pool_info, 0, sizeof(VkDescriptorPoolCreateInfo));
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1 + 1;

  return vkCreateDescriptorPool(
      dev->logical_device, &pool_info, NULL,
      &dev->descriptor_pool);
}

NK_INTERN VkResult
nk_glfw3_create_uniform_descriptor_set_layout(struct nk_glfw_device *dev)
{
  VkDescriptorSetLayoutBinding binding;
  VkDescriptorSetLayoutCreateInfo descriptor_set_info;

  memset(&binding, 0, sizeof(VkDescriptorSetLayoutBinding));
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  memset(&descriptor_set_info, 0, sizeof(VkDescriptorSetLayoutCreateInfo));
  descriptor_set_info.sType =
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_info.bindingCount = 1;
  descriptor_set_info.pBindings = &binding;

  return vkCreateDescriptorSetLayout(dev->logical_device, &descriptor_set_info,
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

  result = vkAllocateDescriptorSets(dev->logical_device, &allocate_info,
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

  vkUpdateDescriptorSets(dev->logical_device, 1, &descriptor_write, 0, NULL);
  return VK_SUCCESS;
}

NK_INTERN VkResult
nk_glfw3_create_texture_descriptor_set_layout(struct nk_glfw_device *dev)
{
  VkDescriptorSetLayoutBinding binding;
  VkDescriptorSetLayoutCreateInfo descriptor_set_info;

  memset(&binding, 0, sizeof(VkDescriptorSetLayoutBinding));
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_ALL;//VK_SHADER_STAGE_FRAGMENT_BIT;

  memset(&descriptor_set_info, 0, sizeof(VkDescriptorSetLayoutCreateInfo));
  descriptor_set_info.sType =
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_info.bindingCount = 1;
  descriptor_set_info.pBindings = &binding;

  VkResult result = vkCreateDescriptorSetLayout(
      dev->logical_device, &descriptor_set_info,
      NULL, &dev->texture_descriptor_set_layout);
  if(result != VK_SUCCESS) return result;

  NK_ASSERT(result == VK_SUCCESS);

  VkDescriptorSetAllocateInfo allocate_info = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool     = dev->descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &dev->texture_descriptor_set_layout,
  };
  return vkAllocateDescriptorSets(dev->logical_device, &allocate_info, &dev->font_dset);
}

NK_INTERN VkResult
nk_glfw3_create_pipeline_layout(struct nk_glfw_device *dev)
{
    VkPipelineLayoutCreateInfo pipeline_layout_info;
    VkDescriptorSetLayout descriptor_set_layouts[2];

    descriptor_set_layouts[0] = dev->uniform_descriptor_set_layout;
    descriptor_set_layouts[1] = dev->texture_descriptor_set_layout;

    VkPushConstantRange pcrange = {
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset     = 0,
      .size       = dev->push_constant_size,
    };
    memset(&pipeline_layout_info, 0, sizeof(VkPipelineLayoutCreateInfo));
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 2;
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &pcrange;

    return vkCreatePipelineLayout(dev->logical_device, &pipeline_layout_info,
                                     NULL, &dev->pipeline_layout);
}

NK_INTERN VkPipelineShaderStageCreateInfo
nk_glfw3_create_shader(struct nk_glfw_device *dev, unsigned char *spv_shader,
                       uint32_t size, VkShaderStageFlagBits stage_bit) {
    VkShaderModuleCreateInfo create_info;
    VkPipelineShaderStageCreateInfo shader_info = {0};
    VkShaderModule module = NULL;
    VkResult result;

    memset(&create_info, 0, sizeof(VkShaderModuleCreateInfo));
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (const uint32_t *)spv_shader;
    result =
        vkCreateShaderModule(dev->logical_device, &create_info, NULL, &module);
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

    result = vkCreateGraphicsPipelines(dev->logical_device, NULL, 1,
                                       &pipeline_info, NULL, &dev->pipeline);

    vkDestroyShaderModule(dev->logical_device, shader_stages[0].module, NULL);
    vkDestroyShaderModule(dev->logical_device, shader_stages[1].module, NULL);
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
  result = nk_glfw3_create_texture_descriptor_set_layout(dev);
  if(result != VK_SUCCESS) return result;
  result = nk_glfw3_create_pipeline_layout(dev);
  if(result != VK_SUCCESS) return result;
  result = nk_glfw3_create_pipeline(dev);
  if(result != VK_SUCCESS) return result;
  return result;
}

NK_API VkResult nk_glfw3_device_create(
    VkRenderPass render_pass,
    VkDevice logical_device, VkPhysicalDevice physical_device,
    VkDeviceSize max_vertex_buffer, VkDeviceSize max_element_buffer)
{
  struct nk_glfw_device *dev = &glfw.vulkan;
  dev->max_vertex_buffer = max_vertex_buffer;
  dev->max_element_buffer = max_element_buffer;
  dev->push_constant_size = (2*(3+9)+3)*sizeof(float);
  nk_buffer_init_default(&dev->cmds);
  dev->logical_device = logical_device;
  dev->physical_device = physical_device;
  dev->render_pass = render_pass;

  nk_glfw3_create_sampler(dev);
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

  vkMapMemory(dev->logical_device, dev->vertex_memory, 0, max_vertex_buffer, 0, &dev->mapped_vertex);
  vkMapMemory(dev->logical_device, dev->index_memory, 0, max_element_buffer, 0, &dev->mapped_index);
  vkMapMemory(dev->logical_device, dev->uniform_memory, 0, sizeof(struct Mat4f), 0, &dev->mapped_uniform);

  return nk_glfw3_create_render_resources(dev);
}

NK_INTERN VkResult
nk_glfw3_device_upload_atlas(
    VkCommandBuffer cmd,
    VkQueue graphics_queue,
    const void *image, int width,
    int height)
{
    struct nk_glfw_device *dev = &glfw.vulkan;

    VkImageCreateInfo image_info;
    VkResult result;
    VkMemoryRequirements mem_reqs;
    VkMemoryAllocateInfo alloc_info;
    VkBufferCreateInfo buffer_info;
    uint8_t *data = 0;
    VkCommandBufferBeginInfo begin_info;
    VkImageMemoryBarrier image_memory_barrier;
    VkBufferImageCopy buffer_copy_region;
    VkImageMemoryBarrier image_shader_memory_barrier;
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
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = (uint32_t)width;
    image_info.extent.height = (uint32_t)height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(dev->logical_device, &image_info, NULL, &dev->font_image);
    if(result != VK_SUCCESS) return result;

    vkGetImageMemoryRequirements(
        dev->logical_device, dev->font_image, &mem_reqs);

    memset(&alloc_info, 0, sizeof(VkMemoryAllocateInfo));
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        dev->physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(dev->logical_device, &alloc_info, NULL,
                              &dev->font_memory);
    if(result != VK_SUCCESS) return result;
    result = vkBindImageMemory(dev->logical_device, dev->font_image,
                               dev->font_memory, 0);
    if(result != VK_SUCCESS) return result;

    memset(&buffer_info, 0, sizeof(VkBufferCreateInfo));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = alloc_info.allocationSize;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(dev->logical_device, &buffer_info, NULL,
                            &staging_buffer.buffer);
    if(result != VK_SUCCESS) return result;
    vkGetBufferMemoryRequirements(dev->logical_device, staging_buffer.buffer,
                                  &mem_reqs);

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        dev->physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(dev->logical_device, &alloc_info, NULL,
                              &staging_buffer.memory);
    if(result != VK_SUCCESS) return result;
    result = vkBindBufferMemory(dev->logical_device, staging_buffer.buffer,
                                staging_buffer.memory, 0);
    if(result != VK_SUCCESS) return result;

    result = vkMapMemory(dev->logical_device, staging_buffer.memory, 0, alloc_info.allocationSize, 0, (void **)&data);
    if(result != VK_SUCCESS) return result;
    memcpy(data, image, width * height * 4);
    vkUnmapMemory(dev->logical_device, staging_buffer.memory);

    memset(&begin_info, 0, sizeof(VkCommandBufferBeginInfo));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    /*
    use the same command buffer as for render as we are regenerating the
    buffer during render anyway
    */
    result = vkBeginCommandBuffer(cmd, &begin_info);
    if(result != VK_SUCCESS) return result;

    memset(&image_memory_barrier, 0, sizeof(VkImageMemoryBarrier));
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.image = dev->font_image;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.levelCount = 1;
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
        cmd, staging_buffer.buffer, dev->font_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_copy_region);

    memset(&image_shader_memory_barrier, 0, sizeof(VkImageMemoryBarrier));
    image_shader_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_shader_memory_barrier.image = dev->font_image;
    image_shader_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_shader_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_shader_memory_barrier.oldLayout =
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_shader_memory_barrier.newLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_shader_memory_barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_shader_memory_barrier.subresourceRange.levelCount = 1;
    image_shader_memory_barrier.subresourceRange.layerCount = 1;
    image_shader_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    image_shader_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &image_shader_memory_barrier);

    result = vkEndCommandBuffer(cmd);
    if(result != VK_SUCCESS) return result;

    memset(&fence_create, 0, sizeof(VkFenceCreateInfo));
    fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    result = vkCreateFence(dev->logical_device, &fence_create, NULL, &fence);
    if(result != VK_SUCCESS) return result;

    memset(&submit_info, 0, sizeof(VkSubmitInfo));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    result = vkQueueSubmit(graphics_queue, 1, &submit_info, fence);
    if(result != VK_SUCCESS) return result;
    result =
        vkWaitForFences(dev->logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if(result != VK_SUCCESS) return result;

    vkDestroyFence(dev->logical_device, fence, NULL);

    vkFreeMemory(dev->logical_device, staging_buffer.memory, NULL);
    vkDestroyBuffer(dev->logical_device, staging_buffer.buffer, NULL);

    memset(&image_view_info, 0, sizeof(VkImageViewCreateInfo));
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = dev->font_image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = image_info.format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.layerCount = 1;
    image_view_info.subresourceRange.levelCount = 1;

    result = vkCreateImageView(dev->logical_device, &image_view_info, NULL,
                               &dev->font_image_view);
    if(result != VK_SUCCESS) return result;

    VkDescriptorImageInfo descriptor_image_info = {
      .sampler     = dev->sampler,
      .imageView   = dev->font_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet descriptor_write = {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = dev->font_dset,
      .dstBinding      = 0,
      .dstArrayElement = 0,
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo      = &descriptor_image_info,
    };
    vkUpdateDescriptorSets(dev->logical_device, 1, &descriptor_write, 0, NULL);
    return VK_SUCCESS;
}

NK_INTERN void nk_glfw3_destroy_render_resources(struct nk_glfw_device *dev)
{
    vkDestroyPipeline(dev->logical_device, dev->pipeline, NULL);
    vkDestroyPipelineLayout(dev->logical_device, dev->pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(dev->logical_device,
                                 dev->texture_descriptor_set_layout, NULL);
    vkDestroyDescriptorSetLayout(dev->logical_device,
                                 dev->uniform_descriptor_set_layout, NULL);
    vkDestroyDescriptorPool(dev->logical_device, dev->descriptor_pool, NULL);
}

NK_API void
nk_glfw3_resize(
    uint32_t framebuffer_width,
    uint32_t framebuffer_height)
{
  glfwGetWindowSize(glfw.win, &glfw.width, &glfw.height);
  glfwGetFramebufferSize(glfw.win, &glfw.display_width, &glfw.display_height);
  glfw.fb_scale.x = (float)glfw.display_width / (float)glfw.width;
  glfw.fb_scale.y = (float)glfw.display_height / (float)glfw.height;
}

NK_API void nk_glfw3_device_destroy(void)
{
    struct nk_glfw_device *dev = &glfw.vulkan;

    vkDeviceWaitIdle(dev->logical_device);

    nk_glfw3_destroy_render_resources(dev);

    vkUnmapMemory(dev->logical_device, dev->vertex_memory);
    vkUnmapMemory(dev->logical_device, dev->index_memory);
    vkUnmapMemory(dev->logical_device, dev->uniform_memory);

    vkFreeMemory(dev->logical_device, dev->vertex_memory, NULL);
    vkFreeMemory(dev->logical_device, dev->index_memory, NULL);
    vkFreeMemory(dev->logical_device, dev->uniform_memory, NULL);

    vkDestroyBuffer(dev->logical_device, dev->vertex_buffer, NULL);
    vkDestroyBuffer(dev->logical_device, dev->index_buffer, NULL);
    vkDestroyBuffer(dev->logical_device, dev->uniform_buffer, NULL);

    vkDestroySampler(dev->logical_device, dev->sampler, NULL);

    vkFreeMemory(dev->logical_device, dev->font_memory, NULL);
    vkDestroyImage(dev->logical_device, dev->font_image, NULL);
    vkDestroyImageView(dev->logical_device, dev->font_image_view, NULL);
    nk_buffer_free(&dev->cmds);
}

NK_API
void nk_glfw3_shutdown(void)
{
  nk_font_atlas_clear(&glfw.atlas);
  nk_glfw3_device_destroy();
  memset(&glfw, 0, sizeof(glfw));
}

NK_API void nk_glfw3_font_cleanup()
{
  struct nk_glfw_device *dev = &glfw.vulkan;
  vkDestroyImage(dev->logical_device, dev->font_image, 0);
  vkDestroyImageView(dev->logical_device, dev->font_image_view, 0);
  vkFreeMemory(dev->logical_device, dev->font_memory, 0);
  nk_font_atlas_cleanup(&glfw.atlas);
}

NK_API void nk_glfw3_font_stash_begin(struct nk_font_atlas **atlas)
{
  nk_font_atlas_init_default(&glfw.atlas);
  nk_font_atlas_begin(&glfw.atlas);
  *atlas = &glfw.atlas;
}

NK_API void nk_glfw3_font_stash_end(
    struct nk_context *ctx,
    VkCommandBuffer cmd,
    VkQueue graphics_queue)
{
  struct nk_glfw_device *dev = &glfw.vulkan;
  const void *image;
  int w, h;
  image = nk_font_atlas_bake(&glfw.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
  nk_glfw3_device_upload_atlas(cmd, graphics_queue, image, w, h);
  nk_font_atlas_end(
      &glfw.atlas,
      nk_handle_ptr(dev->font_dset),
      &dev->tex_null);

  if (glfw.atlas.default_font)
    nk_style_set_font(ctx, &glfw.atlas.default_font->handle);
}

NK_API void nk_glfw3_new_frame(struct nk_context *ctx)
{
    int i;
    double x, y;
    struct GLFWwindow *win = glfw.win;

    nk_input_begin(ctx);
    for (i = 0; i < glfw.text_len; ++i)
        nk_input_unicode(ctx, glfw.text[i]);

#ifdef NK_GLFW_GL4_MOUSE_GRABBING
    /* optional grabbing behavior */
    if (ctx->input.mouse.grab)
        glfwSetInputMode(glfw.win, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    else if (ctx->input.mouse.ungrab)
        glfwSetInputMode(glfw.win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif

    if(glfw.key_repeat[0])
    {
      nk_input_key(ctx, NK_KEY_DEL, nk_false);
      nk_input_key(ctx, NK_KEY_DEL, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_DEL, glfwGetKey(win, GLFW_KEY_DELETE) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_ENTER, glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TAB, glfwGetKey(win, GLFW_KEY_TAB) == GLFW_PRESS);
    if(glfw.key_repeat[2])
    {
      nk_input_key(ctx, NK_KEY_BACKSPACE, nk_false);
      nk_input_key(ctx, NK_KEY_BACKSPACE, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_BACKSPACE, glfwGetKey(win, GLFW_KEY_BACKSPACE) == GLFW_PRESS);
    if(glfw.key_repeat[3])
    {
      nk_input_key(ctx, NK_KEY_UP, nk_false);
      nk_input_key(ctx, NK_KEY_UP, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_UP, glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS);
    if(glfw.key_repeat[4])
    {
      nk_input_key(ctx, NK_KEY_DOWN, nk_false);
      nk_input_key(ctx, NK_KEY_DOWN, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_DOWN, glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_START,
                 glfwGetKey(win, GLFW_KEY_HOME) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_END,
                 glfwGetKey(win, GLFW_KEY_END) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_START,
                 glfwGetKey(win, GLFW_KEY_HOME) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_END,
                 glfwGetKey(win, GLFW_KEY_END) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_DOWN,
                 glfwGetKey(win, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_UP,
                 glfwGetKey(win, GLFW_KEY_PAGE_UP) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SHIFT,
                 glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        nk_input_key(ctx, NK_KEY_COPY,
                     glfwGetKey(win, GLFW_KEY_C) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_PASTE,
                     glfwGetKey(win, GLFW_KEY_V) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_CUT,
                     glfwGetKey(win, GLFW_KEY_X) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_UNDO,
                     glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_REDO,
                     glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT,
                     glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT,
                     glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_START,
                     glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_END,
                     glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL,
                     glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS);
    }
    else
    {
      if(glfw.key_repeat[5])
      {
        nk_input_key(ctx, NK_KEY_LEFT, nk_false);
        nk_input_key(ctx, NK_KEY_LEFT, nk_true);
      }
      else nk_input_key(ctx, NK_KEY_LEFT, glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS);
      if(glfw.key_repeat[6])
      {
        nk_input_key(ctx, NK_KEY_RIGHT, nk_false);
        nk_input_key(ctx, NK_KEY_RIGHT, nk_true);
      }
      else nk_input_key(ctx, NK_KEY_RIGHT, glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS);
      nk_input_key(ctx, NK_KEY_COPY, 0);
      nk_input_key(ctx, NK_KEY_PASTE, 0);
      nk_input_key(ctx, NK_KEY_CUT, 0);
      nk_input_key(ctx, NK_KEY_SHIFT, 0);
    }

    glfwGetCursorPos(win, &x, &y);
    nk_input_motion(ctx, (int)x, (int)y);
#ifdef NK_GLFW_GL4_MOUSE_GRABBING
    if (ctx->input.mouse.grabbed) {
        glfwSetCursorPos(glfw.win, ctx->input.mouse.prev.x,
                         ctx->input.mouse.prev.y);
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }
#endif
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y,
                    glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) ==
                        GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y,
                    glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_MIDDLE) ==
                        GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y,
                    glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) ==
                        GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)glfw.double_click_pos.x,
                    (int)glfw.double_click_pos.y, glfw.is_double_click_down);
    nk_input_scroll(ctx, glfw.scroll);
    nk_input_end(ctx);
    glfw.text_len = 0;
    glfw.scroll = nk_vec2(0, 0);
}

NK_API
void nk_glfw3_create_cmd(
    struct nk_context    *ctx,
    VkCommandBuffer       command_buffer,
    enum nk_anti_aliasing AA,
    int frame)
{
  struct nk_glfw_device *dev = &glfw.vulkan;
  struct Mat4f projection = {
    {2.0f,  0.0f,  0.0f, 0.0f,
     0.0f, -2.0f,  0.0f, 0.0f,
     0.0f,  0.0f, -1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f}};
  projection.m[0] /= glfw.width;
  projection.m[5] /= glfw.height;

  memcpy(dev->mapped_uniform, &projection, sizeof(projection));

  VkViewport viewport = {
    .width    = (float)glfw.width,
    .height   = (float)glfw.height,
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
    .tex_null = dev->tex_null,
    .circle_segment_count = 22,
    .curve_segment_count = 22,
    .arc_segment_count = 22,
    .global_alpha = 1.0f,
    .shape_AA = AA,
    .line_AA = AA,
  };

  /* setup buffers to load vertices and elements */
  struct nk_buffer vbuf, ebuf;
  nk_buffer_init_fixed(&vbuf, dev->mapped_vertex+frame*dev->max_vertex_buffer /2, (size_t)dev->max_vertex_buffer/2);
  nk_buffer_init_fixed(&ebuf, dev->mapped_index +frame*dev->max_element_buffer/2, (size_t)dev->max_element_buffer/2);
  nk_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);

  VkDeviceSize voffset = frame*dev->max_vertex_buffer/2;
  VkDeviceSize ioffset = frame*dev->max_element_buffer/2;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &dev->vertex_buffer, &voffset);
  vkCmdBindIndexBuffer(command_buffer, dev->index_buffer, ioffset, VK_INDEX_TYPE_UINT16);

  vkCmdPushConstants(command_buffer, dev->pipeline_layout,
      VK_SHADER_STAGE_ALL, 0, dev->push_constant_size, dev->push_constant);

  VkDescriptorSet current_texture = 0;
  uint32_t index_offset = 0;
  const struct nk_draw_command *cmd;
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

    VkRect2D scissor = {
      .offset.x      = (int32_t)(NK_MAX(cmd->clip_rect.x, 0.f)),
      .offset.y      = (int32_t)(NK_MAX(cmd->clip_rect.y, 0.f)),
      .extent.width  = (uint32_t)(cmd->clip_rect.w),
      .extent.height = (uint32_t)(cmd->clip_rect.h),
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdDrawIndexed(command_buffer, cmd->elem_count, 1, index_offset, 0, 0);
    index_offset += cmd->elem_count;
  }
  nk_clear(ctx);
}

NK_API void nk_glfw3_keyboard_callback(struct nk_context *ctx, GLFWwindow *w, int key, int scancode, int action, int mods)
{
  // this is called while wait event, right after that there'll be new_frame nuking all our efforts here.
  // going new_frame earlier will introduce lag key/render, so we remember in a separate list:
  for(int k=0;k<7;k++) glfw.key_repeat[k] = 0;
  if(action == GLFW_REPEAT)
  { // also input key repeats not captured by new_frame
    if     (key == GLFW_KEY_DELETE)    glfw.key_repeat[0] = 1;
    else if(key == GLFW_KEY_ENTER)     glfw.key_repeat[1] = 1;
    else if(key == GLFW_KEY_BACKSPACE) glfw.key_repeat[2] = 1;
    else if(key == GLFW_KEY_UP)        glfw.key_repeat[3] = 1;
    else if(key == GLFW_KEY_DOWN)      glfw.key_repeat[4] = 1;
    else if(key == GLFW_KEY_LEFT)      glfw.key_repeat[5] = 1;
    else if(key == GLFW_KEY_RIGHT)     glfw.key_repeat[6] = 1;
  }
}

NK_API void nk_glfw3_char_callback(GLFWwindow *win, unsigned int codepoint) {
    (void)win;
    if (glfw.text_len < NK_GLFW_TEXT_MAX)
        glfw.text[glfw.text_len++] = codepoint;
}

NK_API void nk_glfw3_scroll_callback(GLFWwindow *win, double xoff,
                                     double yoff) {
    (void)win;
    (void)xoff;
    glfw.scroll.x += (float)xoff;
    glfw.scroll.y += (float)yoff;
}

NK_API void
nk_glfw3_mouse_button_callback(
    GLFWwindow *window, int button,
    int action, int mods)
{
  double x, y;
  NK_UNUSED(mods);
  if (button != GLFW_MOUSE_BUTTON_LEFT)
    return;
  glfwGetCursorPos(window, &x, &y);
  if (action == GLFW_PRESS) {
    double dt = glfwGetTime() - glfw.last_button_click;
    if (dt > NK_GLFW_DOUBLE_CLICK_LO && dt < NK_GLFW_DOUBLE_CLICK_HI) {
      glfw.is_double_click_down = nk_true;
      glfw.double_click_pos = nk_vec2((float)x, (float)y);
    }
    glfw.last_button_click = glfwGetTime();
  } else
    glfw.is_double_click_down = nk_false;
}

NK_INTERN void nk_glfw3_clipboard_paste(nk_handle usr,
                                        struct nk_text_edit *edit) {
    const char *text = glfwGetClipboardString(glfw.win);
    if (text)
        nk_textedit_paste(edit, text, nk_strlen(text));
    (void)usr;
}

NK_INTERN void nk_glfw3_clipboard_copy(nk_handle usr, const char *text,
                                       int len) {
    char *str = 0;
    (void)usr;
    if (!len)
        return;
    str = (char *)malloc((size_t)len + 1);
    if (!str)
        return;
    memcpy(str, text, (size_t)len);
    str[len] = '\0';
    glfwSetClipboardString(glfw.win, str);
    free(str);
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
    glfw.win = win;
    ctx->clip.copy  = nk_glfw3_clipboard_copy;
    ctx->clip.paste = nk_glfw3_clipboard_paste;
    ctx->clip.userdata = nk_handle_ptr(0);
    glfw.last_button_click = 0;

    glfwGetWindowSize(win, &glfw.width, &glfw.height);
    glfwGetFramebufferSize(win, &glfw.display_width, &glfw.display_height);

    nk_glfw3_device_create(
        render_pass,
        logical_device, physical_device,
        max_vertex_buffer,
        max_element_buffer);

    glfw.is_double_click_down = nk_false;
    glfw.double_click_pos = nk_vec2(0, 0);
}

#endif
