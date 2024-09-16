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
NK_API void nk_glfw3_new_frame(struct nk_context *ctx, GLFWwindow *win);
NK_API void nk_glfw3_create_cmd(
    struct nk_context *ctx,
    GLFWwindow *win,
    VkCommandBuffer cmd,
    enum nk_anti_aliasing AA,
    int frame, int max_frames);
NK_API void nk_glfw3_resize(GLFWwindow *win,
    uint32_t framebuffer_width,
    uint32_t framebuffer_height);
NK_API void nk_glfw3_keyboard_callback(struct nk_context *ctx, GLFWwindow *w, int key, int scancode, int action, int mods);
NK_API void nk_glfw3_char_callback(GLFWwindow *win, unsigned int codepoint);
NK_API void nk_glfw3_scroll_callback(GLFWwindow *win, double xoff, double yoff);
NK_API void nk_glfw3_mouse_button_callback(GLFWwindow *win, int button, int action, int mods);

NK_API void nk_glfw3_setup_display_colour_management(float *g0, float *M0, float *g1, float *M1, int xpos1, int bitdepth);
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
  unsigned int text[NK_GLFW_TEXT_MAX];
  int text_len;
  struct nk_vec2 scroll;
  double last_button_click;
  int is_double_click_down;
  struct nk_vec2 double_click_pos;

  int key_repeat[10];
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
  struct nk_font_atlas atlas;
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
  int xpos, ypos;
  float maxval = powf(2.0f, bitdepth);
  for(int i=0;i<2;i++)
  {
    struct nk_glfw_device *dev = &glfw.d0;
    struct nk_glfw_win    *win = &glfw.w0;
    if(i)
    {
      if(!glfw.w1.win) return;
      dev = &glfw.d1;
      win = &glfw.w1;
    }
    glfwGetWindowPos(win->win, &xpos, &ypos);
    dev->push_constant_size = (2*(3+9)+3)*sizeof(float);
    memcpy(dev->push_constant,     g0, 3*sizeof(float));
    memcpy(dev->push_constant +3,  g1, 3*sizeof(float));
    memcpy(dev->push_constant +6,  M0, 9*sizeof(float));
    memcpy(dev->push_constant+15,  M1, 9*sizeof(float));
    memcpy(dev->push_constant+24, &maxval, sizeof(maxval));
    memcpy(dev->push_constant+25, &xpos, sizeof(xpos));
    memcpy(dev->push_constant+26, &xpos1, sizeof(xpos1));
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
    .mipLodBias = 0.0f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.0f,
    .maxLod = 0.0f,
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
  dev->max_vertex_buffer = max_vertex_buffer;
  dev->max_element_buffer = max_element_buffer;
  dev->push_constant_size = (2*(3+9)+3)*sizeof(float);
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

NK_INTERN VkResult
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

    result = vkCreateImage(glfw.logical_device, &image_info, NULL, &glfw.font_image);
    if(result != VK_SUCCESS) return result;

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
    if(result != VK_SUCCESS) return result;
    result = vkBindImageMemory(glfw.logical_device, glfw.font_image,
                               glfw.font_memory, 0);
    if(result != VK_SUCCESS) return result;

    memset(&buffer_info, 0, sizeof(VkBufferCreateInfo));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = alloc_info.allocationSize;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(glfw.logical_device, &buffer_info, NULL,
                            &staging_buffer.buffer);
    if(result != VK_SUCCESS) return result;
    vkGetBufferMemoryRequirements(glfw.logical_device, staging_buffer.buffer,
                                  &mem_reqs);

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = nk_glfw3_find_memory_index(
        glfw.physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(glfw.logical_device, &alloc_info, NULL,
                              &staging_buffer.memory);
    if(result != VK_SUCCESS) return result;
    result = vkBindBufferMemory(glfw.logical_device, staging_buffer.buffer,
                                staging_buffer.memory, 0);
    if(result != VK_SUCCESS) return result;

    result = vkMapMemory(glfw.logical_device, staging_buffer.memory, 0, alloc_info.allocationSize, 0, (void **)&data);
    if(result != VK_SUCCESS) return result;
    memcpy(data, image, width * height * 4);
    vkUnmapMemory(glfw.logical_device, staging_buffer.memory);

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
    image_memory_barrier.image = glfw.font_image;
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
        cmd, staging_buffer.buffer, glfw.font_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_copy_region);

    memset(&image_shader_memory_barrier, 0, sizeof(VkImageMemoryBarrier));
    image_shader_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_shader_memory_barrier.image = glfw.font_image;
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

    result = vkCreateFence(glfw.logical_device, &fence_create, NULL, &fence);
    if(result != VK_SUCCESS) return result;

    memset(&submit_info, 0, sizeof(VkSubmitInfo));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    result = vkQueueSubmit(graphics_queue, 1, &submit_info, fence);
    if(result != VK_SUCCESS) return result;
    result =
        vkWaitForFences(glfw.logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if(result != VK_SUCCESS) return result;

    vkDestroyFence(glfw.logical_device, fence, NULL);

    vkFreeMemory(glfw.logical_device, staging_buffer.memory, NULL);
    vkDestroyBuffer(glfw.logical_device, staging_buffer.buffer, NULL);

    memset(&image_view_info, 0, sizeof(VkImageViewCreateInfo));
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = glfw.font_image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = image_info.format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.layerCount = 1;
    image_view_info.subresourceRange.levelCount = 1;

    result = vkCreateImageView(glfw.logical_device, &image_view_info, NULL,
                               &glfw.font_image_view);
    if(result != VK_SUCCESS) return result;

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
    return VK_SUCCESS;
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
    uint32_t framebuffer_width,
    uint32_t framebuffer_height)
{
  if(w == glfw.w1.win) glfwGetFramebufferSize(glfw.w1.win, &glfw.w1.width, &glfw.w1.height);
  else                 glfwGetFramebufferSize(glfw.w0.win, &glfw.w0.width, &glfw.w0.height);
}

NK_API void nk_glfw3_device_destroy(struct nk_glfw_device *dev)
{
  nk_glfw3_destroy_render_resources(dev);

  vkUnmapMemory(glfw.logical_device, dev->vertex_memory);
  vkUnmapMemory(glfw.logical_device, dev->index_memory);
  vkUnmapMemory(glfw.logical_device, dev->uniform_memory);

  vkFreeMemory(glfw.logical_device, dev->vertex_memory, NULL);
  vkFreeMemory(glfw.logical_device, dev->index_memory, NULL);
  vkFreeMemory(glfw.logical_device, dev->uniform_memory, NULL);

  vkDestroyBuffer(glfw.logical_device, dev->vertex_buffer, NULL);
  vkDestroyBuffer(glfw.logical_device, dev->index_buffer, NULL);
  vkDestroyBuffer(glfw.logical_device, dev->uniform_buffer, NULL);

  nk_buffer_free(&dev->cmds);
}

NK_API
void nk_glfw3_shutdown(void)
{
  vkDeviceWaitIdle(glfw.logical_device);
  vkDestroyDescriptorSetLayout(glfw.logical_device, glfw.font_dset_layout, NULL);
  vkDestroyDescriptorPool(glfw.logical_device, glfw.descriptor_pool, NULL);
  nk_font_atlas_clear(&glfw.atlas);
  vkDestroySampler(glfw.logical_device, glfw.sampler, NULL);
  nk_glfw3_device_destroy(&glfw.d0);
  if(glfw.w1.win) nk_glfw3_device_destroy(&glfw.d1);
  vkDestroyImage(glfw.logical_device, glfw.font_image, NULL);
  vkDestroyImageView(glfw.logical_device, glfw.font_image_view, NULL);
  vkFreeMemory(glfw.logical_device, glfw.font_memory, NULL);

  memset(&glfw, 0, sizeof(glfw));
}

NK_API void nk_glfw3_font_cleanup()
{
  vkDestroyImage(glfw.logical_device, glfw.font_image, 0);
  vkDestroyImageView(glfw.logical_device, glfw.font_image_view, 0);
  vkFreeMemory(glfw.logical_device, glfw.font_memory, 0);
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
  const void *image;
  int w, h;
  image = nk_font_atlas_bake(&glfw.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
  nk_glfw3_device_upload_atlas(cmd, graphics_queue, image, w, h);
  nk_font_atlas_end(
      &glfw.atlas,
      nk_handle_ptr(glfw.font_dset),
      &glfw.tex_null);

  if (glfw.atlas.default_font)
    nk_style_set_font(ctx, &glfw.atlas.default_font->handle);
}

NK_API void nk_glfw3_new_frame(struct nk_context *ctx, GLFWwindow *window)
{
    int i;
    double x, y;
    struct nk_glfw_win *win = &glfw.w0;
    if(window == glfw.w1.win) win = &glfw.w1;

    nk_input_begin(ctx);
    for (i = 0; i < win->text_len; ++i)
        nk_input_unicode(ctx, win->text[i]);

#ifdef NK_GLFW_GL4_MOUSE_GRABBING
    /* optional grabbing behavior */
    if (ctx->input.mouse.grab)
        glfwSetInputMode(win->win, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    else if (ctx->input.mouse.ungrab)
        glfwSetInputMode(win->win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif

    if(win->key_repeat[0])
    {
      nk_input_key(ctx, NK_KEY_DEL, nk_false);
      nk_input_key(ctx, NK_KEY_DEL, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_DEL, glfwGetKey(win->win, GLFW_KEY_DELETE) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_ENTER, glfwGetKey(win->win, GLFW_KEY_ENTER) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TAB, glfwGetKey(win->win, GLFW_KEY_TAB) == GLFW_PRESS);
    if(win->key_repeat[2])
    {
      nk_input_key(ctx, NK_KEY_BACKSPACE, nk_false);
      nk_input_key(ctx, NK_KEY_BACKSPACE, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_BACKSPACE, glfwGetKey(win->win, GLFW_KEY_BACKSPACE) == GLFW_PRESS);
    if(win->key_repeat[3])
    {
      nk_input_key(ctx, NK_KEY_UP, nk_false);
      nk_input_key(ctx, NK_KEY_UP, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_UP, glfwGetKey(win->win, GLFW_KEY_UP) == GLFW_PRESS);
    if(win->key_repeat[4])
    {
      nk_input_key(ctx, NK_KEY_DOWN, nk_false);
      nk_input_key(ctx, NK_KEY_DOWN, nk_true);
    }
    else nk_input_key(ctx, NK_KEY_DOWN, glfwGetKey(win->win, GLFW_KEY_DOWN) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_START,
                 glfwGetKey(win->win, GLFW_KEY_HOME) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_END,
                 glfwGetKey(win->win, GLFW_KEY_END) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_START,
                 glfwGetKey(win->win, GLFW_KEY_HOME) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_END,
                 glfwGetKey(win->win, GLFW_KEY_END) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_DOWN,
                 glfwGetKey(win->win, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_UP,
                 glfwGetKey(win->win, GLFW_KEY_PAGE_UP) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SHIFT,
                 glfwGetKey(win->win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(win->win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (glfwGetKey(win->win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win->win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        nk_input_key(ctx, NK_KEY_COPY,
                     glfwGetKey(win->win, GLFW_KEY_C) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_PASTE,
                     glfwGetKey(win->win, GLFW_KEY_V) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_CUT,
                     glfwGetKey(win->win, GLFW_KEY_X) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_UNDO,
                     glfwGetKey(win->win, GLFW_KEY_Z) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_REDO,
                     glfwGetKey(win->win, GLFW_KEY_R) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT,
                     glfwGetKey(win->win, GLFW_KEY_LEFT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT,
                     glfwGetKey(win->win, GLFW_KEY_RIGHT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_START,
                     glfwGetKey(win->win, GLFW_KEY_B) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_END,
                     glfwGetKey(win->win, GLFW_KEY_E) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL,
                     glfwGetKey(win->win, GLFW_KEY_A) == GLFW_PRESS);
    }
    else
    {
      if(win->key_repeat[5])
      {
        nk_input_key(ctx, NK_KEY_LEFT, nk_false);
        nk_input_key(ctx, NK_KEY_LEFT, nk_true);
      }
      else nk_input_key(ctx, NK_KEY_LEFT, glfwGetKey(win->win, GLFW_KEY_LEFT) == GLFW_PRESS);
      if(win->key_repeat[6])
      {
        nk_input_key(ctx, NK_KEY_RIGHT, nk_false);
        nk_input_key(ctx, NK_KEY_RIGHT, nk_true);
      }
      else nk_input_key(ctx, NK_KEY_RIGHT, glfwGetKey(win->win, GLFW_KEY_RIGHT) == GLFW_PRESS);
      nk_input_key(ctx, NK_KEY_COPY, 0);
      nk_input_key(ctx, NK_KEY_PASTE, 0);
      nk_input_key(ctx, NK_KEY_CUT, 0);
      nk_input_key(ctx, NK_KEY_SHIFT, 0);
    }

    glfwGetCursorPos(win->win, &x, &y);
    nk_input_motion(ctx, (int)x, (int)y);
#ifdef NK_GLFW_GL4_MOUSE_GRABBING
    if (ctx->input.mouse.grabbed) {
        glfwSetCursorPos(win->win, ctx->input.mouse.prev.x,
                         ctx->input.mouse.prev.y);
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }
#endif
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y,
                    glfwGetMouseButton(win->win, GLFW_MOUSE_BUTTON_LEFT) ==
                        GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y,
                    glfwGetMouseButton(win->win, GLFW_MOUSE_BUTTON_MIDDLE) ==
                        GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y,
                    glfwGetMouseButton(win->win, GLFW_MOUSE_BUTTON_RIGHT) ==
                        GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)win->double_click_pos.x,
                    (int)win->double_click_pos.y, win->is_double_click_down);
    nk_input_scroll(ctx, win->scroll);
    nk_input_end(ctx);
    win->text_len = 0;
    win->scroll = nk_vec2(0, 0);
}

NK_API
void nk_glfw3_create_cmd(
    struct nk_context    *ctx,
    GLFWwindow           *window,
    VkCommandBuffer       command_buffer,
    enum nk_anti_aliasing AA,
    int                   frame,
    int                   max_frames)
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

  VkDeviceSize voffset = frame*dev->max_vertex_buffer/num;
  VkDeviceSize ioffset = frame*dev->max_element_buffer/num;
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
  struct nk_glfw_win  *win = &glfw.w0;
  if(w == glfw.w1.win) win = &glfw.w1;
  // this is called while wait event, right after that there'll be new_frame nuking all our efforts here.
  // going new_frame earlier will introduce lag key/render, so we remember in a separate list:
  for(int k=0;k<7;k++) win->key_repeat[k] = 0;
  if(action == GLFW_REPEAT)
  { // also input key repeats not captured by new_frame
    if     (key == GLFW_KEY_DELETE)    win->key_repeat[0] = 1;
    else if(key == GLFW_KEY_ENTER)     win->key_repeat[1] = 1;
    else if(key == GLFW_KEY_BACKSPACE) win->key_repeat[2] = 1;
    else if(key == GLFW_KEY_UP)        win->key_repeat[3] = 1;
    else if(key == GLFW_KEY_DOWN)      win->key_repeat[4] = 1;
    else if(key == GLFW_KEY_LEFT)      win->key_repeat[5] = 1;
    else if(key == GLFW_KEY_RIGHT)     win->key_repeat[6] = 1;
  }
}

NK_API void nk_glfw3_char_callback(GLFWwindow *w, unsigned int codepoint)
{
  struct nk_glfw_win  *win = &glfw.w0;
  if(w == glfw.w1.win) win = &glfw.w1;
  if (win->text_len < NK_GLFW_TEXT_MAX)
    win->text[win->text_len++] = codepoint;
}

NK_API void nk_glfw3_scroll_callback(
    GLFWwindow *w, double xoff, double yoff)
{
  struct nk_glfw_win  *win = &glfw.w0;
  if(w == glfw.w1.win) win = &glfw.w1;
  win->scroll.x += (float)xoff;
  win->scroll.y += (float)yoff;
}

NK_API void
nk_glfw3_mouse_button_callback(
    GLFWwindow *w, int button,
    int action, int mods)
{
  struct nk_glfw_win  *win = &glfw.w0;
  if(w == glfw.w1.win) win = &glfw.w1;
  double x, y;
  NK_UNUSED(mods);
  if (button != GLFW_MOUSE_BUTTON_LEFT)
    return;
  glfwGetCursorPos(win->win, &x, &y);
  if (action == GLFW_PRESS) {
    double dt = glfwGetTime() - win->last_button_click;
    if (dt > NK_GLFW_DOUBLE_CLICK_LO && dt < NK_GLFW_DOUBLE_CLICK_HI) {
      win->is_double_click_down = nk_true;
      win->double_click_pos = nk_vec2((float)x, (float)y);
    }
    win->last_button_click = glfwGetTime();
  } else
    win->is_double_click_down = nk_false;
}

NK_INTERN void nk_glfw3_clipboard_paste(
    nk_handle usr,
    struct nk_text_edit *edit)
{
  GLFWwindow *w = (GLFWwindow*)usr.ptr;
  struct nk_glfw_win  *win = &glfw.w0;
  if(w == glfw.w1.win) win = &glfw.w1;
  const char *text = glfwGetClipboardString(win->win);
  if (text) nk_textedit_paste(edit, text, nk_strlen(text));
}

NK_INTERN void nk_glfw3_clipboard_copy(
    nk_handle usr, const char *text,
    int len)
{
  GLFWwindow *w = (GLFWwindow*)usr.ptr;
  struct nk_glfw_win  *win = &glfw.w0;
  if(w == glfw.w1.win) win = &glfw.w1;
  char *str = 0;
  if (!len) return;
  str = (char *)malloc((size_t)len + 1);
  if (!str) return;
  memcpy(str, text, (size_t)len);
  str[len] = '\0';
  glfwSetClipboardString(win->win, str);
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

  glfw.w1.is_double_click_down = nk_false;
  glfw.w1.double_click_pos = nk_vec2(0, 0);
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

  glfw.w0.is_double_click_down = nk_false;
  glfw.w0.double_click_pos = nk_vec2(0, 0);
}

#endif
